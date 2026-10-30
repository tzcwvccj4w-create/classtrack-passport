#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
source_dir="$repo_root/tools/mac_manager"
dist_dir="$repo_root/dist"
app_name="ClassTrack 管理工具.app"
app_path="$dist_dir/$app_name"
work_dir="$(mktemp -d "${TMPDIR:-/tmp}/classtrack-mac-app.XXXXXX")"
trap 'rm -rf "$work_dir"' EXIT

staged_app="$work_dir/$app_name"
mkdir -p "$staged_app/Contents/MacOS" "$staged_app/Contents/Resources"
cp "$source_dir/Info.plist" "$staged_app/Contents/Info.plist"
cp "$repo_root/examples/classes.csv" "$staged_app/Contents/Resources/classes.csv"

clang_bin="/Library/Developer/CommandLineTools/usr/bin/clang"
sdk_path="/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk"
if [[ ! -x "$clang_bin" || ! -d "$sdk_path" ]]; then
  echo "Apple Command Line Tools are required to build the native app." >&2
  exit 1
fi
"$clang_bin" -fobjc-arc -Wall -Wextra -Werror \
  -isysroot "$sdk_path" -mmacosx-version-min=12.0 -framework Cocoa -framework UniformTypeIdentifiers \
  "$source_dir/ClassTrackManager.m" -o "$staged_app/Contents/MacOS/ClassTrackManager"

plutil -lint "$staged_app/Contents/Info.plist" >/dev/null
"$staged_app/Contents/MacOS/ClassTrackManager" --self-test
xattr -cr "$staged_app"
codesign --force --deep --sign - "$staged_app" >/dev/null
codesign --verify --deep --strict "$staged_app"

mkdir -p "$dist_dir"
archive="$dist_dir/ClassTrack-Manager-macOS.zip"
rm -f "$archive"
archive_temp="$work_dir/ClassTrack-Manager-macOS.zip"
ditto -c -k --sequesterRsrc --keepParent "$staged_app" "$archive_temp"
mv "$archive_temp" "$archive"

dmg_stage="$work_dir/dmg"
mkdir -p "$dmg_stage"
ditto "$staged_app" "$dmg_stage/$app_name"
ln -s /Applications "$dmg_stage/Applications"
dmg="$dist_dir/ClassTrack-Manager-macOS.dmg"
rm -f "$dmg"
dmg_temp="$work_dir/ClassTrack-Manager-macOS.dmg"
hdiutil create -quiet -volname "ClassTrack 管理工具" -srcfolder "$dmg_stage" -format UDZO "$dmg_temp"
mv "$dmg_temp" "$dmg"

# Keep a directly openable copy beside the installers. The signed copies inside
# ZIP/DMG are the authoritative installation artifacts.
rm -rf "$app_path"
ditto "$staged_app" "$app_path"

echo "APP: $app_path"
echo "ZIP: $archive"
echo "DMG: $dmg"

