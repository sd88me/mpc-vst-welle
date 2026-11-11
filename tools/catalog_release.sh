#!/usr/bin/env bash
# The catalog release (Plugin Manager / OpenMPC catalog): dist/Welle-<version>-mpc-aarch64.zip, made by the catalog's release.py
# from the SDK build. The plugin folder is the SDK folder with the .so moved to its root (plugin.json still marks the folder, so
# the plugin finds Content/ the same way) plus mpc-plugin.json, whose user_files let the Plugin Manager upload the Wave OS ROMs
# and sound sets into Content/roms and Content/banks.
#   tools/catalog_release.sh        (MPC_VST_PLUGINS overrides the path to the mpc-vst-plugins checkout)
set -euo pipefail
cd "$(dirname "$0")/.."
TOOLS="${MPC_VST_PLUGINS:-$HOME/mpc-vst-plugins-postlaunch}/tools"
./build-sdk.sh
VER="$(python3 -c 'import json;print(json.load(open("plugin.json"))["version"])')"
NAME="sd88me - VST - Welle"
SRC="build/sdk/$NAME"
ST="build/catalog"
rm -rf "$ST"; mkdir -p "$ST/$NAME"
cp -r "$SRC/Plugin Skins" "$SRC/version.xml" "$ST/$NAME/"
cp LICENSE CREDITS.md "$ST/$NAME/"
cp "$SRC/VST/welle-arm64.so" "$ST/welle-arm64.so"
mkdir -p "$ST/content/roms" "$ST/content/banks"
printf 'Put w2sys.bin and wdv.sys (Wave OS 1.700) here, or upload them from the Plugin Manager page.\n' > "$ST/content/roms/README.txt"
printf 'Put your Wave sound sets (.set or .img) here. A set whose name starts with "Factory" is listed first.\n' > "$ST/content/banks/README.txt"
# uid "WeLe" as hex (0x57654c65): the same VST uid
cat > "$ST/pluginlist-entry.xml" <<EOF
<PLUGIN name="Welle" descriptiveName="Welle" format="VST" category="Synth" manufacturer="sd88me" version="$VER" file="/sdcard/Synths/$NAME/welle-arm64.so" uid="57654c65" isInstrument="1" fileTime="0" infoUpdateTime="0" numInputs="0" numOutputs="2" isShell="0"/>
EOF
cat > "$ST/user_files.json" <<'EOF'
[
  { "path": "Content/roms", "label": "Wave OS 1.700",
    "description": "Welle runs the Waldorf Wave's own operating system: add your copy of both Wave OS 1.700 files.",
    "rules": [
      { "label": "Wave OS 1.700 (w2sys.bin)", "ext": [".bin"], "size": 302768, "signature": "NuWAVE Operating System", "save_as": "w2sys.bin" },
      { "label": "Wave DSP file (wdv.sys)", "ext": [".sys"], "size": 12840, "header_hex": "0102010b", "save_as": "wdv.sys" }
    ] },
  { "path": "Content/banks", "label": "Wave sound sets (optional)",
    "description": "Wave sound sets (.set or .img). A set whose name starts with \"Factory\" is listed first.",
    "rules": [
      { "label": "Wave sound set", "ext": [".set", ".img"], "size_min": 1024, "size_max": 16777216, "unverified": true,
        "note": "The plugin checks the set when it loads it." }
    ] }
]
EOF
python3 "$TOOLS/release.py" --so "$ST/welle-arm64.so" --skin "$ST/$NAME" --entry "$ST/pluginlist-entry.xml" --version "$VER" \
  --extra "$ST/content:Content" --user-data Content/roms --user-data Content/banks --user-files "$ST/user_files.json" \
  --id welle --repo sd88me/mpc-vst-welle --license GPL-3.0-only \
  --about "Waldorf Wave emulation running the Wave OS. Needs your own Wave OS 1.700 files." -o dist
python3 "$TOOLS/catalog_check.py" --catalog "dist/Welle-$VER-mpc-aarch64.zip"
