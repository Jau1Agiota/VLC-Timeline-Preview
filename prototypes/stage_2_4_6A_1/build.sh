#!/usr/bin/env bash
# GPL-2.0-or-later. Standalone; never invokes VLC build targets.
set -euo pipefail
[[ $# == 2 && ! -e $2 ]] || { echo 'usage: build.sh VERIFIED_SDK NEW_OUTPUT'; exit 2; }
root=$(cd -- "$(dirname -- "$0")" && pwd -P)
sdk=$(cd -- "$1" && pwd -P)
compiler=${CC:-x86_64-w64-mingw32-gcc}
[[ $("$compiler" -dumpmachine) == x86_64-w64-mingw32 ]] || exit 2
"${PYTHON:-python3}" - "$root/dependencies.json" "$sdk/libmpv-2.dll" <<'PY'
import hashlib,json,sys
from pathlib import Path
assert hashlib.sha256(Path(sys.argv[2]).read_bytes()).hexdigest()==json.loads(Path(sys.argv[1]).read_text())['dll_sha256']
PY
mkdir -- "$2"
"$compiler" -std=c11 -O2 -Wall -Wextra -Werror -municode -D__USE_MINGW_ANSI_STDIO=1 \
    -I"$sdk/include" "$root/preview.c" "$sdk/libmpv.dll.a" -lopengl32 -lgdi32 -luser32 -lpsapi -o "$2/mpv-preview.exe"
"$compiler" --version > "$2/compiler.txt"
