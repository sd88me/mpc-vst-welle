// Welle for MPC: the MPC Plugin SDK front end (framework/mpc_plugin.h) around the Wave Emulation engine (src/dsp/welle_engine.cpp,
// linked in as libwelle_all.a, driven through its string API in engine_api.h).
//
// Signal flow: MPC MIDI -> engine midi (the engine's own emulator thread runs the Wave OS) -> engine render (drains its ring of int16
// stereo, any frame count) -> float outputs.
//
// Parameters: 478, in vst/params.lock order (tools/gen_sdk_params.py -> params_gen.h), so every index is the same as in the wrapper build.
//   * value params: a change goes to the engine on the audio thread as its own set_param string.
//   * momentary params (PANIC, +FAV, the disk/performance tiles ...): 0 -> 1 fires the verb once, then the param drops back to 0.
//   * text params (disk name, the LCD's cells ...): the value is unused, the screen shows the engine's text (formatValue).
//   * Quick/Perf fader values, LCD cell styles and whatever the Wave OS changes itself are read back from the engine (syncFromEngine).
// Parameter 0 is "bank_index" (frozen order). The host nudges parameter 0 on every prepare: a change is applied only once it has
// stayed put for ~50 ms, so a nudge-and-return never switches disk.
// Favourites are VST programs: 0 = the current sound (no-op), 1..64 = favourite slots. They are applied in reset(), on the audio thread.
// State: the framework saves every parameter by id; the engine's own "state" string (disk by name, performance, edits) goes in the
// extra state and is applied last.

#include "mpc_plugin.h"
#include "mpc_resources.h"

#include "engine_api.h"
#include "params_gen.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <string>
#include <thread>
#include <vector>
#include <sys/stat.h>

namespace {

#ifndef WELLE_NAME
#define WELLE_NAME "Welle"
#endif
#ifndef WELLE_UID
#define WELLE_UID 'W', 'e', 'L', 'e'
#endif
#define WELLE_FOURCC(...) MPC_FOURCC(__VA_ARGS__)   // expands WELLE_UID before MPC_FOURCC counts its arguments

mpc::PluginInfo makeInfo()
{
    mpc::PluginInfo i;
    i.name = WELLE_NAME;
    i.vendor = "sd88me";
    i.uniqueId = WELLE_FOURCC(WELLE_UID);
    i.version = 1000;
    i.isInstrument = true;
    i.numInputs = 0;
    i.numOutputs = 2;
    return i;
}

bool isDir(const std::string& p)
{
    struct stat st;
    return stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

long long nowMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

// A tiny lock between the UI/worker threads (reading engine text) and the audio thread (changing the engine).
// The audio thread only ever try-locks: if the UI holds it, the changes wait for the next block.
struct SpinLock
{
    std::atomic_flag f = ATOMIC_FLAG_INIT;
    bool tryLock() { return !f.test_and_set(std::memory_order_acquire); }
    void lock() { while (f.test_and_set(std::memory_order_acquire)) {} }
    void unlock() { f.clear(std::memory_order_release); }
};

// Verbs and values after which the engine's parameters are read back (a performance load changes almost all of them).
bool changesSound(const char* key)
{
    return !strcmp(key, "preset") || !strcmp(key, "bank_index") || !strcmp(key, "preset_next") || !strcmp(key, "preset_prev") ||
           !strcmp(key, "bank_next") || !strcmp(key, "bank_prev") || !strcmp(key, "init_patch") || !strcmp(key, "fader_mode") ||
           !strncmp(key, "bank_slot_", 10) || !strncmp(key, "patch_slot_", 11) || !strcmp(key, "state") || !strcmp(key, "favourite");
}

} // namespace

class WellePlugin : public mpc::Plugin
{
public:
    WellePlugin() : mpc::Plugin(makeInfo())
    {
        for (int i = 0; i < kNumParams; ++i)
        {
            const PSpec& s = g_pspecs[i];
            if (s.flags & kChoice)
                addParam(mpc::choiceParam(s.key, s.name, std::vector<std::string>(s.choices, s.choices + s.nchoices), s.def));
            else
                addParam(mpc::intParam(s.key, s.name, s.min, s.max > s.min ? s.max : s.min + 1, s.def));
        }
        addProgram("Current sound", {});
        for (int i = 0; i < kFavSlots; ++i) addProgram("--", {});
        dirty_ = new std::atomic<bool>[(size_t)kNumParams];
        for (int i = 0; i < kNumParams; ++i) dirty_[i].store(false);
        notify_ = new std::atomic<bool>[(size_t)kNumParams];
        for (int i = 0; i < kNumParams; ++i) notify_[i].store(false);
        textCache_.assign((size_t)kNumParams, std::string("\x01"));

        // The engine's data folder (roms/, banks/, cache/, favourites.txt): <plugin folder>/VST/welle for installs of the wrapper build,
        // else <plugin folder>/Content. (Not mpc::contentPath(): a function-local static std::string, see docs/SDK-PORTING.md.)
        const std::string folder = mpc::detail::computePluginFolder();
        dataDir_ = isDir(folder + "/VST/welle") ? folder + "/VST/welle" : folder + "/Content";
        eng_ = mpc_engine();
        inst_ = eng_->create(dataDir_.c_str());
        refreshProgramNames();
        syncFromEngine();
        worker_ = std::thread(&WellePlugin::workerMain, this);
    }

