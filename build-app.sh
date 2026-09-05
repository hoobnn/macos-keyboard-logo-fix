#!/bin/zsh
set -euo pipefail

root_dir="${0:A:h}"
app_dir="$root_dir/dist/T100 Logo 白色呼吸.app"

mkdir -p "$app_dir/Contents/MacOS" "$app_dir/Contents/Resources"
cp "$root_dir/Info.plist" "$app_dir/Contents/Info.plist"
cp "$root_dir/LICENSE" "$app_dir/Contents/Resources/LICENSE"
cp "$root_dir/t100-logo" "$app_dir/Contents/MacOS/T100 Logo 白色呼吸"
chmod +x "$app_dir/Contents/MacOS/T100 Logo 白色呼吸"
xattr -cr "$app_dir"
codesign --force --deep --sign - \
    --identifier local.codex.t100-logo-white "$app_dir"
codesign --verify --deep --strict --verbose=2 "$app_dir"

echo "Built: $app_dir"
