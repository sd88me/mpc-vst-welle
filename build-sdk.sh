#!/usr/bin/env bash
# Build Welle on the MPC Plugin SDK front end (src/sdk/WellePlugin.cpp) into build/sdk/"sd88me - VST - Welle"/ (aarch64 only: the
# engine is Gen2/Live III). The engine archive comes from ./build.sh engine aarch64 (Docker, gcc); the front end is compiled in the same image.
#   ./build-sdk.sh            gen params, engine archive (if missing), plugin .so, folder (plugin.json, version.xml, Plugin Skins)
#   ./build-sdk.sh beta       the same as "Welle 2 Beta" (uid WeLB): installs next to the release
# MPC_SDK: the mpc-plugin-builder-sdk checkout.
set -euo pipefail
cd "$(dirname "$0")"
SDK="${MPC_SDK:-$HOME/mpc-310-private/mpc-plugin-builder-sdk}"
[ -f "$SDK/framework/mpc_plugin.h" ] || { echo "need the SDK (MPC_SDK)" >&2; exit 1; }
IMAGE=xenia-a64-builder
NAME="Welle"; UID_="'W','e','L','e'"; SLUG=welle; EXTRA=()
if [ "${1:-}" = beta ]; then NAME="Welle 2 Beta"; UID_="'W','e','L','B'"; SLUG=welle2beta; fi
FOLDER="build/sdk/sd88me - VST - $NAME"
python3 tools/gen_params.py >/dev/null
python3 tools/gen_sdk_params.py
if [ "${1:-}" = test ]; then
  # native: the x86 engine (./build.sh engine x86) + the SDK's host simulator, with a scratch data dir (no firmware: the engine idles)
  ./build.sh engine x86
  mkdir -p build/sdk-native
  g++ -std=c++17 -O1 -g -fPIC -shared -fvisibility=hidden -I"$SDK/framework" -Isrc/sdk -DWELLE_NAME='"Welle"' -DWELLE_UID="'W','e','L','e'" \
    -o build/sdk-native/welle.so src/sdk/WellePlugin.cpp vst/build/engine/libwelle_all.a -lpthread -ldl
  scratch="$(mktemp -d)"; mkdir -p "$scratch/Content"; trap 'rm -rf "$scratch"' EXIT
  [ -n "${WELLE_ROMS:-}" ] && ln -s "$WELLE_ROMS" "$scratch/Content/roms"
  MPC_PLUGIN_FOLDER="$scratch" "$SDK/.sdk-tools/host_sim" build/sdk-native/welle.so dump > build/sdk-native/params.json
  MPC_PLUGIN_FOLDER="$scratch" "$SDK/.sdk-tools/host_sim" build/sdk-native/welle.so test --notes 60,64,67 --wav build/sdk-native/test.wav
  exit
fi
[ -f build/sdk-native/params.json ] || "$0" test >/dev/null || true
./build.sh engine aarch64      # the archive is shared with the x86 one (vst/build/engine/): always rebuild the aarch64 one here
rm -rf "$FOLDER"; mkdir -p "$FOLDER/VST" "$FOLDER/Plugin Skins"
docker image inspect $IMAGE >/dev/null 2>&1 || docker build -q -t $IMAGE -f scripts/Dockerfile scripts
docker run --rm -u "$(id -u):$(id -g)" -v "$PWD":/b -v "$SDK":/sdk:ro -w /b $IMAGE aarch64-linux-gnu-g++ \
  -std=c++17 -O2 -fPIC -shared -fvisibility=hidden -fvisibility-inlines-hidden -fno-gnu-unique -ffunction-sections -fdata-sections \
  -Wall -Wextra -mcpu=cortex-a76 -I/sdk/framework -Isrc/sdk \
  -DWELLE_NAME="\"$NAME\"" -DWELLE_UID="$UID_" ${WELLE_DEFS:-} \
  -o "$FOLDER/VST/$SLUG-arm64.so" src/sdk/WellePlugin.cpp vst/build/engine/libwelle_all.a \
  -Wl,--gc-sections -Wl,--version-script=/sdk/tools/exports.map -Wl,--exclude-libs,ALL -static-libstdc++ -static-libgcc -ldl -lpthread -lm
python3 - "$NAME" "$SLUG" "$FOLDER" <<'PY'
import json, sys
name, slug, folder = sys.argv[1:]
m = json.load(open("plugin.json"))
m["name"] = name
if name != "Welle":
    m["uid"] = "WeLB"
m["file64"] = "VST/%s-arm64.so" % slug
m.pop("file32")
json.dump(m, open(folder + "/plugin.json", "w"), indent=2)
open(folder + "/version.xml", "w").write('<?xml version="1.0" encoding="utf-8"?>\n<plugincontent version="1.0">\n    <version>%s.0</version>\n</plugincontent>\n' % m["version"])
PY
# Phase 1: the SDK's generated default skin (pages of 16 parameters). Phase 2 replaces it with skins/ from tools/make_tui.py.
python3 tools/make_tui.py
if [ -d skins ]; then cp -r skins/. "$FOLDER/Plugin Skins/"
else python3 "$SDK/tools/gen_ui.py" --params build/sdk-native/params.json --out "$FOLDER/Plugin Skins" >/dev/null; fi
sed -i 's#</plugincontent>#    <uiversion>0.1.0.0</uiversion>\n</plugincontent>#' "$FOLDER/version.xml"
python3 "$SDK/tools/validate.py" "$FOLDER" || true
echo "built: $FOLDER"
