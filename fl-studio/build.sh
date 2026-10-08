#!/bin/sh
# Builds the WhatKey VST3 + AU and installs them into ~/Library/Audio/Plug-Ins.
# Re-run if you move the repo or switch Python installs (paths are baked in).
set -e
cd "$(dirname "$0")"

PY=$("${PYTHON:-python3}" -c 'import sys, numpy; print(sys.executable)' 2>/dev/null) \
    || { echo "Need python3 with numpy (pip install numpy)"; exit 1; }
FFMPEG=$(command -v ffmpeg) || { echo "ffmpeg not found (brew install ffmpeg)"; exit 1; }
SCRIPT="$(cd .. && pwd)/whatkey.py"

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    -DWHATKEY_PYTHON="$PY" -DWHATKEY_SCRIPT="$SCRIPT" \
    -DWHATKEY_PATH="$(dirname "$FFMPEG"):/usr/local/bin:/opt/homebrew/bin:/usr/bin:/bin"
cmake --build build --config Release -j "$(sysctl -n hw.ncpu)"

echo
echo "Installed:"
echo "  ~/Library/Audio/Plug-Ins/VST3/WhatKey.vst3"
echo "  ~/Library/Audio/Plug-Ins/Components/WhatKey.component"
echo "In FL Studio: Options > Manage plugins > Find more plugins, then add WhatKey to a mixer insert."
