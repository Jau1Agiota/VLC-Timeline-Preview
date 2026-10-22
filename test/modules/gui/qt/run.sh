#!/usr/bin/env bash
# GPL-2.0-or-later. Independent mapping tests, not a VLC application build.
set -euo pipefail
if [[ $# != 1 || -e $1 ]]; then
    echo 'usage: bash test/modules/gui/qt/run.sh NEW_OUTPUT_DIRECTORY' >&2
    exit 2
fi
source_root=$(cd "$(dirname "$0")/../../../.." && pwd)
mkdir -p "$1"
output_dir=$(cd "$1" && pwd)
"${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror -pedantic -I"$source_root" \
    "$source_root/test/modules/gui/qt/seekhover.cpp" -o "$output_dir/seekhover-test"
"$output_dir/seekhover-test"
if [[ -n ${MINGW_CXX:-} ]]; then
    "$MINGW_CXX" --version > "$output_dir/mingw-version.txt"
    "$MINGW_CXX" -std=c++11 -Wall -Wextra -Werror -pedantic -static \
        -I"$source_root" "$source_root/test/modules/gui/qt/seekhover.cpp" \
        -o "$output_dir/seekhover-test.exe"
    echo 'Windows mapping-test executable compiled; native execution is separate.'
fi
