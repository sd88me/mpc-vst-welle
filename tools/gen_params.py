#!/usr/bin/env python3
"""Write vst/params.json and src/dsp/welle_params.h from Wave Emulation's own parameter list.

tools/wave_params.jsonl is that list as the processor reports it (build/x86/wave_dump_params > tools/wave_params.jsonl; rerun after
updating src/wave). Every one of its parameters becomes an MPC parameter with the same key (the APVTS parameter ID):
  - int / bool / choice parameters keep their own whole-number range (an option list where we know the labels),
  - float parameters become 0..127 steps of the parameter's normalised range (the Wave's own 7-bit resolution); their value text is
    the processor's (Hz, ms, ...), answered by the engine as "<key>_display".
The VST parameter index is the position in params.json and saved projects bind to it: append only."""
import json, os

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")
SRC = os.path.join(HERE, "wave_params.jsonl")
OUT = os.path.join(ROOT, "vst", "params.json")
HDR = os.path.join(ROOT, "src", "dsp", "welle_params.h")

W = [json.loads(l) for l in open(SRC)]

MODSRC = ["LFO 1", "LFO 2", "Amp Env", "Filter Env", "Wave Env", "Free Env", "Src 6", "Src 7", "Src 8", "Src 9",
          "Ctrl S&H", "Comp +", "Comp -", "Keytrack", "Velocity", "Src 15", "Aftertouch", "Src 17", "Src 18", "Src 19",
          "Src 20", "Pitch Bend", "Mod Wheel", "Free Whl +", "Free Whl -", "Free Wheel", "Gate", "Src 27", "Src 28", "Button 1",
          "Button 2", "Volume", "Pan", "Breath", "Control X", "Control Y", "Src 36", "Minimum", "Maximum", "Src 39"]
OPT = {
    "filterMode": ["Low-pass", "High-pass", "Band-pass", "LP + HP"],
    "highpassEnvelopeSelect": ["Amp Env", "Filter Env", "Wave Env", "Free Env"],
    "panMode": ["Off", "Normal", "Inverted"],
    "lfo1Shape": ["Sine", "Triangle", "Saw", "Square", "Smooth Rnd", "S&H"],
    "lfo1Sync": ["Off", "Sync", "Retrig"],
    "glideType": ["Normal", "Normal Gliss", "MIDI", "MIDI Gliss", "Fingered", "Fingered Gliss"],
    "glideTimeMode": ["Rate", "Time"],
}
OPT["lfo2Shape"], OPT["lfo2Sync"] = OPT["lfo1Shape"], OPT["lfo1Sync"]

SHORT = {   # names for the MPC screen (the processor's own are long)
    "position": "Wave Pos", "scan": "Wave Env Amt", "position2": "Wave 2 Pos", "scan2": "Wave 2 Env Amt",
    "oscillatorLink": "Osc 2 Link", "balance": "Osc Balance", "detune": "Osc Detune", "drive": "Drive",
    "filterEnv": "Filter Env", "filterVelocity": "Filter Vel", "filterKeytrack": "Filter Key", "filterKeyCenter": "Key Center",
    "filterDelay": "F Env Delay", "filterAttack": "F Env Attack", "filterDecay": "F Env Decay", "filterSustain": "F Env Sustain",
    "filterRelease": "F Env Release", "highpassCutoff": "HP Cutoff", "highpassEnvelopeSelect": "HP Env",
    "highpassEnvelopeAmount": "HP Env Amt", "highpassVelocity": "HP Velocity", "highpassKeytrack": "HP Keytrack",
    "highpassKeyCenter": "HP Key Center", "bandpassBandwidth": "BP Width", "panMode": "Pan Mod Mode", "circuitAge": "Circuit Age",
    "freeEnvZeroAxis": "Free Env Zero", "waveEnvKeyOff": "Key Off Point", "waveEnvLoopStart": "Loop Start",
    "waveEnvLoopEnabled": "Env Loop", "glideTimeMode": "Glide Mode", "quickWavePosition": "Quick Wave Pos", "glideRateModSource": "Glide Mod Src",
    "glideRateModAmount": "Glide Mod Amt",
}

params, sec_keys = [], {}


def add(p, sec):
    params.append(p)
    sec_keys.setdefault(sec, []).append(p["key"])


