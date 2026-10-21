#!/usr/bin/env bash
# GPL-2.0-or-later. Compose NEW portable output; never update either input.
set -euo pipefail
if [[ $# != 3 ]]; then
    echo 'usage: bash package-thumbnail-preview.sh MODIFIED_VLC_PORTABLE VALIDATED_STAGE23_PORTABLE NEW_OUTPUT' >&2
    exit 2
fi
app=$(cd "$1" && pwd -P)
helper=$(cd "$2" && pwd -P)
output=$3
[[ ! -e $output ]] || { echo 'Output must be new.' >&2; exit 2; }
for input in "$app/vlc.exe" "$app/plugins/gui/libqt_plugin.dll" \
             "$helper/thumbnail-helper.exe" "$helper/libvlc.dll" "$helper/libvlccore.dll"; do
    [[ -f $input ]] || { echo "Missing $input" >&2; exit 2; }
    file -b "$input" | grep -q 'PE32+' || { echo "Expected Windows x64 binary: $input" >&2; exit 2; }
done
[[ "$app" != "$helper" && -d "$helper/plugins" && ! -e "$app/thumbnail-preview" && ! -e "$helper/thumbnail-preview" ]] || {
    echo 'Expected isolated input packages without recursive preview bundles.' >&2; exit 2;
}
grep -a -q 'timeline-preview:' "$app/plugins/gui/libqt_plugin.dll" || {
    echo 'Qt plugin does not contain the Stage 2.4.2 marker.' >&2; exit 2;
}
# Prefer a sibling directory outside both inputs. Copying onto/inside an input
# could recursively modify a validated runtime.
output_parent=$(cd "$(dirname "$output")" && pwd -P)
case "$output_parent/" in "$app/"*|"$helper/"*) echo 'Output cannot be inside an input.' >&2; exit 2;; esac
mkdir -- "$output"
cp -a "$app/." "$output/"
mkdir -- "$output/thumbnail-preview"
cp -a "$helper/." "$output/thumbnail-preview/"
# Hashes capture BOTH runtime sets; source provenance still requires the build log.
(cd "$output" && sha256sum vlc.exe plugins/gui/libqt_plugin.dll \
    thumbnail-preview/thumbnail-helper.exe thumbnail-preview/libvlc.dll \
    thumbnail-preview/libvlccore.dll > STAGE_2_4_2_SHA256SUMS)
echo "Isolated preview bundle: $output"