    ~WellePlugin() override
    {
        quit_.store(true);
        if (worker_.joinable()) worker_.join();
        if (inst_) eng_->destroy(inst_);
        delete[] dirty_;
        delete[] notify_;
    }

    void prepare(double, int) override {}

    // Audio thread, after a state load (and on resume): the project's engine state, then a favourite picked from the PRESET menu.
    void reset() override
    {
        store(kUiParam, 0.0f);
        notify_[kUiParam].store(true);
        const int prog = currentProgram();
        if (prog != lastProgram_)
        {
            lastProgram_ = prog;
            if (prog > 0 && !ignoreProgram_ && inst_)
            {
                char v[16]; snprintf(v, sizeof v, "%d", prog - 1);
                lock_.lock(); eng_->set_param(inst_, "favourite", v); lock_.unlock();
                scheduleSync();
            }
        }
        ignoreProgram_ = false;
        if (!pendingState_.empty() && inst_)
        {
            lock_.lock();
            eng_->set_param(inst_, "state", pendingState_.c_str());
            lock_.unlock();
            pendingState_.clear();
            bankCountdown_ = -1;
            scheduleSync();
        }
    }

    void parameterChanged(int index, float plain) override
    {
        const PSpec& s = g_pspecs[index];
        if (!strcmp(s.key, "favourite")) return;                    // programs do this (reset())
        if (s.flags & kUi) return;                                  // which drawn pick-list is open: the skin only
        if ((s.flags & kChoice) && getInt(kUiParam) != 0)           // a pick closes the open list (the host is told from the worker)
        {
            store(kUiParam, 0.0f);
            notify_[kUiParam].store(true);
        }
        if (s.flags & kMomentary) { if (plain < 0.5f) return; }     // a verb that also shows text (a tile with a name): momentary wins
        else if (s.flags & kText) return;
        dirty_[index].store(true, std::memory_order_release);
    }

    bool formatValue(int index, float plain, std::string& out) override
    {
        const PSpec& s = g_pspecs[index];
        if (!inst_) return false;
        if (s.flags & kText)
        {
            out = engineGet(s.key);
            return true;
        }
        if (s.flags & kFloatDisp)   // a float parameter in the processor's own words (Hz, ms ...)
        {
            (void)plain;
            std::string r = engineGet((std::string(s.key) + "_display").c_str());
            if (r.empty()) return false;
            out = r;
            return true;
        }
        return false;
    }

    void saveExtraState(std::vector<uint8_t>& out) override
    {
        if (!inst_) return;
        std::vector<char> buf(256 * 1024);
        lock_.lock();
        const int n = eng_->get_param(inst_, "state", buf.data(), (int)buf.size());
        lock_.unlock();
        if (n > 0 && n < (int)buf.size()) out.assign(buf.data(), buf.data() + n);
    }

    void loadExtraState(const uint8_t* data, size_t size) override
    {
        pendingState_.assign(reinterpret_cast<const char*>(data), size);   // applied in reset(), on the audio thread
        ignoreProgram_ = true;
    }

