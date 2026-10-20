#!/usr/bin/env bash
# GPL-2.0-or-later. Run INSIDE the existing validated VideoLAN build container.
set -euo pipefail
if [[ $# != 2 ]]; then
    echo "usage: $0 VLC_BUILD_DIRECTORY NEW_OUTPUT_DIRECTORY" >&2
    exit 2
fi
source_root=$(cd "$(dirname "$0")/../.." && pwd)
build_dir=$(cd "$1" && pwd)
output_dir=$2
compiler=${CC:-x86_64-w64-mingw32-gcc}
import_lib="$build_dir/lib/.libs/libvlc.dll.a"
[[ -f "$import_lib" ]] || { echo "Missing validated build import library: $import_lib" >&2; exit 2; }
[[ ! -e "$output_dir" ]] || { echo 'Output directory must be new; refusing overwrite.' >&2; exit 2; }
"$compiler" -dumpmachine | grep -qx x86_64-w64-mingw32
mkdir -p "$output_dir"
"$compiler" -std=c11 -O2 -Wall -Wextra -Werror -municode \
    -D__USE_MINGW_ANSI_STDIO=1 -I"$source_root/include" -I"$build_dir/include" \
    "$source_root/prototypes/stage_2_3/thumbnail_helper.c" "$import_lib" \
    -o "$output_dir/thumbnail-helper.exe"
file "$output_dir/thumbnail-helper.exe"
x86_64-w64-mingw32-objdump -p "$output_dir/thumbnail-helper.exe" > "$output_dir/pe-imports.txt"
sha256sum "$output_dir/thumbnail-helper.exe" > "$output_dir/SHA256SUMS"
printf 'Standalone helper built. No VLC application targets were rebuilt.\n'