def section(pid):
    if pid.startswith(("wave", "oscillator", "position", "scan", "wavetable")) and not pid.startswith("waveEnv"):
        return "Waves" if "Mod" not in pid else "Mod"
    if pid.startswith("waveEnv"):
        return "Wave Env"
    if pid.startswith("performanceFader"):
        return "Faders"
    if pid.startswith(("lfo", "glide")) and "Mod" not in pid:
        return "LFO"
    if "Mod" in pid and ("Source" in pid or "Control" in pid or "Amount" in pid):
        return "Mod"
    if pid.startswith(("filter", "cutoff", "resonance", "highpass", "bandpass")):
        return "Filter"
    if pid.startswith("freeEnv"):
        return "Free Env"
    if pid.startswith("quick"):
        return "Quick"
    return "Amp"


def short(w):
    pid, n = w["id"], w["name"]
    if pid in SHORT:
        return SHORT[pid]
    n = (n.replace("Modulation ", "Mod ").replace("Envelope", "Env").replace("Oscillator ", "Osc ").replace("Filter Env Stage ", "F Stage ")
         .replace(" Source", " Src").replace(" Amount", " Amt").replace("Wave Env Time ", "Time ").replace("Wave Env Level ", "Level ")
         .replace("Free Env Time ", "Time ").replace("Free Env Level ", "Level ").replace("Performance Fader ", "Fader "))
    return n


# --- the port's own controls ---
add(dict(key="bank_index", name="BANK", min=0, max=63, default=0, display="int", nudge_pct=10), "Browse")
add(dict(key="preset", name="PATCH", min=0, max=255, default=0, display="int", nudge_pct=10), "Browse")
add(dict(key="bank_name", name="BANK NAME", min=0, max=0, default=0, display="string", poll=True), "Browse")
add(dict(key="preset_name", name="PATCH NAME", min=0, max=0, default=0, display="string", poll=True), "Browse")
for k, n in (("bank_prev", "BANK -"), ("bank_next", "BANK +"), ("preset_prev", "PATCH -"), ("preset_next", "PATCH +"),
             ("all_notes_off", "PANIC")):
    add(dict(key=k, name=n, min=0, max=1, default=0, momentary=True), "Browse")
add(dict(key="rom_name", name="FIRMWARE", min=0, max=0, default=0, display="string", poll=True), "System")
add(dict(key="loading_status", name="STATUS", min=0, max=0, default=0, display="string", poll=True), "System")
add(dict(key="engine_stats", name="STATS", min=0, max=0, default=0, display="string", poll=True), "System")
add(dict(key="gain", name="GAIN %", min=10, max=400, default=100, display="int"), "System")
add(dict(key="voices", name="VOICES", options=["6", "8", "12"], default="8"), "System")
add(dict(key="init_patch", name="RELOAD", min=0, max=1, default=0, momentary=True), "System")

# --- every Wave parameter ---
# Wave parameters added after the first release of the port (new upstream or MPC-port ones): appended after everything, so the VST
# indices saved projects bind to never move (vst/params.lock)
LATE = ["quickCutoff", "quickResonance", "quickFilterEnv", "quickWavePosition"]
late_entries = []
_add = add


def add(p, sec):
    if p["key"] in LATE:
        late_entries.append((p, sec))
    else:
        _add(p, sec)


ROWS = []   # (key, kind, lo, hi, def)  kind 0: whole number (native range), 1: float as 0..127 of the normalised range
for w in W:
    pid, kind = w["id"], w["kind"]
    sec = section(pid)
    if pid.endswith(("ModSource", "Source", "Control")) and w["max"] == 39:
        labels = MODSRC
    elif pid in OPT:
        labels = OPT[pid]
    elif kind == "bool":
        labels = ["Off", "On"]
    else:
        labels = None
    if labels is not None:
        lo = int(w["min"])
        if len(labels) != int(w["max"]) - lo + 1:
            raise SystemExit("%s: %d labels for %g..%g" % (pid, len(labels), w["min"], w["max"]))
        d = int(round(w["def"])) - lo
        add(dict(key=pid, name=short(w), options=labels, default=labels[d]), sec)
        ROWS.append((pid, 2, lo, int(w["max"]), int(round(w["def"]))))
    elif kind in ("int",):
        add(dict(key=pid, name=short(w), min=int(w["min"]), max=int(w["max"]), default=int(round(w["def"])), display="int"), sec)
        ROWS.append((pid, 0, int(w["min"]), int(w["max"]), int(round(w["def"]))))
    else:
        d = round((w["def"] - w["min"]) / (w["max"] - w["min"]) * 127) if w["skew"] == 1 else 0
        add(dict(key=pid, name=short(w), min=0, max=127, default=int(d), display="int"), sec)
        ROWS.append((pid, 1, 0, 127, int(d)))

