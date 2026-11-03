/* Welle for MPC: mo0kid's Wave Emulation (a model of the Waldorf Wave's voice circuits, driven by the instrument's own OS 1.700
 * running on emulated 68000s) as an mpc-vst-plugins engine (wrapper/engine.h). GPL-3.0.
 *
 * Wave Emulation's JUCE AudioProcessor (src/wave, vendored with small patches: src/VENDORED.md) runs headless: no editor, no JUCE
 * message loop. One emulator thread per instance, pinned to the fast cores, owns the processor: it feeds MIDI and parameter changes
 * in, renders 128-frame blocks into a ring of int16 stereo, and does the processor's "message thread" work (serviceAsyncUpdate).
 * render() only drains the ring.
 *
 * Parameters: every one of the processor's (APVTS) parameters, keyed by its ID (tools/gen_params.py -> welle_params.h). The
 * values the screen reads are cached here and refreshed from the processor, so a program change or the firmware's own edits show.
 * Sounds: the Wave keeps them on floppy disk. Each disk image (.img) or Wave setup file in <data_dir>/banks/ is a bank of 256
 * performances. A setup file is first written into a disk image under <data_dir>/cache/.
 * Firmware: <data_dir>/roms/ (w2sys.bin + wdv.sys, OS 1.700). None ships. */
#include "fast_cores.h"
#include "PluginProcessor.h"
#include "PanelWiring.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <sched.h>
#include <memory>
#include <mutex>
#include <string>
#include <strings.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <vector>

extern "C" {
#include "engine.h"
}

#include "welle_params.h"
#include "wave_lcd.h"
#include "UI/WaveLcdModel.h"

