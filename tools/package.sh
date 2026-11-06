#!/usr/bin/env bash
# The release zip: the plugin folder in the MPC plugin folder format. Copy the folder into "VST2 Plugins" at the root of a drive and restart.
#   tools/package.sh            -> dist/VST2 Plugins/sd88me - VST - Welle/ and dist/Welle-<version>-VST2-Plugins.zip (version from plugin.json)
# Builds with ./build-sdk.sh first. aarch64 (Gen2) only. No firmware or sounds are included: Content/roms and Content/banks ship empty.
set -euo pipefail
cd "$(dirname "$0")/.."
./build-sdk.sh
VER="$(python3 -c 'import json;print(json.load(open("plugin.json"))["version"])')"
NAME="sd88me - VST - Welle"
OUT="dist/VST2 Plugins/$NAME"
rm -rf "dist/VST2 Plugins"; mkdir -p "$(dirname "$OUT")"
cp -r "build/sdk/$NAME" "$OUT"
mkdir -p "$OUT/Content/roms" "$OUT/Content/banks"
printf 'Put w2sys.bin and wdv.sys (Wave OS 1.700) here.\n' > "$OUT/Content/roms/README.txt"
printf 'Put your Wave sound sets (.set or .img) here. A set whose name starts with "Factory" is listed first.\n' > "$OUT/Content/banks/README.txt"
cp LICENSE CREDITS.md "$OUT/"
if find "$OUT" \( -iname "*.mid" -o -iname "*.bin" -o -iname "*.syx" -o -iname "*.dat" -o -iname "*.sys" -o -iname "*.img" -o -iname "*.set" \) | grep -q .; then
  echo "refusing: a ROM-like file is in the folder" >&2; exit 1; fi
(cd dist && rm -f "Welle-$VER-VST2-Plugins.zip" && python3 -m zipfile -c "Welle-$VER-VST2-Plugins.zip" "VST2 Plugins")
echo "ok: $OUT, dist/Welle-$VER-VST2-Plugins.zip"
