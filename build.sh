#!/usr/bin/env bash
# Build Wavicle. The engine (Wave Emulation + JUCE (headless) + Musashi + src/dsp glue) is built with CMake into one static archive,
# vst/build/engine/libwavicle_all.a, which vst/vst.json links into the plugin through tools/build_port.sh.
#   ./build.sh engine [x86|aarch64]   just the archive for that architecture (default aarch64, cross-compiled in Docker)
#   ./build.sh test                   x86 engine + the offline host test (tools/test_port.sh, ASan/UBSan)
#   ./build.sh [all]                  aarch64 engine + skin + wavicle.so in vst/build/aarch64/ (Gen2)
# MPC_VST: checkout of mpc-vst-plugins (default ~/mpc-vst-plugins).
set -euo pipefail
cd "$(dirname "$0")"
MPC_VST="${MPC_VST:-$HOME/mpc-vst-plugins}"
IMAGE=xenia-a64-builder

merge() {   # merge <ar> <build dir>
  local AR="$1" B="$2" out="vst/build/engine/libwavicle_all.a"
  mkdir -p vst/build/engine
  { echo "CREATE $out"
    for l in libwavicle_dsp.a libWaveCore.a libwave_juce.a libWaveM68k.a; do
      [ -f "$B/$l" ] || { echo "missing $B/$l" >&2; exit 1; }
      echo "ADDLIB $B/$l"
    done
    echo "SAVE"; echo "END"; } | $AR -M
  echo "engine archive: $out ($(du -h $out | cut -f1))"
}

musashi() {   # Musashi's opcode tables, generated once by its m68kmake built for this machine (the cross image has no host compiler)
  [ -f build/musashi-gen/m68kops.c ] && return
  mkdir -p build/musashi-gen
  cc -O1 -o build/musashi-gen/m68kmake third_party/Musashi/m68kmake.c
  build/musashi-gen/m68kmake build/musashi-gen third_party/Musashi/m68k_in.c >/dev/null
}

deps() {      # JUCE and Musashi at the versions Wave Emulation pins (not vendored: large, and fetched as upstream does)
  [ -d third_party/JUCE ] || git clone -q --depth 1 -b 8.0.15 https://github.com/juce-framework/JUCE.git third_party/JUCE
  [ -d third_party/Musashi ] || { git clone -q https://github.com/kstenerud/Musashi.git third_party/Musashi
                                  git -C third_party/Musashi checkout -q 313ebf1bd9f4d0d93341eb5ce21fd8a119e9dbdd; }
  musashi
}

engine() {
  deps
  case "${1:-aarch64}" in
    x86)
      cmake -S . -B build/x86 -G Ninja -DCMAKE_BUILD_TYPE=Release -DMPC_VST="$MPC_VST" >/dev/null
      cmake --build build/x86 --target wavicle_dsp -j"$(nproc)"
      merge ar build/x86 ;;
    aarch64)
      docker image inspect $IMAGE >/dev/null 2>&1 || docker build -q -t $IMAGE -f scripts/Dockerfile scripts
      docker run --rm -u "$(id -u):$(id -g)" -v "$PWD":/b -v "$MPC_VST":/mv:ro -w /b $IMAGE bash -euc '
        cmake -S . -B build/a64 -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-toolchain.cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS=-mcpu=cortex-a76 -DCMAKE_C_FLAGS=-mcpu=cortex-a76 -DMPC_VST=/mv >/dev/null
        cmake --build build/a64 --target wavicle_dsp -j"$(nproc)"'
      merge "docker run --rm -i -u $(id -u):$(id -g) -v $PWD:/b -w /b $IMAGE aarch64-linux-gnu-ar" build/a64 ;;
    *) echo "unknown arch $1" >&2; exit 1 ;;
  esac
}

case "${1:-all}" in
  engine) engine "${2:-aarch64}" ;;
  test)   python3 tools/gen_params.py; python3 tools/make_skin.py; engine x86; "$MPC_VST/tools/test_port.sh" vst/vst.json ;;
  all)    python3 tools/gen_params.py; python3 tools/make_skin.py; engine aarch64; "$MPC_VST/tools/build_port.sh" vst/vst.json aarch64 ;;
  *) sed -n 2,9p "$0"; exit 1 ;;
esac
