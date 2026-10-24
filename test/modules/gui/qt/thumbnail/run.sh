#!/usr/bin/env bash
# GPL-2.0-or-later. Host Qt event-loop tests, not Windows GUI acceptance.
set -euo pipefail
[[ $# == 1 && ! -e $1 ]] || { echo 'usage: bash test/modules/gui/qt/thumbnail/run.sh NEW_OUTPUT'; exit 2; }
source_root=$(cd "$(dirname "$0")/../../../../.." && pwd)
mkdir -p -- "$1"
output=$(cd "$1" && pwd)
if [[ -n ${QT_DEV_ROOT:-} ]]; then
    # Cloud tests use matching official Debian dev headers with installed Qt5 libs.
    qt_include="$QT_DEV_ROOT/usr/include/x86_64-linux-gnu/qt5"
    qt_cflags=(-I"$qt_include" -I"$qt_include/QtCore" -I"$qt_include/QtGui" -I"$qt_include/QtWidgets")
    qt_libs=(-l:libQt5Widgets.so.5 -l:libQt5Gui.so.5 -l:libQt5Core.so.5)
else
    read -r -a qt_cflags <<< "$(pkg-config --cflags Qt5Widgets)"
    read -r -a qt_libs <<< "$(pkg-config --libs Qt5Widgets)"
fi
compiler=${CXX:-c++}
moc_tool=${MOC:-moc}
"$moc_tool" "${qt_cflags[@]}" "$source_root/modules/gui/qt/util/timetooltip.hpp" -o "$output/timetooltip.moc.cpp"
"$compiler" -std=c++11 -Wall -Wextra -Werror \
    "$source_root/test/modules/gui/qt/thumbnail/fake_helper.cpp" -o "$output/fake-helper"
"$compiler" -std=c++11 -Wall -Wextra -Werror -fPIC -I"$source_root" \
    -DQT_USE_QSTRINGBUILDER -DQT_NO_CAST_FROM_BYTEARRAY -DQT_NO_CAST_TO_ASCII \
    "${qt_cflags[@]}" "$source_root/modules/gui/qt/util/thumbnailpreview.cpp" \
    "$source_root/modules/gui/qt/util/timetooltip.cpp" \
    "$source_root/test/modules/gui/qt/thumbnail/controller_test.cpp" "$output/timetooltip.moc.cpp" "${qt_libs[@]}" -o "$output/controller-test"
VLC_VISUAL_TEST_OUTPUT="$output" QT_QPA_PLATFORM=offscreen "$output/controller-test" "$output/fake-helper"

if [[ -n ${REAL_PREVIEW_HELPER:-} && -n ${REAL_PREVIEW_MEDIA:-} ]]; then
    "$compiler" -std=c++11 -Wall -Wextra -Werror -fPIC -I"$source_root" \
        -DQT_USE_QSTRINGBUILDER -DQT_NO_CAST_FROM_BYTEARRAY -DQT_NO_CAST_TO_ASCII \
        "${qt_cflags[@]}" "$source_root/modules/gui/qt/util/thumbnailpreview.cpp" \
        "$source_root/test/modules/gui/qt/thumbnail/real_helper_test.cpp" "${qt_libs[@]}" -o "$output/real-helper-test"
    QT_QPA_PLATFORM=offscreen "$output/real-helper-test" "$REAL_PREVIEW_HELPER" "$REAL_PREVIEW_MEDIA" "$output/real-candidate.png"
fi