# --- browser ---
N_BANK_TILES, N_TILES = 10, 32
for i in range(1, N_BANK_TILES + 1):
    add(dict(key="bank_slot_%d" % i, name="BANK %d" % i, min=0, max=1, default=0, momentary=True, display="string", poll=True), "Banks")
for i in range(1, N_TILES + 1):
    add(dict(key="patch_slot_%d" % i, name="PATCH %d" % i, min=0, max=1, default=0, momentary=True, display="string", poll=True), "Banks")
add(dict(key="patch_page_text", name="PAGE", min=0, max=0, default=0, display="string", poll=True), "Banks")
add(dict(key="patch_page_prev", name="PAGE -", min=0, max=1, default=0, momentary=True), "Banks")
add(dict(key="patch_page_next", name="PAGE +", min=0, max=1, default=0, momentary=True), "Banks")
add(dict(key="bank_page_text", name="BANK PAGE", min=0, max=0, default=0, display="string", poll=True), "Banks")
add(dict(key="bank_page_prev", name="BANK PAGE -", min=0, max=1, default=0, momentary=True), "Banks")
add(dict(key="bank_page_next", name="BANK PAGE +", min=0, max=1, default=0, momentary=True), "Banks")

for ln in range(1, 9):     # the Wave's LCD read back as text (src/dsp/wave_lcd.h): line, column
    for c in range(1, 9):
        add(dict(key="lcd_%d_%d" % (ln, c), name="LCD %d.%d" % (ln, c), min=0, max=0, default=0, display="string", poll=True), "LCD")

add(dict(key="favourite", name="FAVOURITE", min=0, max=63, default=63, display="int", nudge_pct=10), "Banks")
add(dict(key="fav_save", name="SAVE FAV", min=0, max=1, default=0, momentary=True), "Banks")
add(dict(key="fav_delete", name="DEL FAV", min=0, max=1, default=0, momentary=True), "Banks")
add(dict(key="fav_text", name="FAVOURITES", min=0, max=0, default=0, display="string", poll=True), "Banks")

for ln in range(1, 9):     # the LCD's inverted text and each cell's background (wave_lcd.h S_* bits)
    for c in range(1, 9):
        add(dict(key="lcdi_%d_%d" % (ln, c), name="LCD INV %d.%d" % (ln, c), min=0, max=0, default=0, display="string", poll=True), "LCD")
for ln in range(1, 9):
    for c in range(1, 9):
        add(dict(key="lcdst_%d_%d" % (ln, c), name="LCD CELL %d.%d" % (ln, c), min=0, max=31, default=0, display="int", poll=True), "LCD")

add(dict(key="midi_mode", name="MIDI", options=["All", "As sent"], default="All"), "System")
# the MAIN faders: the Wave's Quick Edit offsets (default) or the Performance faders as each performance assigns them
add(dict(key="fader_mode", name="FADERS", options=["Quick", "Perf"], default="Quick"), "System")
for i in range(1, 9):
    add(dict(key="fader_%d" % i, name="FADER %d" % i, min=0, max=127, default=64, display="int"), "System")
for i in range(1, 9):
    add(dict(key="fader_label_%d" % i, name="FADER %d NAME" % i, min=0, max=0, default=0, display="string", poll=True), "System")

add = _add
for p_, sec_ in sorted(late_entries, key=lambda e: LATE.index(e[0]["key"])):
    add(p_, sec_)

order = ["Browse", "LCD", "Banks", "System", "Waves", "Wave Env", "Filter", "Amp", "LFO", "Mod", "Free Env", "Quick", "Faders"]
sections = [{"label": s, "keys": sec_keys[s]} for s in order if s in sec_keys]
json.dump({"name": "Welle", "params": params, "sections": sections}, open(OUT, "w"), indent=1)

with open(HDR, "w") as f:
    f.write("/* generated by tools/gen_params.py from tools/wave_params.jsonl: do not edit.\n"
            " * One row per Wave Emulation parameter: key (= its APVTS ID), kind (0: whole number, value = the parameter's own;\n"
            " * 1: float, value 0..127 = normalised * 127; 2: option list, value index = parameter value - lo), range, default. */\n"
            "#pragma once\nstruct wv_param_t { const char *key; int kind, lo, hi, def; };\nstatic const wv_param_t g_wv_params[] = {\n")
    for r in ROWS:
        f.write('    {"%s", %d, %d, %d, %d},\n' % r)
    f.write("};\nstatic const int WV_NUM_PARAMS = %d;\n#define WV_PATCH_TILES %d\n#define WV_BANK_TILES %d\n" % (len(ROWS), N_TILES, N_BANK_TILES))
print("params.json: %d params (%d Wave), %d sections" % (len(params), len(ROWS), len(sections)))
