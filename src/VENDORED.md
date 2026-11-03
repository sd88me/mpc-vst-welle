# Vendored: Wave Emulation

`src/wave/` is `Source/` of [mo0kid/wave](https://github.com/mo0kid/wave) at the commit in `src/wave/UPSTREAM_COMMIT`, without the editor
(`PluginEditor.*`, `StandaloneApp.cpp`, `UI/WaveLcdComponent.*`). `src/wave-data/` is its `data/` (the PPG V6 samples and the blank disk).

## How it is built (CMakeLists.txt)
- JUCE's own CMake is not used (it wants the GUI modules' system packages). The modules the engine needs are compiled one by one
  (`src/gen/jm_*.cpp`): core, events, data_structures, audio_basics, audio_formats, audio_processors_headless, dsp, cryptography.
- `src/shim/juce_audio_processors/`: the headless processor module plus a copy of AudioProcessorValueTreeState without the
  Slider/ComboBox/Button attachments. `src/shim/juce_audio_utils/`: MidiMessageCollector without MidiInputCallback.
- `src/gen/BinaryData.h` and `cmake/assets.S` embed the blank disk and the PPG samples (`juce_add_binary_data` upstream).
- Musashi is prepared with Wave Emulation's own `cmake/PrepareMusashi.cmake`; its opcode tables (`m68kmake`) are generated on the build
  machine (`./build.sh`).

## Changes in src/wave (each marked "MPC port")
- `PluginProcessor.*`: no editor (`createEditor` returns nullptr). There is no JUCE message loop inside MPC: `triggerAsyncUpdate()` sets a
  flag that the port's emulator thread services (`serviceAsyncUpdate()`). `setVoiceLimit()` / `setVoiceCardThreading()` pass through to the
  engine.
- `Dsp/WaldorfEngine.*`: a polyphony cap (`setVoiceLimit`), spread over the three voice cards so each card's thread gets its share (card 1,
  rendered on the thread that also runs the firmware, gets the fewest); voice-card threading from 2 voices instead of 24.