    void process(mpc::ProcessContext& ctx) override
    {
        if (!inst_) return;
        applyParamChanges(ctx.numFrames);

        float* outL = ctx.outputs[0];
        float* outR = ctx.numOutputs > 1 ? ctx.outputs[1] : nullptr;
        int frame = 0, mi = 0;
        while (frame < ctx.numFrames)
        {
            while (mi < ctx.numMidiEvents && ctx.midi[mi].sampleOffset <= frame)
            {
                const mpc::MidiEvent& e = ctx.midi[mi++];
                eng_->midi(inst_, e.data, e.size);
            }
            int end = ctx.numFrames;
            if (mi < ctx.numMidiEvents) end = mpc::clamp(ctx.midi[mi].sampleOffset, frame + 1, ctx.numFrames);
            while (frame < end)
            {
                const int n = std::min(end - frame, kChunk);
                eng_->render(inst_, pcm_, n);
                for (int i = 0; i < n; ++i)
                {
                    outL[frame + i] = pcm_[2 * i] * (1.0f / 32768.0f);
                    if (outR) outR[frame + i] = pcm_[2 * i + 1] * (1.0f / 32768.0f);
                }
                frame += n;
            }
        }
    }

private:
    static constexpr int kChunk = 128;
    static constexpr int kFavSlots = 64;

    std::string engineGet(const char* key)
    {
        char buf[256] = "";
        lock_.lock();
        const int n = eng_->get_param(inst_, key, buf, sizeof buf);
        lock_.unlock();
        return n > 0 ? std::string(buf) : std::string();
    }

    // The PRESET menu: "<n> <favourite's name>" (the engine's "favourite:<i>" answers "-" for an empty slot).
    void refreshProgramNames()
    {
        for (int i = 0; i < kFavSlots; ++i)
        {
            const std::string nm = engineGet(("favourite:" + std::to_string(i)).c_str());
            programs_[(size_t)i + 1].name = std::to_string(i + 1) + " " + (nm.empty() || nm == "-" ? "--" : nm);
        }
    }

    void scheduleSync()   // the OS needs a moment to load a performance: read the values back twice
    {
        const long long t = nowMs();
        syncAt1_.store(t + 350);
        syncAt2_.store(t + 1200);
    }

    // Set a parameter's value without calling parameterChanged (so it is not written back to the engine).
    void store(int i, float plain) { values_[(size_t)i]->store(param(i).toNormalised(plain), std::memory_order_relaxed); }

    // Read the value params back from the engine without marking them dirty; a value the user has just changed is left alone.
    void syncFromEngine()
    {
        char buf[64];
        for (int i = 0; i < kNumParams; ++i)
        {
            const PSpec& s = g_pspecs[i];
            if ((s.flags & (kText | kMomentary | kUi)) || dirty_[i].load(std::memory_order_acquire)) continue;
            if (i == 0 && bankCountdown_ >= 0) continue;
            buf[0] = 0;
            lock_.lock();
            const int n = eng_->get_param(inst_, s.key, buf, sizeof buf);
            lock_.unlock();
            if (n <= 0 || !buf[0]) continue;
            store(i, (float)atoi(buf));
        }
    }

    // The LCD cells' background styles and the fader values follow the Wave OS: cheap, and polled when the engine says it changed.
    void syncLive()
    {
        char buf[64];
        for (int i = 0; i < kNumParams; ++i)
        {
            const PSpec& s = g_pspecs[i];
            if (strncmp(s.key, "lcdst_", 6) && strncmp(s.key, "fader_", 6)) continue;
            if ((s.flags & kText) || dirty_[i].load(std::memory_order_acquire)) continue;
            buf[0] = 0;
            lock_.lock();
            const int n = eng_->get_param(inst_, s.key, buf, sizeof buf);
            lock_.unlock();
            if (n > 0 && buf[0]) store(i, (float)atoi(buf));
        }
    }

    // Text params never change value, so the host never asks for their text again: flip the value when the text changes.
    void syncText()
    {
        for (int i = 0; i < kNumParams; ++i)
        {
            const PSpec& s = g_pspecs[i];
            if (!(s.flags & kText) || (s.flags & kMomentary)) continue;
            const std::string t = engineGet(s.key);
            if (t == textCache_[(size_t)i]) continue;
            textCache_[(size_t)i] = t;
            const int lo = s.min, hi = s.max > s.min ? s.max : s.min + 1;
            store(i, (float)(getInt(i) == lo ? hi : lo));
            notify_[i].store(true);
        }
    }

    void drainNotify()
    {
        for (int i = 0; i < kNumParams; ++i)
            if (notify_[i].exchange(false)) notifyHostParameterChanged(i);
    }

