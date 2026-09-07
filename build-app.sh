#!/bin/zsh
set -euo pipefail

root_dir="${0:A:h}"
app_dir="$root_dir/dist/Keyboard Logo Fix.app"
icon_source="$root_dir/assets/keyboard-logo-fix-icon-heartbeat-1024.png"
iconset_dir="$root_dir/dist/KeyboardLogoFix.iconset"

mkdir -p "$app_dir/Contents/MacOS" "$app_dir/Contents/Resources"
cp "$root_dir/Info.plist" "$app_dir/Contents/Info.plist"
cp "$root_dir/LICENSE" "$app_dir/Contents/Resources/LICENSE"
cp "$root_dir/keyboard-logo-fix" "$app_dir/Contents/MacOS/Keyboard Logo Fix"
chmod +x "$app_dir/Contents/MacOS/Keyboard Logo Fix"

mkdir -p "$iconset_dir"
make_icon() {
    /usr/bin/sips -z "$1" "$1" "$icon_source" \
        --out "$iconset_dir/$2" >/dev/null
}
make_icon 16 icon_16x16.png
make_icon 32 icon_16x16@2x.png
make_icon 32 icon_32x32.png
make_icon 64 icon_32x32@2x.png
make_icon 128 icon_128x128.png
make_icon 256 icon_128x128@2x.png
make_icon 256 icon_256x256.png
make_icon 512 icon_256x256@2x.png
make_icon 512 icon_512x512.png
make_icon 1024 icon_512x512@2x.png
/usr/bin/iconutil -c icns "$iconset_dir" \
    -o "$app_dir/Contents/Resources/KeyboardLogoFix.icns"
rm -rf "$iconset_dir"

xattr -cr "$app_dir"

# CI exports a real Developer ID; locally we fall back to ad-hoc so the app
# still launches on the machine that built it.
sign_identity="${APPLE_SIGN_IDENTITY_APPLICATION:--}"
codesign_opts=(--force --sign "$sign_identity" \
    --identifier com.ikuyu.keyboard-logo-fix)
if [[ "$sign_identity" != "-" ]]; then
    # Notarization rejects anything without hardened runtime, and an untimestamped
    # signature stops validating once the certificate expires.
    codesign_opts+=(--options runtime --timestamp)
fi

codesign "${codesign_opts[@]}" "$app_dir"
codesign --verify --deep --strict --verbose=2 "$app_dir"

echo "Built: $app_dir"
