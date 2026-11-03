// Hold <notes> notes for <seconds> with the voice option <0|1> (8|16), pulling audio in real time as MPC would; report dropouts.
// usage: welle_hold_bench <data dir> <notes> <voices option> [seconds]
extern "C" {
#include "engine.h"
}
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
static std::string get(const mpc_engine_t* e, void* i, const char* k) { char b[256] = {0}; return e->get_param(i, k, b, sizeof b) > 0 ? b : ""; }
int main(int argc, char** argv)
{
    const mpc_engine_t* e = mpc_engine();
    void* i = e->create(argv[1]);
    while (get(e, i, "loading_status").rfind("Ready", 0) != 0) std::this_thread::sleep_for(std::chrono::milliseconds(100));
    e->set_param(i, "voices", argv[3]);
    if (argc > 6) {   // bank, program
        e->set_param(i, "bank_index", argv[5]);
        int16_t z[256]; for (int k = 0; k < 44100 * 3 / 128; k++) e->render(i, z, 128), std::this_thread::sleep_for(std::chrono::microseconds(2900));
        e->set_param(i, "preset", argv[6]);
        for (int k = 0; k < 44100 * 2 / 128; k++) e->render(i, z, 128), std::this_thread::sleep_for(std::chrono::microseconds(2900));
        printf("%s / %s\n", get(e, i, "bank_name").c_str(), get(e, i, "preset_name").c_str());
    }
    const int notes = atoi(argv[2]);
    const double secs = argc > 4 ? atof(argv[4]) : 10;
    int16_t buf[256];
    auto t0 = std::chrono::steady_clock::now();
    const int blocks = (int)(secs * 44100 / 128);
    const unsigned d0 = (unsigned)atoi(get(e, i, "engine_stats").c_str() + 9);
    for (int b = 0; b < blocks; b++) {
        if (b == 40) for (int k = 0; k < notes; k++) { uint8_t on[3] = {0x90, (uint8_t)(40 + 3 * k), 100}; e->midi(i, on, 3); }
        e->render(i, buf, 128);
        std::this_thread::sleep_until(t0 + std::chrono::microseconds((long)((b + 1) * 128 * 1e6 / 44100)));
    }
    printf("%d notes, voices option %s: %s, active %s (start %u)\n", notes, argv[3], get(e, i, "engine_stats").c_str(),
           get(e, i, "active_voices").c_str(), d0);
    e->destroy(i);
}