    void applyParamChanges(int frames)
    {
        bool any = bankCountdown_ >= 0;
        for (int i = 0; i < kNumParams && !any; ++i) any = dirty_[i].load(std::memory_order_relaxed);
        if (!any || !lock_.tryLock()) return;

        bool resync = false, favChanged = false, listChanged = false;
        char val[32];
        for (int i = 0; i < kNumParams; ++i)
        {
            if (!dirty_[i].exchange(false, std::memory_order_acq_rel)) continue;
            const PSpec& s = g_pspecs[i];
            if (i == 0)   // bank_index: debounce (see the header comment)
            {
                bankTarget_ = getInt(0);
                bankCountdown_ = (int)(0.05 * sampleRate() / std::max(1, frames)) + 1;
                continue;
            }
            if (s.flags & kMomentary)
            {
                eng_->set_param(inst_, s.key, "1");
                store(i, 0.0f);
                notify_[i].store(true);   // the host keeps showing "1" (and ignores the next press) unless it is told
                if (!strcmp(s.key, "fav_save") || !strcmp(s.key, "fav_delete")) favChanged = true;
            }
            else
            {
                snprintf(val, sizeof val, "%d", getInt(i));
                eng_->set_param(inst_, s.key, val);
            }
            if (changesSound(s.key)) resync = true;
            if (resync || !strncmp(s.key, "patch_page", 10) || !strncmp(s.key, "bank_page", 9)) listChanged = true;
        }
        if (bankCountdown_ >= 0 && --bankCountdown_ < 0)
        {
            if (getInt(0) == bankTarget_)
            {
                snprintf(val, sizeof val, "%d", bankTarget_);
                eng_->set_param(inst_, "bank_index", val);   // the engine ignores a re-pick of the current disk
                resync = true;
            }
        }
        lock_.unlock();
        if (favChanged) { favDirty_.store(true); }
        if (resync) scheduleSync();
        if (listChanged)   // tile names (momentary text params) are only re-read on a display update
        {
            const long long t = nowMs();
            displayAt1_.store(t + 600);
            displayAt2_.store(t + 1600);
        }
    }

    // Worker thread: everything that must not run on the audio thread (host calls, reading the engine back, favourite names).
    void workerMain()
    {
        uint32_t seenRev = 0;
        while (!quit_.load(std::memory_order_relaxed))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            const long long t = nowMs();
            bool did = false;
            for (std::atomic<long long>* a : {&syncAt1_, &syncAt2_})
            {
                const long long due = a->load();
                if (due && t >= due) { a->store(0); syncFromEngine(); did = true; }
            }
            for (std::atomic<long long>* a : {&displayAt1_, &displayAt2_})
            {
                const long long due = a->load();
                if (due && t >= due) { a->store(0); requestDisplayUpdate(); }
            }
            if (favDirty_.exchange(false))
            {
                refreshProgramNames();
                syncFromEngine();
                requestDisplayUpdate();   // new names in the PRESET menu
            }
            const std::string r = engineGet("display_rev");
            const uint32_t rev = (uint32_t)atoll(r.c_str());
            if (rev != seenRev)
            {
                seenRev = rev;
                syncLive();
#ifdef WELLE_REV_UPDATE
                if (t - lastUpdateMs_ > 250) { lastUpdateMs_ = t; requestDisplayUpdate(); }
#endif
            }
            (void)did;
            if (t - lastTextMs_ > 250) { lastTextMs_ = t; syncText(); }
            drainNotify();
        }
    }

    std::atomic<long long> displayAt1_{0}, displayAt2_{0};
    std::atomic<bool>* notify_ = nullptr;
    std::vector<std::string> textCache_;
    long long lastTextMs_ = 0;
    const mpc_engine_t* eng_ = nullptr;
    void* inst_ = nullptr;
    std::string dataDir_;
    std::thread worker_;
    std::atomic<bool> quit_{false}, favDirty_{false};
    std::atomic<long long> syncAt1_{0}, syncAt2_{0};
    long long lastUpdateMs_ = 0;
    std::atomic<bool>* dirty_ = nullptr;
    int bankCountdown_ = -1, bankTarget_ = 0;
    int lastProgram_ = 0;
    bool ignoreProgram_ = false;
    std::string pendingState_;
    SpinLock lock_;
    int16_t pcm_[kChunk * 2];
};

MPC_PLUGIN_ENTRY(WellePlugin)
