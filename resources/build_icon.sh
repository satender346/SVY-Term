#!/usr/bin/env bash
# build_icon.sh — generate SVY-Term.icns and embed it in the app bundle.
# Usage: bash resources/build_icon.sh <app_bundle_path>
set -euo pipefail

APP="${1:-build/SVY-Term.app}"
ICONSET="/tmp/SVY-Term.iconset"
ICNS="resources/SVY-Term.icns"

# 1. Generate source PNGs
mkdir -p "$ICONSET"
python3 resources/gen_icon.py "$ICONSET"

# 2. Rename to iconutil's required naming scheme
python3 - "$ICONSET" <<'PYEOF'
import shutil, os, sys
d = sys.argv[1]
pairs = [
    ("icon_16x16.png",    "icon_16x16.png"),
    ("icon_32x32.png",    "icon_16x16@2x.png"),
    ("icon_32x32.png",    "icon_32x32.png"),
    ("icon_64x64.png",    "icon_32x32@2x.png"),
    ("icon_128x128.png",  "icon_128x128.png"),
    ("icon_256x256.png",  "icon_128x128@2x.png"),
    ("icon_256x256.png",  "icon_256x256.png"),
    ("icon_512x512.png",  "icon_256x256@2x.png"),
    ("icon_512x512.png",  "icon_512x512.png"),
    ("icon_1024x1024.png","icon_512x512@2x.png"),
]
for src, dst in pairs:
    s = os.path.join(d, src)
    t = os.path.join(d, dst)
    if os.path.exists(s) and s != t:
        shutil.copy2(s, t)
PYEOF

# 3. Convert iconset → icns
iconutil --convert icns "$ICONSET" --output "$ICNS"
echo "Generated $ICNS ($(du -h "$ICNS" | cut -f1))"

# 4. Embed in app bundle
mkdir -p "$APP/Contents/Resources"
cp "$ICNS" "$APP/Contents/Resources/SVY-Term.icns"

# 5. Patch Info.plist
PLIST="$APP/Contents/Info.plist"
if /usr/libexec/PlistBuddy -c "Print :CFBundleIconFile" "$PLIST" &>/dev/null; then
    /usr/libexec/PlistBuddy -c "Set :CFBundleIconFile SVY-Term" "$PLIST"
else
    /usr/libexec/PlistBuddy -c "Add :CFBundleIconFile string SVY-Term" "$PLIST"
fi
echo "Icon embedded in $APP"
