# Credits and licences

Welle for MPC stands on the work of others. Each licence applies to its own part; the plugin as a whole is **GPL-3.0** (`LICENSE`).

- **[Wave Emulation](https://github.com/mo0kid/wave)** by mo0kid (DJW Audio), **GPL-3.0-or-later**: the Waldorf Wave emulation (`src/wave/`:
  the voice DSP, the ASIC and CEM3387 models, the firmware runtimes for the master and voice-board 68000s, the disk and set handling, the
  parameter set and the AudioProcessor that ties them together) and its data (`src/wave-data/`: the decoded PPG V6 wavetable samples and a
  blank Wave disk image; see `src/wave-data/README.md`). Vendored with small changes for the MPC: `src/VENDORED.md`.
- **Wave Emulation's panel artwork** (`media/` of the same repository, GPL-3.0-or-later): the knob, red knob and fader-cap SVGs, and the
  scale rings, fader slots and section titles cut from its panel (`skin/parts/`, `tools/make_assets.py`).
- **[JUCE](https://juce.com) 8.0.15**, **AGPL-3.0** (or a commercial JUCE licence): the audio, data-structure, DSP and headless processor
  modules Wave Emulation is written against. Fetched at build time, as upstream does. GPL-3.0 section 13 permits combining GPL-3.0 and
  AGPL-3.0 code. `src/shim/` holds a copy of JUCE's AudioProcessorValueTreeState without its GUI attachments, and of its
  MidiMessageCollector without the MIDI-input side, so no GUI module is needed.
- **[Musashi](https://github.com/kstenerud/Musashi)** by Karl Stenerud, **MIT**: the MC68000 emulator, prepared as Wave Emulation prepares it
  (thread-local CPU state; `cmake/PrepareMusashi.cmake`).
- **Liberation Sans** (Red Hat / Google, **SIL OFL 1.1**) for the page text (`vst/fonts/`).
- **[mpc-vst-plugins](https://github.com/open-mpc/mpc-vst-plugins)**: the plugin wrapper (`wrapper/vst2_wrap.c`), `fast_cores.h` and the skin tools.

The WELLE wordmark (`skin/wordmark/welle.svg`, `tools/make_wordmark.py`) is original to this project.

Welle for MPC is an independent project, not affiliated with or endorsed by Waldorf. No firmware or sound sets are included; the LCD fonts
are read at run time from the user's own OS file.