namespace {

constexpr int kRate = 44100, kBlock = 128;
constexpr int kRingFrames = 16384;      // power of two
constexpr int kTargetFill = 768;        // frames the emulator thread keeps queued (~17 ms)
constexpr int kBoostFill = 2048;        // ~46 ms, for 500 ms after a program or bank load (the firmware does its work then)
constexpr int kPrograms = 256;          // a Wave setup: two banks of 128 performances
constexpr int kFavSlots = 64;
// the MAIN faders in Quick Edit mode: the Wave's eight Quick Edit offsets, in its own order
const char *const kQuickKeys[8] = {"quickAttack", "quickDecay", "quickSustain", "quickRelease", "quickPitchMod", "quickTimbre", "quickTimbreMod", "quickWavescan"};
const char *const kQuickNames[8] = {"ATTACK", "DECAY", "SUSTAIN", "RELEASE", "PITCH MOD", "TIMBRE", "TIMBRE MOD", "WAVESCAN"};
const char *const kPerfNames[8] = {"FADER 1", "FADER 2", "FADER 3", "FADER 4", "FADER 5", "FADER 6", "FADER 7", "FADER 8"};

FILE *g_log = nullptr;
void xlog(const char *fmt, ...) {
    if (!g_log) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    va_end(ap);
    fputc('\n', g_log);
    fflush(g_log);
}

long long mono_ms() { return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

/* CPU ticks of this instance's threads (ours "WVemu" and Wave's voice-card workers it starts), for the STATS readout */
long thread_ticks(const std::vector<pid_t> &tids) {
    long total = 0;
    for (pid_t t : tids) {
        char path[64], line[512];
        snprintf(path, sizeof path, "/proc/self/task/%d/stat", (int)t);
        FILE *f = fopen(path, "r");
        if (!f) continue;
        if (fgets(line, sizeof line, f)) {
            const char *q = strrchr(line, ')');
            unsigned long ut = 0, st = 0;
            if (q && sscanf(q + 2, "%*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu", &ut, &st) == 2) total += (long)(ut + st);
        }
        fclose(f);
    }
    return total;
}

enum CmdType : uint8_t { C_MIDI, C_PARAM, C_PROGRAM, C_BANK, C_PANIC, C_VOICES, C_RESTORE };
struct Cmd {
    CmdType type = C_MIDI;
    uint8_t m[3] = {0, 0, 0};
    int a = 0, b = 0;
};

struct Bank { std::string name, file; bool setup = false; };

struct Inst {
    std::string dir;
    std::thread th;
    std::atomic<bool> stop{false}, ready{false}, failed{false};

    std::vector<int16_t> ring = std::vector<int16_t>(kRingFrames * 2);
    std::atomic<uint32_t> wr{0}, rd{0};
    std::atomic<uint32_t> underruns{0};
    std::atomic<int> emu_cpu{0}, active{0};
    std::atomic<int> midi_all{1};
    std::atomic<int> fader_quick{1};                 // MAIN faders: 1 Quick Edit offsets, 0 the Performance faders                    // 1: play every part of a multi-channel performance from one channel
    std::atomic<uint16_t> part_channels{1};          // the MIDI channels (bits 0-15) the current performance's instruments listen on
    std::atomic<long long> loading_until{0};       // a program / bank load in progress: its silence is not a dropout

    std::mutex qm;
    std::vector<Cmd> q;

    std::atomic<int> val[WV_NUM_PARAMS];             // the screen's values (see gen_params.py for the kinds)
    std::atomic<int> bank{0}, preset{0}, page{0}, gain{100}, bank_page{0}, voices{8};
    std::atomic<int> loaded_bank{-1};                // the bank whose disk is mounted (-1: none, the Wave's initial memory)
    std::atomic<uint32_t> rev{1};
    std::mutex sm;                                   // status, firmware name, program names
    char status[96] = "Starting...";
    char rom_name[96] = "";
    std::vector<std::string> names;                  // the mounted bank's program names
    std::vector<Bank> banks;
    std::vector<juce::RangedAudioParameter *> P;     // the processor's parameters (published before ready, for value texts)
    std::string restore;                             // parameter values from a saved project, applied after its program loads
    std::mutex lm;                                   // the LCD as text (wave_lcd.h), what the lcd_<line>_<column> parameters read
    wavelcd::Decoder::Screen lcd{};
    std::mutex fm;                                   // favourites: a name and a saved state each (favourites.txt)
    std::vector<std::pair<std::string, std::string>> favs;
    int fav_current = -1;
    uint32_t fav_stamp = 0;
    std::atomic<uint32_t> load_gen{0};               // bumped by every program / bank / state load: an edit keeps a favourite picked
    std::atomic<long long> fav_list_ms{-100000};
    std::mutex stm;
    char stats_cache[96] = "";
    int stats_cpu = -1;

    void bump() { rev.fetch_add(1, std::memory_order_relaxed); }
    void set_status(const char *s) {
        { std::lock_guard<std::mutex> l(sm); snprintf(status, sizeof status, "%s", s); }
        bump();
        xlog("[wv] %s", s);
    }
    void push(const Cmd &c) { std::lock_guard<std::mutex> l(qm); if (q.size() < 4096) q.push_back(c); }
};

std::unordered_map<std::string, int> &param_index() {
    static std::unordered_map<std::string, int> m = [] {
        std::unordered_map<std::string, int> r;
        for (int i = 0; i < WV_NUM_PARAMS; i++) r[g_wv_params[i].key] = i;
        return r;
    }();
    return m;
}

/* screen value <-> the parameter's normalised value */
float to_norm(juce::RangedAudioParameter *p, const wv_param_t &d, int v) {
    if (d.kind == 1) return clampi(v, 0, 127) / 127.0f;
    const int x = d.kind == 2 ? d.lo + v : v;
    return p->convertTo0to1((float)clampi(x, d.lo, d.hi));
}
int from_norm(juce::RangedAudioParameter *p, const wv_param_t &d, float n) {
    if (d.kind == 1) return clampi((int)std::lround(n * 127.0f), 0, 127);
    const int x = clampi((int)std::lround(p->convertFrom0to1(n)), d.lo, d.hi);
    return d.kind == 2 ? x - d.lo : x;
}

bool has_ext(const std::string &n, const char *ext) {
    const size_t e = strlen(ext);
    return n.size() > e && !strcasecmp(n.c_str() + n.size() - e, ext);
}

/* banks/: Wave disk images (.img) and Wave setup files (anything else the Wave Emulation set loader accepts: tried at load) */
void scan_banks(Inst *in) {
    std::vector<Bank> found;
    const std::string bd = in->dir + "/banks";
    if (DIR *d = opendir(bd.c_str())) {
        while (struct dirent *e = readdir(d)) {
            const std::string n = e->d_name;
            if (n[0] == '.' || has_ext(n, ".txt") || has_ext(n, ".md")) continue;
            Bank b;
            b.file = bd + "/" + n;
            b.setup = !has_ext(n, ".img");
            const size_t dot = n.rfind('.');
            b.name = dot == std::string::npos ? n : n.substr(0, dot);
            found.push_back(b);
        }
        closedir(d);
    }
    // alphabetical, with the factory set first
    auto factory = [](const Bank &b) { return strncasecmp(b.name.c_str(), "factory", 7) == 0 || strcasecmp(b.name.c_str(), "wave") == 0; };
    std::sort(found.begin(), found.end(), [&](const Bank &a, const Bank &b) {
        if (factory(a) != factory(b)) return factory(a);
        return strcasecmp(a.name.c_str(), b.name.c_str()) < 0;
    });
    in->banks = std::move(found);
}
int bank_count(Inst *in) { return std::max(1, (int)in->banks.size()); }
std::string bank_name(Inst *in, int b) {
    if (in->banks.empty()) return "INTERNAL";
    return b >= 0 && b < (int)in->banks.size() ? in->banks[b].name : "-";
}

void tune_thread() {
    pthread_setname_np(pthread_self(), "WVemu");
    // Gen2 big.LITTLE (Live III: A55 0-3, A76 4-7): the fast cores minus MPC's "Audio Processing" core. Wave's voice-card worker
    // threads, started later from this thread, inherit the mask.
    if (!getenv("WV_NOPIN")) fastcores::pin_this_thread();
}

std::vector<pid_t> all_threads() {
    std::vector<pid_t> r;
    if (DIR *d = opendir("/proc/self/task")) {
        while (struct dirent *e = readdir(d))
            if (e->d_name[0] >= '0' && e->d_name[0] <= '9') r.push_back((pid_t)atoi(e->d_name));
        closedir(d);
    }
    return r;
}

std::vector<pid_t> instance_threads(pid_t emu) {
    std::vector<pid_t> r{emu};
    DIR *d = opendir("/proc/self/task");
    if (!d) return r;
    while (struct dirent *e = readdir(d)) {
        if (e->d_name[0] < '0' || e->d_name[0] > '9') continue;
        char path[64], comm[32] = "";
        snprintf(path, sizeof path, "/proc/self/task/%s/comm", e->d_name);
        if (FILE *f = fopen(path, "r")) {
            if (!fgets(comm, sizeof comm, f)) comm[0] = 0;
            fclose(f);
        }
        if (!strncmp(comm, "Wave Voice Card", 15) || !strncmp(comm, "Wave voice", 10)) r.push_back((pid_t)atoi(e->d_name));
    }
    closedir(d);
    return r;
}

/* one fast core each for our thread and Wave's two voice-card workers (they render cards 2 and 3 while ours renders card 1 and
 * waits for them: threads that hand work to each other must not queue behind one another). WV_NOSPREAD=1 skips it. */
void spread_threads(const std::vector<pid_t> &t) {          // ours first, then the workers in creation order
    const auto fast = fastcores::usable();
    if (fast.size() < 3 || getenv("WV_NOSPREAD")) return;
    for (size_t i = 0; i < t.size() && i < 3; i++) {
        cpu_set_t set;
        CPU_ZERO(&set);
        CPU_SET(fast[i], &set);
        if (sched_setaffinity(t[i], sizeof set, &set) == 0) xlog("[wv] thread %d -> core %d", (int)t[i], fast[i]);
    }
}

std::vector<uint8_t> read_file(const std::string &path) {
    std::vector<uint8_t> d;
    if (FILE *f = fopen(path.c_str(), "rb")) {
        fseek(f, 0, SEEK_END);
        const long n = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (n > 0 && n < (8 << 20)) { d.resize((size_t)n); if (fread(d.data(), 1, d.size(), f) != d.size()) d.clear(); }
        fclose(f);
    }
    return d;
}

/* The LCD reader: a thread of its own (a decode takes milliseconds, more than a block's budget), on whatever core is free. It
 * reads the firmware's video RAM whenever the firmware has drawn something, at most 8 times a second, and turns it into text. */
void lcd_main(Inst *in, const WaveEmulationAudioProcessor *proc, std::atomic<bool> *quit) {
    pthread_setname_np(pthread_self(), "WVlcd");
    {   // off the fast cores (the emulator's), and below everyone else
        const auto fast = fastcores::usable();
        if (!fast.empty()) {
            cpu_set_t set;
            CPU_ZERO(&set);
            const int n = (int)sysconf(_SC_NPROCESSORS_ONLN);
            for (int c = 0; c < n; c++)
                if (std::find(fast.begin(), fast.end(), c) == fast.end() && c != n - 1) CPU_SET(c, &set);
            if (CPU_COUNT(&set)) sched_setaffinity(0, sizeof set, &set);
        }
        setpriority(PRIO_PROCESS, (id_t)syscall(SYS_gettid), 10);
    }
    wavelcd::Decoder dec;
    if (!dec.load(read_file(in->dir + "/roms/w2sys.bin"))) { xlog("[wv] LCD fonts not found in roms/w2sys.bin"); return; }
    const auto &rt = proc->getMasterFirmwareRuntime();
    uint64_t seen_rev = ~0ull, seen_writes = ~0ull;
    uint8_t seen_page = 0xff;
    std::vector<uint8_t> px(wavelcd::W * wavelcd::H);
    while (!quit->load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
        const uint64_t rev = rt.lcdDisplayRevision(), writes = rt.lcdVideoWriteCount();
        const uint8_t page = rt.lcdDisplayPage();
        if (rev == seen_rev && writes == seen_writes && page == seen_page) continue;
        seen_rev = rev; seen_writes = writes; seen_page = page;
        const auto video = rt.lcdVideoSnapshot();
        wave::ui::LcdFramebuffer fb;
        fb.loadHardwareVideoRam(video.data(), video.size(), page);
        for (int y = 0; y < wavelcd::H; y++)
            for (int x = 0; x < wavelcd::W; x++) px[(size_t)y * wavelcd::W + x] = fb.pixel(x, y) ? 1 : 0;
        auto screen = dec.decode_screen(px.data());
        bool changed = false;
        {
            std::lock_guard<std::mutex> l(in->lm);
            if (screen != in->lcd) { in->lcd = screen; changed = true; }
        }
        if (changed) in->bump();
    }
}

void emu_main(Inst *in) {
    tune_thread();
    const pid_t my_tid = (pid_t)syscall(SYS_gettid);
    in->set_status("Booting the Wave...");
    const auto t0 = std::chrono::steady_clock::now();
    std::unique_ptr<WaveEmulationAudioProcessor> proc;
    const std::vector<pid_t> before = all_threads();
    std::vector<pid_t> mine{my_tid};
    try {
        proc = std::make_unique<WaveEmulationAudioProcessor>(juce::File{}, WaveEmulationAudioProcessor::InitialBank::initialised);
        proc->setPlayConfigDetails(0, 2, kRate, kBlock);
        proc->prepareToPlay(kRate, kBlock);
        proc->setVoiceLimit(in->voices.load());
        if (getenv("WV_SERIAL")) proc->setVoiceCardThreading(false);
        if (getenv("WV_VOICES")) { in->voices = atoi(getenv("WV_VOICES")); proc->setVoiceLimit(in->voices); }
        for (pid_t t : all_threads())
            if (std::find(before.begin(), before.end(), t) == before.end()) mine.push_back(t);
        std::sort(mine.begin() + 1, mine.end());
        const auto report = proc->loadFirmware(juce::File(juce::String(in->dir + "/roms")), false);
        (void)report;
        if (!proc->getMasterFirmwareRuntime().isLoaded()) {
            in->set_status("No Wave OS 1.700 in welle/roms/ (w2sys.bin + wdv.sys)");
            in->failed = true;
        } else {
            std::lock_guard<std::mutex> l(in->sm);
            snprintf(in->rom_name, sizeof in->rom_name, "Wave OS 1.700");
        }
    } catch (const std::exception &e) {
        char b[96];
        snprintf(b, sizeof b, "Wave start failed: %.60s", e.what());
        in->set_status(b);
        in->failed = true;
        return;
    }
    xlog("[wv] processor up in %.1f s", std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count());

    auto &plist = proc->getParameters();
    std::vector<juce::RangedAudioParameter *> P(WV_NUM_PARAMS, nullptr);
    for (int i = 0; i < WV_NUM_PARAMS; i++) {
        P[i] = proc->parameters.getParameter(g_wv_params[i].key);
        if (!P[i]) xlog("[wv] missing parameter %s", g_wv_params[i].key);
    }
    (void)plist;
    std::vector<int> pending(WV_NUM_PARAMS, INT32_MIN);   // the latest screen value per parameter, not yet sent
    std::vector<int> fader_of(WV_NUM_PARAMS, -1);         // parameter -> performance fader 0-7
    for (int i = 0; i < WV_NUM_PARAMS; i++)
        if (!strncmp(g_wv_params[i].key, "performanceFader", 16)) fader_of[i] = atoi(g_wv_params[i].key + 16) - 1;
    bool fader_held[8] = {};
    long long fader_ms[8] = {};

    auto refresh_values = [&](bool force) {               // the processor's values -> the screen's
        bool changed = false;
        for (int i = 0; i < WV_NUM_PARAMS; i++) {
            if (!P[i] || pending[i] != INT32_MIN) continue;
            const int v = from_norm(P[i], g_wv_params[i], P[i]->getValue());
            if (force || in->val[i].load() != v) { changed |= in->val[i].exchange(v) != v; }
        }
        if (changed) in->bump();
    };
    auto refresh_names = [&] {
        std::vector<std::string> nm;
        const int np = proc->getNumPrograms();
        for (int i = 0; i < np; i++) nm.push_back(proc->getProgramName(i).toStdString());
        { std::lock_guard<std::mutex> l(in->sm); in->names.swap(nm); }
        in->bump();
    };

    auto mount_bank = [&](int b) -> bool {
        if (b < 0 || b >= (int)in->banks.size()) return false;
        const Bank &bk = in->banks[b];
        juce::File image(juce::String(bk.file));
        if (bk.setup) {                                    // a setup file: put it on a blank disk first
            juce::File cache(juce::String(in->dir + "/cache"));
            cache.createDirectory();
            juce::File dst = cache.getChildFile(juce::String(bk.name + ".img"));
            if (!dst.existsAsFile()) {
                const auto r = proc->createDiskImageFromWaveSetup(image, dst);
                if (r.failed()) { in->set_status(("Not a Wave setup: " + bk.name).substr(0, 90).c_str()); return false; }
            }
            image = dst;
        }
        auto r = proc->mountDiskImage(image);
        if (r.wasOk()) r = proc->reloadBankFromMountedImage();
        if (r.failed()) {
            in->set_status(("Bank load failed: " + r.getErrorMessage().toStdString()).substr(0, 90).c_str());
            return false;
        }
        in->loaded_bank = b;
        refresh_names();
        return true;
    };

    std::vector<float> L(kBlock), R(kBlock);
    juce::AudioBuffer<float> buf(2, kBlock);
    juce::MidiBuffer midi;
    long long boost_until = 0, last_names = 0;
    uint32_t block_no = 0;
    long last_ticks = 0;
    auto last_t = std::chrono::steady_clock::now();
    std::vector<pid_t> tids{my_tid};

    auto apply_restore = [&] {
        std::string s;
        { std::lock_guard<std::mutex> l(in->sm); s.swap(in->restore); }
        if (s.empty()) return;
        // "<index>=<value>;" pairs, the parameter's screen value
        const char *p = s.c_str();
        while (*p) {
            char *e = nullptr;
            const long i = strtol(p, &e, 10);
            if (e == p || *e != '=') break;
            const long v = strtol(e + 1, &e, 10);
            if (i >= 0 && i < WV_NUM_PARAMS && P[i]) P[i]->setValueNotifyingHost(to_norm(P[i], g_wv_params[i], (int)v));
            p = *e == ';' ? e + 1 : e;
        }
        refresh_values(true);
    };

    auto produce = [&]() {
        std::vector<Cmd> cmds;
        { std::lock_guard<std::mutex> l(in->qm); cmds.swap(in->q); }
        midi.clear();
        for (const auto &c : cmds) {
            switch (c.type) {
            case C_MIDI: {
                const uint16_t chans = in->midi_all.load() ? in->part_channels.load() : 0;
                if (!chans || c.m[0] < 0x80 || c.m[0] >= 0xf0) { midi.addEvent(c.m, c.b, 0); break; }
                for (int ch = 0; ch < 16; ch++)             // the same message on each part's channel
                    if (chans >> ch & 1) { uint8_t m2[3] = {(uint8_t)((c.m[0] & 0xf0) | ch), c.m[1], c.m[2]}; midi.addEvent(m2, c.b, 0); }
                break;
            }
            case C_PARAM:
                if (c.a >= 0 && c.a < WV_NUM_PARAMS) pending[c.a] = c.b;
                break;
            case C_BANK:
                in->loading_until = mono_ms() + 3000;
                if (c.a != in->loaded_bank.load()) mount_bank(c.a);
                in->loading_until = mono_ms() + 300;
                boost_until = mono_ms() + 500;
                break;
            case C_PROGRAM:
                in->loading_until = mono_ms() + 3000;
                if (c.a < proc->getNumPrograms()) proc->setCurrentProgram(c.a);
                in->loading_until = mono_ms() + 1000;    // the firmware loads the performance over the next blocks
                for (auto &v : pending) v = INT32_MIN;     // the program's own values win over edits made before it
                boost_until = mono_ms() + 500;
                break;
            case C_RESTORE:
                apply_restore();
                break;
            case C_PANIC:
                proc->allSoundOffFromUi();
                for (int ch = 1; ch <= 16; ch++) {
                    midi.addEvent(juce::MidiMessage::allNotesOff(ch), 0);
                    midi.addEvent(juce::MidiMessage::allSoundOff(ch), 0);
                }
                break;
            case C_VOICES:
                proc->setVoiceLimit(c.a);
                break;
            }
        }
        for (int i = 0; i < WV_NUM_PARAMS; i++) {
            if (pending[i] == INT32_MIN) continue;
            const int f = fader_of[i];
            if (f >= 0) {                                 // a performance fader: moved as the panel's, so the firmware sees it
                if (!fader_held[f]) { proc->beginPanelFaderGesture(f, true); fader_held[f] = true; }
                proc->setPanelFader(f, wave::panel::performanceFaderAdcChannels[(size_t)f], to_norm(P[i], g_wv_params[i], pending[i]), true);
                fader_ms[f] = mono_ms();
            } else if (P[i]) P[i]->setValueNotifyingHost(to_norm(P[i], g_wv_params[i], pending[i]));
            pending[i] = INT32_MIN;
        }
        for (int f = 0; f < 8; f++)                       // a gesture ends when the fader has been still for a moment
            if (fader_held[f] && mono_ms() - fader_ms[f] > 400) { proc->endPanelFaderGesture(f, true); fader_held[f] = false; }
        buf.clear();
        proc->processBlock(buf, midi);
        proc->serviceAsyncUpdate();
        in->active.store(proc->getActiveVoiceCount(), std::memory_order_relaxed);
        block_no++;
        if ((block_no & 7) == 0) refresh_values(false);    // ~23 ms
        if ((block_no & 31) == 4) {                        // the channels the performance's instruments listen on (0 = the base channel 1)
            const auto perf = proc->getCurrentPerformanceSnapshot();
            uint16_t m = 0;
            for (const auto &layer : perf.layers)
                if (layer.enabled) m |= (uint16_t)(1u << std::max(0, std::min(15, layer.midiChannel - 1)));
            in->part_channels = m ? m : 1;
        }
        if (mono_ms() - last_names > 2000) { last_names = mono_ms(); if (in->loaded_bank >= 0 || in->names.empty()) refresh_names(); }

        const float g = in->gain.load() / 100.0f * 32767.0f;
        const float *l = buf.getReadPointer(0), *r = buf.getReadPointer(1);
        uint32_t w = in->wr.load(std::memory_order_relaxed);
        for (int i = 0; i < kBlock; i++) {
            in->ring[(w & (kRingFrames - 1)) * 2] = (int16_t)std::max(-32768.0f, std::min(32767.0f, l[i] * g));
            in->ring[(w & (kRingFrames - 1)) * 2 + 1] = (int16_t)std::max(-32768.0f, std::min(32767.0f, r[i] * g));
            w++;
        }
        in->wr.store(w, std::memory_order_release);
    };
    if (!in->failed) {
        in->set_status("Loading sounds...");
        if (!in->banks.empty()) mount_bank(clampi(in->bank.load(), 0, (int)in->banks.size() - 1));
        else refresh_names();
        proc->setCurrentProgram(clampi(in->preset.load(), 0, kPrograms - 1));
    }
    refresh_values(true);
    for (int i = 0; i < kTargetFill / kBlock + 2 && !in->stop; i++) produce();
    {   // a saved project's values, if any, now that its program is in
        std::lock_guard<std::mutex> l(in->qm);
        in->q.push_back(Cmd{C_RESTORE});
    }
    tids = mine;
    spread_threads(mine);
    std::atomic<bool> lcd_quit{false};
    std::thread lcd_th;
    if (!in->failed) lcd_th = std::thread(lcd_main, in, proc.get(), &lcd_quit);
    in->P = P;
    in->ready = true;
    if (!in->failed) in->set_status(in->banks.empty() ? "Ready - no sound disks in welle/banks/" : "Ready");
    xlog("[wv] ready after %.1f s", std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count());

    static const bool bench = getenv("WV_BENCH") != nullptr;
    long long rep_ms = mono_ms();
    long slow_us = 0;
    int slow_blocks = 0;
    uint32_t rep_drops = in->underruns.load();
    while (!in->stop) {
        const uint32_t fill = in->wr.load(std::memory_order_relaxed) - in->rd.load(std::memory_order_acquire);
        if (bench) in->rd.store(in->wr.load());
        const uint32_t target = mono_ms() < boost_until ? (uint32_t)kBoostFill : (uint32_t)kTargetFill;
        if (!bench && (fill >= target || fill + kBlock > (uint32_t)kRingFrames)) {
            std::this_thread::sleep_for(std::chrono::microseconds(500));
            continue;
        }
        const auto c0 = std::chrono::steady_clock::now();
        produce();
        const long us = (long)std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - c0).count();
        if (us > slow_us) slow_us = us;
        if (us > 2900) slow_blocks++;                          // a 128-frame block is 2902 us of audio
        if (mono_ms() - rep_ms >= 5000) {                      // one line per 5 s into the log / journal when dropouts rise
            const uint32_t d = in->underruns.load();
            if (d != rep_drops || getenv("WV_RINGLOG"))
                xlog("[wv] dropouts %u (+%u), slowest block %ld us, %d blocks over real time, cpu %d%%, voices %d",
                     d, d - rep_drops, slow_us, slow_blocks, in->emu_cpu.load(), in->active.load());
            rep_drops = d; slow_us = 0; slow_blocks = 0; rep_ms = mono_ms();
        }
        if ((block_no % 690) == 0) {                       // ~2 s
            const auto now = std::chrono::steady_clock::now();
            const long tk = thread_ticks(tids);
            const double dt = std::chrono::duration<double>(now - last_t).count();
            if (dt > 0.1 && last_ticks) in->emu_cpu = (int)((tk - last_ticks) / 100.0 / dt * 100.0);
            if (bench) xlog("[bench] %.2fx real time, cpu %d%%", 690.0 * kBlock / kRate / dt, in->emu_cpu.load());
            last_ticks = tk;
            last_t = now;
        }
    }
    xlog("[wv] stopping");
    lcd_quit = true;
    if (lcd_th.joinable()) lcd_th.join();
    proc->releaseResources();
    proc.reset();
}

void fav_load(Inst *in);

void *eng_create(const char *dir) {
    if (!g_log) g_log = fopen(getenv("WV_LOG") ? getenv("WV_LOG") : "/tmp/welle.log", "a");
    Inst *in = new Inst();
    in->dir = dir ? dir : "";
    for (int i = 0; i < WV_NUM_PARAMS; i++) {
        const auto &d = g_wv_params[i];
        in->val[i].store(d.kind == 2 ? d.def - d.lo : d.def);
    }
    scan_banks(in);
    fav_load(in);
    in->th = std::thread(emu_main, in);
    return in;
}

void eng_destroy(void *p) {
    Inst *in = (Inst *)p;
    if (!in) return;
    in->stop = true;
    if (in->th.joinable()) in->th.join();
    delete in;
}

int num_programs(Inst *in) {
    std::lock_guard<std::mutex> l(in->sm);
    return std::max(1, (int)in->names.size());
}
std::string program_name(Inst *in, int i) {
    std::lock_guard<std::mutex> l(in->sm);
    return i >= 0 && i < (int)in->names.size() ? in->names[i] : std::string();
}

void select_program(Inst *in, int prog) {
    prog = clampi(prog, 0, std::max(0, num_programs(in) - 1));
    in->preset = prog;
    in->page = prog / WV_PATCH_TILES;
    in->push(Cmd{C_PROGRAM, {0, 0, 0}, prog, 0});
    in->load_gen++;
    in->bump();
}
void select_bank(Inst *in, int bank, int prog) {
    bank = clampi(bank, 0, bank_count(in) - 1);
    in->bank = bank;
    in->bank_page = bank / WV_BANK_TILES;
    if (!in->banks.empty()) in->push(Cmd{C_BANK, {0, 0, 0}, bank, 0});
    select_program(in, prog);
}

void eng_midi(void *p, const uint8_t *msg, int len) {
    Inst *in = (Inst *)p;
    if (!in || len < 1 || len > 3 || msg[0] < 0x80 || msg[0] >= 0xf0) return;
    const uint8_t st = msg[0] & 0xf0;
    if (st == 0xc0 && len >= 2) { select_program(in, msg[1] + (in->preset.load() >= 128 ? 128 : 0)); return; }
    if (st == 0xb0 && len >= 3 && msg[1] == 0) {          // bank select 0/1: the setup's two banks of 128
        if (msg[2] < 2) in->preset = msg[2] * 128 + in->preset.load() % 128;
        return;
    }
    Cmd c;
    c.type = C_MIDI;
    c.m[0] = (uint8_t)(st | 0);                           // the Wave listens on channel 1
    c.m[1] = len > 1 ? msg[1] : 0;
    c.m[2] = len > 2 ? msg[2] : 0;
    c.b = st == 0xc0 || st == 0xd0 ? 2 : 3;
    in->push(c);
}

int slot_key(const char *key, const char *prefix, int max, bool *on) {
    const size_t n = strlen(prefix);
    if (strncmp(key, prefix, n) != 0) return 0;
    char *end = nullptr;
    const long v = strtol(key + n, &end, 10);
    if (end == key + n || v < 1 || v > max) return 0;
    if (*end == 0) { *on = false; return (int)v; }
    if (strcmp(end, "_on") == 0) { *on = true; return (int)v; }
    return 0;
}

int pages(Inst *in) { return std::max(1, (num_programs(in) + WV_PATCH_TILES - 1) / WV_PATCH_TILES); }

void restore_state(Inst *in, const char *s) {
    auto num = [&](const char *k, int def) {
        const char *q = strstr(s, k);
        return q ? atoi(q + strlen(k)) : def;
    };
    in->gain = clampi(num("\"gain\":", 100), 10, 400);
    in->midi_all = num("\"midi_all\":", 1) ? 1 : 0;
    in->fader_quick = num("\"fader_quick\":", 1) ? 1 : 0;
    const int nv = num("\"voices\":", 8), v = nv >= 12 ? 12 : nv >= 8 ? 8 : 6;
    in->voices = v;
    in->push(Cmd{C_VOICES, {0, 0, 0}, v, 0});
    // the bank by name (the banks/ folder may have changed since), else by number
    int b = clampi(num("\"bank\":", 0), 0, bank_count(in) - 1);
    if (const char *q = strstr(s, "\"bank_name\":\"")) {
        q += 13;
        const char *e = strchr(q, '"');
        if (e)
            for (size_t i = 0; i < in->banks.size(); i++)
                if (in->banks[i].name == std::string(q, e)) b = (int)i;
    }
    if (const char *q = strstr(s, "\"values\":\"")) {
        q += 10;
        const char *e = strchr(q, '"');
        std::lock_guard<std::mutex> l(in->sm);
        in->restore.assign(q, e ? e : q + strlen(q));
    }
    select_bank(in, b, clampi(num("\"preset\":", 0), 0, kPrograms - 1));
    in->push(Cmd{C_RESTORE});
}

int put(char *buf, int n, const char *fmt, ...);

std::string state_string(Inst *in) {
    std::string s;
    char b[256];
    snprintf(b, sizeof b, "{\"state_version\":1,\"bank\":%d,\"bank_name\":\"%s\",\"preset\":%d,\"gain\":%d,\"voices\":%d,\"midi_all\":%d,\"fader_quick\":%d,\"values\":\"",
             in->bank.load(), bank_name(in, in->bank).c_str(), in->preset.load(), in->gain.load(), in->voices.load(), in->midi_all.load(),
             in->fader_quick.load());
    s = b;
    for (int i = 0; i < WV_NUM_PARAMS; i++) { snprintf(b, sizeof b, "%d=%d;", i, in->val[i].load()); s += b; }
    s += "\"}";
    return s;
}

/* ------------------------------------------------------------------ favourites: <data_dir>/favourites.txt, "name<TAB>state" a line */
std::string fav_path(Inst *in) { return in->dir + "/favourites.txt"; }

void fav_load(Inst *in) {
    std::lock_guard<std::mutex> l(in->fm);
    in->favs.clear();
    FILE *f = fopen(fav_path(in).c_str(), "rb");
    if (!f) return;
    static char line[16384];
    while ((int)in->favs.size() < kFavSlots && fgets(line, sizeof line, f)) {
        char *tab = strchr(line, '\t');
        if (!tab) continue;
        *tab = 0;
        std::string st(tab + 1);
        while (!st.empty() && (st.back() == '\n' || st.back() == '\r')) st.pop_back();
        in->favs.emplace_back(line, st);
    }
    fclose(f);
}

bool fav_write(Inst *in) {            // under fm
    const std::string path = fav_path(in), tmp = path + ".tmp";
    FILE *f = fopen(tmp.c_str(), "wb");
    if (!f) return false;
    bool ok = true;
    for (auto &fv : in->favs) ok = ok && fprintf(f, "%s\t%s\n", fv.first.c_str(), fv.second.c_str()) > 0;
    ok = (fclose(f) == 0) && ok;
    return ok && rename(tmp.c_str(), path.c_str()) == 0;
}

std::string prog_label(Inst *in, int i);
void restore_state(Inst *in, const char *s);

void fav_save(Inst *in) {
    const std::string st = state_string(in);
    std::string nm = program_name(in, in->preset);
    while (!nm.empty() && nm.back() == ' ') nm.pop_back();
    std::lock_guard<std::mutex> l(in->fm);
    if ((int)in->favs.size() >= kFavSlots) return;
    for (auto &fv : in->favs) if (fv.second == st) return;
    // the same name twice: number it, so the PRESET menu tells them apart
    std::string name = nm.empty() ? "Favourite" : nm;
    int dup = 1;
    for (auto &fv : in->favs) if (fv.first == name || fv.first.rfind(name + " ", 0) == 0) dup++;
    if (dup > 1) name += " " + std::to_string(dup);
    in->favs.emplace_back(name, st);
    if (!fav_write(in)) { in->favs.pop_back(); return; }
    in->fav_current = (int)in->favs.size() - 1;
    in->fav_stamp = in->load_gen;
}

void fav_delete(Inst *in) {           // only the favourite that is the current sound
    std::lock_guard<std::mutex> l(in->fm);
    const int d = in->fav_current;
    if (d < 0 || d >= (int)in->favs.size() || in->fav_stamp != in->load_gen) return;
    in->favs.erase(in->favs.begin() + d);
    fav_write(in);
    in->fav_current = -1;
}

void fav_pick(Inst *in, int idx) {
    std::string st;
    {
        std::lock_guard<std::mutex> l(in->fm);
        if (idx < 0 || idx >= kFavSlots) return;
        if (idx < (int)in->favs.size()) st = in->favs[idx].second;
    }
    if (!st.empty()) restore_state(in, st.c_str());
    std::lock_guard<std::mutex> l(in->fm);
    in->fav_current = idx;            // an empty slot shows as picked, nothing loads
    in->fav_stamp = in->load_gen;
    in->bump();
}

void eng_set(void *p, const char *key, const char *val) {
    Inst *in = (Inst *)p;
    if (!in || !key || !val) return;
    bool on = false;
    int n;
    const bool press = atoi(val) != 0;
    if ((n = slot_key(key, "bank_slot_", WV_BANK_TILES, &on)) && !on) {
        const int b = in->bank_page * WV_BANK_TILES + n - 1;
        if (press && b < bank_count(in)) select_bank(in, b, 0);
        return;
    }
    if (!strcmp(key, "bank_page_prev")) { if (press && in->bank_page > 0) { in->bank_page--; in->bump(); } return; }
    if (!strcmp(key, "bank_page_next")) { if (press && (in->bank_page + 1) * WV_BANK_TILES < bank_count(in)) { in->bank_page++; in->bump(); } return; }
    if ((n = slot_key(key, "patch_slot_", WV_PATCH_TILES, &on)) && !on) {
        if (press && in->page * WV_PATCH_TILES + n - 1 < num_programs(in)) select_program(in, in->page * WV_PATCH_TILES + n - 1);
        return;
    }
    if (!strcmp(key, "patch_page_prev")) { if (press && in->page > 0) { in->page--; in->bump(); } return; }
    if (!strcmp(key, "patch_page_next")) { if (press && in->page + 1 < pages(in)) { in->page++; in->bump(); } return; }
    if (!strcmp(key, "preset")) { select_program(in, atoi(val)); return; }
    if (!strcmp(key, "bank_index")) { if (atoi(val) != in->bank) select_bank(in, atoi(val), 0); return; }
    if (!strcmp(key, "preset_next") || !strcmp(key, "preset_prev")) {
        const int np = num_programs(in);
        if (press) select_program(in, (in->preset + (!strcmp(key, "preset_next") ? 1 : np - 1)) % np);
        return;
    }
    if (!strcmp(key, "bank_next") || !strcmp(key, "bank_prev")) {
        const int nb = bank_count(in);
        if (press) select_bank(in, (in->bank + (!strcmp(key, "bank_next") ? 1 : nb - 1)) % nb, 0);
        return;
    }
    if (!strcmp(key, "all_notes_off")) { if (press) in->push(Cmd{C_PANIC}); return; }
    if (!strcmp(key, "init_patch")) { if (press) select_program(in, in->preset); return; }   // the stored performance again
    if (!strcmp(key, "gain")) { in->gain = clampi(atoi(val), 10, 400); return; }
    if (!strcmp(key, "midi_mode")) { in->midi_all = atoi(val) == 0 ? 1 : 0; return; }
    if (!strcmp(key, "fader_mode")) { in->fader_quick = atoi(val) == 0 ? 1 : 0; in->bump(); return; }
    if (!strncmp(key, "fader_", 6) && key[6] >= '1' && key[6] <= '8' && !key[7]) {      // one of the MAIN faders: route by mode
        char k[40];
        if (in->fader_quick) snprintf(k, sizeof k, "%s", kQuickKeys[key[6] - '1']);
        else snprintf(k, sizeof k, "performanceFader%d", key[6] - '0');
        auto f = param_index().find(k);
        if (f != param_index().end()) { const int v = clampi(atoi(val), 0, 127); in->val[f->second] = v; in->push(Cmd{C_PARAM, {0, 0, 0}, f->second, v}); }
        return;
    }
    if (!strcmp(key, "voices")) {
        static const int kVoices[3] = {6, 8, 12};             // option index -> voices
        const int v = getenv("WV_VOICES") ? atoi(getenv("WV_VOICES")) : kVoices[clampi(atoi(val), 0, 2)];
        in->voices = v;
        in->push(Cmd{C_VOICES, {0, 0, 0}, v, 0});
        return;
    }
    if (!strcmp(key, "state")) { restore_state(in, val); in->load_gen++; return; }
    if (!strcmp(key, "fav_save")) { if (press) { fav_save(in); in->bump(); } return; }
    if (!strcmp(key, "fav_delete")) { if (press) { fav_delete(in); in->bump(); } return; }
    if (!strcmp(key, "favourite")) { fav_pick(in, atoi(val)); return; }
    auto it = param_index().find(key);
    if (it == param_index().end()) return;
    const wv_param_t &d = g_wv_params[it->second];
    const int lo = d.kind == 2 ? 0 : d.lo, hi = d.kind == 2 ? d.hi - d.lo : d.hi;
    const int v = clampi(atoi(val), lo, hi);
    in->val[it->second] = v;
    in->push(Cmd{C_PARAM, {0, 0, 0}, it->second, v});
}

int put(char *buf, int n, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    const int r = vsnprintf(buf, n, fmt, ap);
    va_end(ap);
    return r < 0 ? 0 : std::min(r, n - 1);
}

std::string prog_label(Inst *in, int i) {
    std::string nm;                                   // the name with its padding runs closed up ("Land of Oz    NT")
    for (char c : program_name(in, i)) if (c != ' ' || (!nm.empty() && nm.back() != ' ')) nm.push_back(c);
    while (!nm.empty() && nm.back() == ' ') nm.pop_back();
    char b[64];
    snprintf(b, sizeof b, "%c%03d %s", i < 128 ? 'A' : 'B', i % 128 + 1, nm.c_str());
    return b;
}

int eng_get(void *p, const char *key, char *buf, int n) {
    Inst *in = (Inst *)p;
    if (!in || !key || n < 2) return -1;
    bool on = false;
    int k;
    if ((k = slot_key(key, "bank_slot_", WV_BANK_TILES, &on))) {
        const int b = in->bank_page * WV_BANK_TILES + k - 1;
        if (on) return put(buf, n, "%d", b == in->bank ? 1 : 0);
        return put(buf, n, "%s", b < bank_count(in) ? bank_name(in, b).c_str() : "");
    }
    if (!strcmp(key, "bank_page_text")) return put(buf, n, "%d / %d", in->bank_page + 1, (bank_count(in) + WV_BANK_TILES - 1) / WV_BANK_TILES);
    if ((k = slot_key(key, "patch_slot_", WV_PATCH_TILES, &on))) {
        const int idx = in->page * WV_PATCH_TILES + k - 1;
        if (on) return put(buf, n, "%d", idx == in->preset ? 1 : 0);
        if (idx >= num_programs(in)) return put(buf, n, "%s", "");
        return put(buf, n, "%s", prog_label(in, idx).c_str());
    }
    if (!strcmp(key, "patch_page_text")) return put(buf, n, "PAGE %d / %d", in->page + 1, pages(in));
    if (!strcmp(key, "preset")) return put(buf, n, "%d", in->preset.load());
    if (!strncmp(key, "preset:", 7)) {
        const int i = atoi(key + 7);
        return i >= 0 && i < num_programs(in) ? put(buf, n, "%s", prog_label(in, i).c_str()) : put(buf, n, "-");
    }
    if (!strcmp(key, "bank_index")) return put(buf, n, "%d", in->bank.load());
    if (!strcmp(key, "bank_name")) return put(buf, n, "%s", bank_name(in, in->bank).c_str());
    if (!strcmp(key, "preset_name")) return put(buf, n, "%s", prog_label(in, in->preset).c_str());
    if (!strcmp(key, "rom_name")) { std::lock_guard<std::mutex> l(in->sm); return put(buf, n, "%s", in->rom_name[0] ? in->rom_name : "-"); }
    if (!strcmp(key, "loading_status")) { std::lock_guard<std::mutex> l(in->sm); return put(buf, n, "%s", in->status); }
    if (!strcmp(key, "engine_stats")) {
        /* any change of a polled text makes MPC re-list its menus: hold it while the PRESET menu is in use */
        std::lock_guard<std::mutex> l(in->stm);
        if (mono_ms() - in->fav_list_ms < 15000 && in->stats_cache[0]) return put(buf, n, "%s", in->stats_cache);
        const int cpu = in->emu_cpu.load();
        if (in->stats_cpu < 0 || abs(cpu - in->stats_cpu) >= 10) in->stats_cpu = cpu / 10 * 10;
        snprintf(in->stats_cache, sizeof in->stats_cache, "DROPOUTS %u   CPU %d%%", in->underruns.load(), in->stats_cpu);
        return put(buf, n, "%s", in->stats_cache);
    }
    /* the LCD: lcd_<line>_<col> its text on the glass, lcdi_<line>_<col> its text on an inverted bar, lcdst_<line>_<col> the cell's
     * background (wave_lcd.h S_* bits, a frame of the skin's cell filmstrip) */
    if (!strncmp(key, "lcd", 3)) {
        const char *q = key + 3;
        const int kind = *q == '_' ? 0 : !strncmp(q, "i_", 2) ? 1 : !strncmp(q, "st_", 3) ? 2 : -1;
        if (kind >= 0) {
            q += kind == 0 ? 1 : kind == 1 ? 2 : 3;
            if (q[0] >= '1' && q[0] <= '8' && q[1] == '_' && q[2] >= '1' && q[2] <= '8' && !q[3]) {
                const int r = q[0] - '1', c = q[2] - '1';
                std::lock_guard<std::mutex> l(in->lm);
                const bool inv = in->lcd.style[r][c] & wavelcd::Decoder::S_INV;
                if (kind == 2) return put(buf, n, "%d", in->lcd.style[r][c]);
                return put(buf, n, "%s", (kind == 1) == inv ? in->lcd.text[r][c].c_str() : "");
            }
        }
    }
    if (!strcmp(key, "active_voices")) return put(buf, n, "%d", in->active.load());
    if (!strcmp(key, "gain")) return put(buf, n, "%d", in->gain.load());
    if (!strcmp(key, "midi_mode")) return put(buf, n, "%d", in->midi_all.load() ? 0 : 1);
    if (!strcmp(key, "fader_mode")) return put(buf, n, "%d", in->fader_quick.load() ? 0 : 1);
    if (!strncmp(key, "fader_label_", 12) && key[12] >= '1' && key[12] <= '8' && !key[13])
        return put(buf, n, "%s", in->fader_quick ? kQuickNames[key[12] - '1'] : kPerfNames[key[12] - '1']);
    if (!strncmp(key, "fader_", 6) && key[6] >= '1' && key[6] <= '8' && !key[7]) {
        char k[40];
        if (in->fader_quick) snprintf(k, sizeof k, "%s", kQuickKeys[key[6] - '1']);
        else snprintf(k, sizeof k, "performanceFader%d", key[6] - '0');
        auto f = param_index().find(k);
        return f == param_index().end() ? -1 : put(buf, n, "%d", in->val[f->second].load());
    }
    if (!strcmp(key, "voices")) return put(buf, n, "%d", in->voices.load() >= 12 ? 2 : in->voices.load() >= 8 ? 1 : 0);
    if (!strcmp(key, "display_rev")) return put(buf, n, "%u", in->rev.load());
    if (!strcmp(key, "state")) return put(buf, n, "%s", state_string(in).c_str());
    if (!strcmp(key, "favourite")) {      // the picked favourite while it is the sound, else the first empty slot
        std::lock_guard<std::mutex> l(in->fm);
        const bool cur = in->fav_current >= 0 && in->fav_stamp == in->load_gen;
        return put(buf, n, "%d", cur ? in->fav_current : std::min((int)in->favs.size(), kFavSlots - 1));
    }
    if (!strncmp(key, "favourite:", 10)) {
        in->fav_list_ms = mono_ms();
        std::lock_guard<std::mutex> l(in->fm);
        const int i = atoi(key + 10);
        return put(buf, n, "%s", i >= 0 && i < (int)in->favs.size() ? in->favs[i].first.c_str() : "-");
    }
    if (!strcmp(key, "fav_text")) { std::lock_guard<std::mutex> l(in->fm); return put(buf, n, "FAVOURITES %d / %d", (int)in->favs.size(), kFavSlots); }
    // "<key>_display": a float parameter's value in the processor's own words (Hz, s, ...)
    const size_t kl = strlen(key);
    if (kl > 8 && !strcmp(key + kl - 8, "_display")) {
        auto it = param_index().find(std::string(key, kl - 8));
        if (it == param_index().end() || g_wv_params[it->second].kind != 1 || !in->ready) return -1;
        // the processor lives until destroy(), which the wrapper never runs alongside this; getText only formats a number
        juce::RangedAudioParameter *rp = in->P[it->second];
        if (!rp) return -1;
        juce::String t = rp->getText(in->val[it->second].load() / 127.0f, 24);
        if (rp->getLabel().isNotEmpty() && !t.endsWith(rp->getLabel())) t << " " << rp->getLabel();
        return put(buf, n, "%s", t.toRawUTF8());
    }
    auto it = param_index().find(key);
    if (it != param_index().end()) return put(buf, n, "%d", in->val[it->second].load());
    return -1;
}

void eng_render(void *p, int16_t *out, int frames) {
    Inst *in = (Inst *)p;
    if (!in || !in->ready.load(std::memory_order_acquire)) { memset(out, 0, (size_t)frames * 4); return; }
    uint32_t r = in->rd.load(std::memory_order_relaxed);
    const uint32_t avail = in->wr.load(std::memory_order_acquire) - r;
    const int got = (int)std::min<uint32_t>(avail, (uint32_t)frames);
    for (int i = 0; i < got; i++, r++) {
        out[i * 2] = in->ring[(r & (kRingFrames - 1)) * 2];
        out[i * 2 + 1] = in->ring[(r & (kRingFrames - 1)) * 2 + 1];
    }
    in->rd.store(r, std::memory_order_release);
    if (got < frames) {
        memset(out + got * 2, 0, (size_t)(frames - got) * 4);
        if (mono_ms() > in->loading_until.load(std::memory_order_relaxed)) in->underruns.fetch_add(1, std::memory_order_relaxed);
    }
}

const mpc_engine_t engine = {eng_create, eng_destroy, eng_midi, eng_set, eng_get, eng_render, nullptr, nullptr};

}  // namespace

extern "C" const mpc_engine_t *mpc_engine(void) { return &engine; }
