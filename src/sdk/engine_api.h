/* The engine's C interface (src/dsp/welle_engine.cpp), as the mpc-vst-plugins wrapper used it: string keys and values, 44100 Hz,
 * interleaved int16 stereo. WellePlugin.cpp is the only caller. */
#pragma once
#include <stdint.h>
typedef struct {
    void *(*create)(const char *data_dir);
    void (*destroy)(void *inst);
    void (*midi)(void *inst, const uint8_t *msg, int len);
    void (*set_param)(void *inst, const char *key, const char *val);
    int (*get_param)(void *inst, const char *key, char *buf, int buf_len);
    void (*render)(void *inst, int16_t *out_lr, int frames);
    void (*process)(void *inst, const int16_t *in_lr, int16_t *out_lr, int frames);
    void (*song_pos)(void *inst, double ppq, double bpm, int playing, int frames);
} mpc_engine_t;
extern "C" const mpc_engine_t *mpc_engine(void);
