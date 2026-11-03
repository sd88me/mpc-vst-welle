// Engine-level test through the interface the wrapper uses (mpc_engine): boots Wave OS 1.700 from <dir>/roms, plays notes, moves
// parameters (they must change the sound), changes the voice limit, checks the state chunk round trip and a second instance.
// usage: wavicle_engine_test <data dir with roms/> [seconds of audio per phase] [wav out]
extern "C" {
#include "engine.h"
}
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

static int fails = 0;
#define CHECK(c, ...) do { if (c) printf("ok   " __VA_ARGS__); else { printf("FAIL " __VA_ARGS__); fails++; } printf("\n"); } while (0)
static std::vector<int16_t> g_wav;

static std::string get(const mpc_engine_t *e, void *i, const char *k) {
    char b[8192] = {0};
    if (e->get_param(i, k, b, sizeof b) <= 0) return "";
    return b;
}

static double render_rms(const mpc_engine_t *e, void *i, double secs, double *peak = nullptr) {
    int16_t buf[256];
    double sq = 0, pk = 0; long n = 0;
    const int blocks = (int)(secs * 44100 / 128);
    auto t0 = std::chrono::steady_clock::now();
    for (int b = 0; b < blocks; b++) {
        e->render(i, buf, 128);
        g_wav.insert(g_wav.end(), buf, buf + 256);
        for (int k = 0; k < 256; k++) { double v = buf[k] / 32768.0; sq += v * v; if (fabs(v) > pk) pk = fabs(v); n++; }
        std::this_thread::sleep_until(t0 + std::chrono::microseconds((long)((b + 1) * 128 * 1e6 / 44100)));
    }
    if (peak) *peak = pk;
    return sqrt(sq / n);
}

static bool wait_ready(const mpc_engine_t *e, void *i, int seconds) {
    for (int w = 0; w < seconds * 10; w++) {
        const std::string s = get(e, i, "loading_status");
        if (s.rfind("Ready", 0) == 0) return true;
        if (s.rfind("No ", 0) == 0 || s.find("fail") != std::string::npos || s.find("did not") != std::string::npos) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return false;
}

static void write_wav(const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return;
    const uint32_t data = (uint32_t)g_wav.size() * 2, rate = 44100, br = rate * 4, sz = 36 + data;
    const uint16_t fmt = 1, ch = 2, ba = 4, bits = 16;
    const uint32_t fsz = 16;
    fwrite("RIFF", 1, 4, f); fwrite(&sz, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&fsz, 4, 1, f);
    fwrite(&fmt, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&rate, 4, 1, f); fwrite(&br, 4, 1, f); fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&data, 4, 1, f); fwrite(g_wav.data(), 2, g_wav.size(), f);
    fclose(f);
}


static int g_active = 0;
static double note_rms(const mpc_engine_t *e, void *i, double secs, int notes = 1) {
    for (int k = 0; k < notes; k++) { uint8_t on[3] = {0x90, (uint8_t)(48 + 4 * k), 100}; e->midi(i, on, 3); }
    double pk = 0;
    const double r = render_rms(e, i, secs, &pk);
    g_active = atoi(get(e, i, "active_voices").c_str());
    for (int k = 0; k < notes; k++) { uint8_t off[3] = {0x80, (uint8_t)(48 + 4 * k), 0}; e->midi(i, off, 3); }
    render_rms(e, i, 1.0);
    return r;
}

int main(int argc, char **argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc < 2) { fprintf(stderr, "usage: %s <data dir> [seconds] [out.wav]\n", argv[0]); return 2; }
    const double secs = argc > 2 ? atof(argv[2]) : 1.5;
    const mpc_engine_t *e = mpc_engine();
    for (int round = 0; round < 2; round++) {
        void *i = e->create(argv[1]);
        CHECK(i != nullptr, "create");
        auto t0 = std::chrono::steady_clock::now();
        const bool up = wait_ready(e, i, 120);
        printf("boot took %.1f s: firmware %s, status \"%s\"\n", std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count(),
               get(e, i, "rom_name").c_str(), get(e, i, "loading_status").c_str());
        CHECK(up, "boot finished");
        if (!up) { e->destroy(i); printf("FAILED\n"); return 1; }
        printf("bank \"%s\" program \"%s\" tile1 \"%s\" cutoff %s (%s)\n", get(e, i, "bank_name").c_str(), get(e, i, "preset_name").c_str(),
               get(e, i, "patch_slot_1").c_str(), get(e, i, "cutoff").c_str(), get(e, i, "cutoff_display").c_str());
        render_rms(e, i, 0.5);
        int lit = 0;
        for (int ln = 1; ln <= 8; ln++) {
            std::string row;
            for (int c = 1; c <= 8; c++) {
                char k[16]; snprintf(k, sizeof k, "lcd_%d_%d", ln, c);
                const std::string t = get(e, i, k);
                if (!t.empty()) lit++;
                char cell[64]; snprintf(cell, sizeof cell, "[%-10s]", t.c_str()); row += cell;
            }
            if (row.find_first_not_of("[] ") != std::string::npos) printf("LCD %s\n", row.c_str());
        }
        CHECK(lit > 4, "the LCD reads back as text (%d cells)", lit);
        const double a = note_rms(e, i, secs);
        CHECK(a > 0.005, "a note sounds (rms %.4f)", a);
        if (round == 1) { e->destroy(i); break; }
        const std::string cut = get(e, i, "cutoff");
        e->set_param(i, "cutoff", "10");
        render_rms(e, i, 0.1);
        printf("cutoff 10 -> reads %s (%s)\n", get(e, i, "cutoff").c_str(), get(e, i, "cutoff_display").c_str());
        const double b = note_rms(e, i, secs);
        CHECK(b < a * 0.7, "closing the filter darkens it (rms %.4f -> %.4f)", a, b);
        e->set_param(i, "cutoff", cut.c_str());
        e->set_param(i, "filterMode", "2");
        CHECK(get(e, i, "filterMode") == "2", "option parameter reads back");
        e->set_param(i, "filterMode", "0");
        note_rms(e, i, secs, 12);
        const int a8 = g_active;
        e->set_param(i, "voices", "2");
        render_rms(e, i, 0.1);
        note_rms(e, i, secs, 12);
        CHECK(a8 == 8 && g_active == 12, "12 notes: %d voices with the 8 limit, %d with 16", a8, g_active);
        e->set_param(i, "gain", "150");
        e->set_param(i, "resonance", "90");
        render_rms(e, i, 0.2);
        const std::string st = get(e, i, "state");
        printf("state %zu bytes\n", st.size());
        CHECK(st.size() > 100 && st.size() < 8000, "state fits the wrapper's chunk");
        e->set_param(i, "resonance", "0");
        e->set_param(i, "gain", "100");
        e->set_param(i, "state", st.c_str());
        render_rms(e, i, 0.5);
        CHECK(get(e, i, "resonance") == "90" && get(e, i, "gain") == "150" && get(e, i, "voices") == "2",
              "state restores (resonance %s, gain %s, voices %s)", get(e, i, "resonance").c_str(), get(e, i, "gain").c_str(), get(e, i, "voices").c_str());
        printf("stats: %s\n", get(e, i, "engine_stats").c_str());
        e->destroy(i);
    }
    if (argc > 3) write_wav(argv[3]);
    printf(fails ? "FAILED (%d)\n" : "ALL OK\n", fails);
    return fails ? 1 : 0;
}
