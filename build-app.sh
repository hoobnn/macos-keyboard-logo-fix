#!/bin/zsh
set -euo pipefail

root_dir="${0:A:h}"
app_dir="$root_dir/dist/Keyboard Logo Fix.app"

mkdir -p "$app_dir/Contents/MacOS" "$app_dir/Contents/Resources"
cp "$root_dir/Info.plist" "$app_dir/Contents/Info.plist"
cp "$root_dir/LICENSE" "$app_dir/Contents/Resources/LICENSE"
cp "$root_dir/keyboard-logo-fix" "$app_dir/Contents/MacOS/Keyboard Logo Fix"
chmod +x "$app_dir/Contents/MacOS/Keyboard Logo Fix"
xattr -cr "$app_dir"
codesign --force --deep --sign - \
    --identifier com.ikuyu.keyboard-logo-fix "$app_dir"
codesign --verify --deep --strict --verbose=2 "$app_dir"

echo "Built: $app_dir"
