#!/usr/bin/env bash
# GPL-2.0-or-later. CI-1 only: no publication, credentials, or legacy build paths.
set -euo pipefail
CI_DRIVER=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
readonly CI_DRIVER
readonly CI_STAGE=${VLC_CI_STAGE:-2.4.3}
case "$CI_STAGE" in
    2.4.3) CI_SOURCE_SHA=2f7cb820e893c8dc53e40cddbab9c57a2fe1a0fc ;;
    2.4.4) CI_SOURCE_SHA=e879ce656c218eddec13926f03b5f0615d14dc61 ;;
    2.4.6F) CI_SOURCE_SHA=ab3cd4518db560208e737a591ee7164ad6da4370 ;;
    2.4.6E) CI_SOURCE_SHA=e577355ed22aea9d2703aad794f79a68f0a1ce42 ;;
    2.4.6D) CI_SOURCE_SHA=12b25af9272193ddfd0c298e1e9f48c5ff5b66e4 ;;
    2.4.6C) CI_SOURCE_SHA=febfd332f4ffbc94981f0b1cac298dc941de057b ;;
    2.4.6B) CI_SOURCE_SHA=5cfebdb9718b3477e17c5ffcb6d14a2876974350 ;;
    2.4.5) CI_SOURCE_SHA=d10df7f0a54326f122f54b2a2961a3e1b0ee1f66 ;;
    *) echo 'Unsupported diagnostic stage' >&2; exit 2 ;;
esac
readonly CI_SOURCE_SHA
readonly CI_BENCHMARK_ONLY=${VLC_CI_BENCHMARK_ONLY:-false}
[[ $CI_BENCHMARK_ONLY == false || ( $CI_BENCHMARK_ONLY == true && $CI_STAGE == 2.4.5 ) ]] || exit 2
readonly CI_IMAGE=registry.videolan.org/vlc-debian-win64-3.0@sha256:b8e61c83d248cfb8fc3432d347f07f24de5a2ea6b0060cfe3053243e3674e6e7
readonly CI_CONTRIB=910cc9cada6080dd777fbafb2ebdd0c24f702c30
readonly CI_CONTRIB_SHA=49f7690b3d09760f842f9f8e9612c28de8cbb1d027bcb3f6aaa25767611eea8c
readonly CI_SDK_SHA=1ed59c09152e78aff84663fa76efab26057716576db74332858810e5ef54ae1f
readonly CI_SDK_URL=https://download.videolan.org/pub/videolan/vlc/3.0.24/win64/vlc-3.0.24-win64.7z

ci_die() { echo "CI-1: $*" >&2; return 2; }
ci_require_hosted() {
    [[ ${GITHUB_ACTIONS:-} == true && ${RUNNER_ENVIRONMENT:-} == github-hosted &&
       ${RUNNER_OS:-} == Linux && ${RUNNER_ARCH:-} == X64 && ${ImageOS:-} == ubuntu24 ]] ||
        { ci_die 'requires a GitHub-hosted Ubuntu 24.04 x64 runner'; return 2; }
}
ci_cleanup_allowed() {
    # Literal allowlist: never accept caller-selected cleanup roots.
    case "$1" in
        /usr/local/lib/android|/usr/share/dotnet|/opt/ghc|/usr/local/.ghcup) ;;
        *) ci_die "cleanup path is not allowlisted: $1"; return 2 ;;
    esac
}
ci_owned_work() {
    local work parent
    [[ ! -L $1 ]] || { ci_die 'symlinked output directory'; return 2; }
    [[ -n ${RUNNER_TEMP:-} && ${GITHUB_RUN_ID:-} =~ ^[0-9]+$ && ${GITHUB_RUN_ATTEMPT:-} =~ ^[0-9]+$ ]] ||
        { ci_die 'missing run ownership environment'; return 2; }
    work=$(realpath -m -- "$1") || return 2
    parent=$(realpath -- "$RUNNER_TEMP") || return 2
    [[ $(dirname -- "$work") == "$parent" && $(basename -- "$work") == "vlc-ci1-${GITHUB_RUN_ID:?}-${GITHUB_RUN_ATTEMPT:?}" &&
       -f "$work/.ci1-work" && ! -L "$work/.ci1-work" && ! -L "$work" ]] || { ci_die 'not an owned CI-1 work directory'; return 2; }
}
ci_space() {
    local available=$1 minimum=$2
    [[ $available =~ ^[0-9]+$ && $minimum =~ ^[0-9]+$ ]] || { ci_die 'invalid disk measurement'; return 2; }
    (( available >= minimum )) || { ci_die "insufficient free bytes: $available (need $minimum)"; return 2; }
}
ci_source() {
    local source=$1
    [[ $(git -C "$source" rev-parse HEAD) == "$CI_SOURCE_SHA" ]] || { ci_die 'unexpected application source commit'; return 2; }
    [[ -z $(git -C "$source" status --porcelain) ]] || { ci_die 'application checkout must be clean'; return 2; }
    [[ ! -e "$source/win64" && ! -e "$source/contrib/contrib-win64" &&
       ! -e "$source/contrib/x86_64-w64-mingw32" ]] || { ci_die 'refusing existing build directories'; return 2; }
    [[ $(cd "$source" && bash extras/ci/get-contrib-sha.sh win64) == "$CI_CONTRIB" ]] ||
        { ci_die 'contrib identity mismatch (full Git history is required)'; return 2; }
}
ci_make_guard() {
    # Only root contrib invocations are gated. Native .luac recursion is expected.
    if [[ $(pwd -P) == "${CI_CONTRIB_DIR:?}" ]]; then
        [[ ! -e "${CI_GUARD_STATE:?}/prebuilt.failed" ]] || { ci_die 'previous prebuilt failure; source fallback forbidden'; return 86; }
        case "${1:-}" in
            prebuilt)
                [[ $# == 2 && $2 == "PREBUILT_URL=${VLC_PREBUILT_CONTRIBS_URL:?}" ]] ||
                    { ci_die 'unexpected prebuilt invocation'; return 86; }
                if "${CI_REAL_MAKE:?}" "$@"; then
                    touch "$CI_GUARD_STATE/prebuilt.ok"
                    return 0
                else
                    local status=$?
                    touch "$CI_GUARD_STATE/prebuilt.failed"
                    return "$status"
                fi ;;
            .luac|list)
                [[ $# == 1 && -f "$CI_GUARD_STATE/prebuilt.ok" ]] ||
                    { ci_die 'contrib preparation without verified prebuilt'; return 86; } ;;
            *) ci_die 'unexpected contrib source-build/fetch fallback rejected'; return 86 ;;
        esac
    fi
    "${CI_REAL_MAKE:?}" "$@"
}
ci_sha() { echo "$2  $1" | sha256sum --check --strict; }
ci_phase() {
    CI_PHASE=$1
    echo "$CI_PHASE" > "$CI_WORK/current-phase"
    python3 "$CI_DRIVER/package-diagnostic.py" snapshot "$CI_WORK/metrics/events.jsonl" --phase "$CI_PHASE" --disk "$CI_WORK"
    echo "CI-1 phase: $CI_PHASE"
}
ci_prepare_host() {
    ci_require_hosted || return 2
    ci_owned_work "$CI_WORK" || return 2
    local path workspace docker_root
    workspace=$(realpath -- "${GITHUB_WORKSPACE:?}")
    docker_root=$(docker info --format '{{.DockerRootDir}}')
    [[ $(stat -c %d "$CI_WORK") == $(sudo stat -c %d "$docker_root") ]] ||
        { ci_die 'workspace and Docker storage must share the measured filesystem for this pilot'; return 2; }
    for path in /usr/local/lib/android /usr/share/dotnet /opt/ghc /usr/local/.ghcup; do
        ci_cleanup_allowed "$path" || return 2
        [[ ! -L $path ]] || { ci_die 'symlinked SDK cleanup path'; return 2; }
        [[ -d $path ]] || continue
        [[ $(realpath -- "$path") == "$path" ]] || { ci_die 'aliased SDK cleanup path'; return 2; }
        case "$workspace/" in "$path/"*) ci_die 'cleanup overlaps checkout'; return 2 ;; esac
        case "$CI_WORK/" in "$path/"*) ci_die 'cleanup overlaps CI output'; return 2 ;; esac
        case "$docker_root/" in "$path/"*) ci_die 'cleanup overlaps Docker storage'; return 2 ;; esac
        if mountpoint -q "$path"; then ci_die 'SDK cleanup path is a mountpoint'; return 2; fi
        echo "Removing allowlisted preinstalled SDK: $path"
        sudo rm -rf --one-file-system -- "$path"
    done
    sudo apt-get update
    sudo apt-get install -y --no-install-recommends qtbase5-dev qtbase5-dev-tools ffmpeg fonts-dejavu-core p7zip-full
    local available
    available=$(df -B1 --output=avail "$CI_WORK" | tail -1 | tr -d ' ')
    ci_space "$available" "$((35 * 1024 * 1024 * 1024))"
    local memory_bytes
    memory_bytes=$(awk '/^MemTotal:/ {printf "%.0f", $2 * 1024}' /proc/meminfo)
    (( memory_bytes >= 14 * 1024 * 1024 * 1024 )) || { ci_die "runner needs at least 14 GiB RAM; measured $memory_bytes bytes"; return 2; }
    docker info --format 'Docker storage: {{.Driver}}' | tee "$CI_WORK/logs/docker-storage.txt"
    [[ $(docker info --format '{{.Driver}}') != vfs ]] || { ci_die 'vfs image copying is unsupported'; return 2; }
}
ci_inside_owned() {
    [[ ${CI_INSIDE:-} == 1 && $1 == /out && -d /out && ! -L /out &&
       -f /out/.ci1-work && ! -L /out/.ci1-work ]] ||
        { ci_die 'not an owned container output'; return 2; }
}
ci_finalize_guard() {
    if [[ ${CI_INSIDE:-} == 1 ]]; then
        ci_inside_owned "$1" || return 2
    else
        ci_require_hosted || return 2
        ci_owned_work "$1" || return 2
    fi
}
ci_repair_work() {
    ci_finalize_guard "$1" || return 2
    local work=$1 path
    for path in "$work/logs" "$work/metrics"; do
        [[ ! -L $path && ( ! -e $path || -d $path ) ]] ||
            { ci_die 'unsafe reporting directory'; return 2; }
    done
    [[ ! -L $work/container.id && ( ! -e $work/container.id || -f $work/container.id ) ]] ||
        { ci_die 'unsafe container identity file'; return 2; }
    # Never follow evidence-file symlinks, even in an otherwise owned directory.
    for path in "$work/metrics/"* "$work/metrics/".[!.]* "$work/metrics/"..?*; do
        [[ ! -L $path && ( ! -e $path || -f $path ) ]] ||
            { ci_die 'unsafe reporting file'; return 2; }
    done
    mkdir -p -- "$work/logs" "$work/metrics" || return 1
}
ci_console_fallback() {
    ci_finalize_guard "$1" || return 2
    echo 'CI-1: reporting incomplete/unavailable; console-only resource fallback' >&2
    timeout --kill-after=2s 5s df -h -- "$1" || true
    timeout --kill-after=2s 5s free -m || true
}
ci_finish() {
    # Explicit returns are essential: EXIT calls this with errexit disabled.
    ci_finalize_guard "$1" || return 2
    local work
    work=$(realpath -- "$1") || return 2
    ci_repair_work "$work" || {
        local repair_status=$?
        [[ $repair_status != 2 ]] || return 2
        ci_console_fallback "$work"
        return 1
    }
    local scope=host
    [[ ${CI_INSIDE:-} != 1 ]] || scope=container
    local args=(finalize "$work" --scope "$scope" --phase "${3:-before-driver}")
    [[ -z ${2:-} ]] || args+=(--status "$2")
    [[ -z ${CI_MONITOR_PID:-} ]] || args+=(--monitor-pid "$CI_MONITOR_PID")
    timeout --kill-after=5s 70s python3 "$CI_DRIVER/package-diagnostic.py" "${args[@]}" || {
        local status=$?
        echo "CI-1: teardown failed separately (status $status); build status is unchanged" >&2
        ci_console_fallback "$work"
        return "$status"
    }
}
ci_exit() {
    local status=$1
    trap - EXIT
    set +e
    ci_finish "$CI_WORK" "$status" "${CI_PHASE:-initialization}"
    exit "$status"
}
ci_inside_exit() {
    local status=$1
    trap - EXIT
    set +e
    ci_finish /out "$status" "${CI_PHASE:-container}"
    exit "$status"
}
ci_finish_entry() {
    ci_require_hosted || return 2
    if [[ ! -e $1 && ! -L $1 ]]; then
        # No build ran (for example, source checkout failed).
        ci_new_work "$1" || { ci_finish "$1"; return 1; }
    fi
    ci_finish "$1"
}
ci_inside() {
    ci_inside_owned /out || return 2
    CI_WORK=/out
    trap 'ci_inside_exit "$?"' EXIT
    python3 /ci/package-diagnostic.py monitor /out/metrics/container.jsonl --stop /out/metrics/container.stop --disk /vlc /out &
    CI_MONITOR_PID=$!
    ci_source /vlc
    export PATH=/out/guard/bin:/opt/gcc-x86_64-w64-mingw32/bin:/opt/protobuf/bin:/usr/local/bin:/usr/bin:/bin
    export CI_REAL_MAKE=/usr/bin/make CI_CONTRIB_DIR=/vlc/contrib/contrib-win64 CI_GUARD_STATE=/out/guard
    mkdir -p /out/guard/bin
    printf '#!/usr/bin/env bash\nexec bash /ci/build-diagnostic.sh make-guard "$@"\n' > /out/guard/bin/make
    cat > /out/guard/bin/getconf <<'GETCONF'
#!/bin/sh
if [ "$#" = 1 ] && [ "$1" = _NPROCESSORS_ONLN ]; then echo 4; else exec /usr/bin/getconf "$@"; fi
GETCONF
    chmod +x /out/guard/bin/make /out/guard/bin/getconf
    [[ $(x86_64-w64-mingw32-gcc -dumpmachine) == x86_64-w64-mingw32 ]]
    [[ $(x86_64-w64-mingw32-gcc -dumpversion) == 6.4.0 ]]
    x86_64-w64-mingw32-ld --version | head -1 | grep -F '2.27'
    { x86_64-w64-mingw32-gcc --version; x86_64-w64-mingw32-ld --version; wine --version; } > /out/logs/toolchain.txt
    ci_sha /out/downloads/contrib.tar.zst "$CI_CONTRIB_SHA"
    ci_sha /out/downloads/sdk.7z "$CI_SDK_SHA"
    export VLC_PREBUILT_CONTRIBS_URL=file:///out/downloads/contrib.tar.zst
    export WINEPREFIX=/out/wine-prefix WINEARCH=win64 WINEDEBUG=-all XDG_CACHE_HOME=/out/cache
    export HOME=/out/home
    mkdir -m 700 "$WINEPREFIX" "$HOME"
    cd /vlc
    ci_phase helper-build
    mkdir /out/sdk
    7z x -y -o/out/sdk /out/downloads/sdk.7z > /out/logs/sdk-extraction.txt
    local sdk=/out/sdk/vlc-3.0.24
    [[ -f $sdk/sdk/lib/libvlc.lib && -f $sdk/libvlc.dll && -d $sdk/plugins ]]
    x86_64-w64-mingw32-gcc -std=c11 -O2 -Wall -Wextra -Werror -municode -D__USE_MINGW_ANSI_STDIO=1 \
        -I"$sdk/sdk/include" /vlc/prototypes/stage_2_3/thumbnail_helper.c "$sdk/sdk/lib/libvlc.lib" -o "$sdk/thumbnail-helper.exe"
    if [[ $CI_STAGE == 2.4.5 ]]; then
        ci_phase helper-benchmark
        # Exclude one-time Wine prefix creation from Windows process timings.
        timeout --kill-after=5s 60s wine cmd /c exit 0 > /out/logs/wine-prefix-init.txt 2>&1
        python3 /vlc/prototypes/stage_2_3/benchmark.py --helper "$sdk/thumbnail-helper.exe" --fixtures /out/fixtures \
            --output /out/logs/performance-phase-a.json --wine --github-notice --all-fixtures --profiles baseline --persistent
    fi
    [[ $CI_BENCHMARK_ONLY == false ]] || return 0
    if [[ $CI_BENCHMARK_ONLY == false ]]; then
    # .luac is an expected native tool, even with prebuilt target libraries.
    mkdir -p contrib/tarballs
    curl --fail --location --proto '=https' --proto-redir '=https' --output contrib/tarballs/lua-5.1.5.tar.gz https://www.lua.org/ftp/lua-5.1.5.tar.gz
    (cd contrib/tarballs && sha512sum --check /vlc/contrib/src/lua/SHA512SUMS)
    ci_phase vlc-build
    python3 /ci/package-diagnostic.py timed /out/metrics/vlc-time.json -- ./extras/package/win32/build.sh -a x86_64 -p
    [[ -f /out/guard/prebuilt.ok && ! -e /out/guard/prebuilt.failed ]]
    grep -Eq 'QT_VERSION_STR[[:space:]]+"5\.6\.3"' contrib/x86_64-w64-mingw32/include/QtCore/qconfig.h
    ci_phase upstream-package
    python3 /ci/package-diagnostic.py timed /out/metrics/package-time.json -- /usr/bin/make -C win64 package-win32-debug-zip
    unzip -t win64/vlc-3.0.24-win64-debug.zip > /out/logs/upstream-zip-test.txt
    grep -a -q 'timeline-diagnostic slider=' win64/vlc-3.0.24/plugins/gui/libqt_plugin.dll
    fi
    for binary in win64/vlc-3.0.24/vlc.exe win64/vlc-3.0.24/plugins/gui/libqt_plugin.dll "$sdk/thumbnail-helper.exe"; do
        x86_64-w64-mingw32-objdump -f "$binary"
        x86_64-w64-mingw32-objdump -p "$binary"
    done > /out/logs/pe-inspection.txt
    ci_phase wine-smoke
    # Probe the fresh full package, never a previous application's plugin cache.
    (cd win64/vlc-3.0.24 && timeout --kill-after=5s 60s wine ./vlc-cache-gen.exe "Z:\\vlc\\win64\\vlc-3.0.24\\plugins")
    grep -a -q 'Qt interface' win64/vlc-3.0.24/plugins/plugins.dat
    python3 /ci/package-diagnostic.py smoke --runtime "$sdk" --fixtures /out/fixtures --output /out/logs/helper-smoke
    if [[ $CI_STAGE == 2.4.6B || $CI_STAGE == 2.4.6C || $CI_STAGE == 2.4.6D || $CI_STAGE == 2.4.6E || $CI_STAGE == 2.4.6F ]]; then
        ci_phase mpv-worker-build
        ci_sha /out/downloads/mpv.7z efc719eff3dc5954b4a5efc800632808bcec01cc9034940dd14e5eb7fe9ea633
        mkdir /out/mpv-sdk
        7z x -y -o/out/mpv-sdk /out/downloads/mpv.7z > /out/logs/mpv-sdk-extraction.txt
        bash /vlc/prototypes/stage_2_4_6B/build.sh /out/mpv-sdk /out/mpv-worker
        if [[ $CI_STAGE == 2.4.6C || $CI_STAGE == 2.4.6D || $CI_STAGE == 2.4.6E || $CI_STAGE == 2.4.6F ]]; then
            bash /vlc/prototypes/stage_2_4_6C/build.sh /out/mpv-sdk /out/mpv-accurate
        fi
        x86_64-w64-mingw32-objdump -p /out/mpv-worker/mpv-thumbnail-worker.exe > /out/logs/mpv-pe.txt
    fi
    ci_phase compose
    bash extras/package/thumbnail-preview.sh /vlc/win64/vlc-3.0.24 "$sdk" /out/staging
    if [[ $CI_STAGE == 2.4.6B || $CI_STAGE == 2.4.6C || $CI_STAGE == 2.4.6D || $CI_STAGE == 2.4.6E || $CI_STAGE == 2.4.6F ]]; then
        mkdir /out/staging/thumbnail-mpv
        cp /out/mpv-worker/mpv-thumbnail-worker.exe /out/staging/thumbnail-mpv/
        cp /vlc/prototypes/stage_2_4_6B/{Get-Runtime,Setup-Runtime,Start-Preview}.ps1 /out/staging/thumbnail-mpv/
        cp /vlc/prototypes/stage_2_4_6B/{dependencies.json,THIRD_PARTY.md} /out/staging/thumbnail-mpv/
        cp /vlc/docs/STAGE_2_4_6B_{INTEGRATION,RESULTS}.md /out/staging/thumbnail-mpv/
    fi
    if [[ $CI_STAGE == 2.4.6C || $CI_STAGE == 2.4.6D || $CI_STAGE == 2.4.6E || $CI_STAGE == 2.4.6F ]]; then
        cp /out/mpv-accurate/mpv-thumbnail-accurate-worker.exe /out/staging/thumbnail-mpv/
        cp /vlc/prototypes/stage_2_4_6C/{Start-Preview,Setup-Runtime}.ps1 /out/staging/thumbnail-mpv/
        cp /vlc/docs/STAGE_2_4_6C_ACCURACY.md /out/staging/thumbnail-mpv/
    fi
    if [[ $CI_STAGE == 2.4.6D || $CI_STAGE == 2.4.6E || $CI_STAGE == 2.4.6F ]]; then
        cp /vlc/prototypes/stage_2_4_6D/Start-Preview.ps1 /out/staging/thumbnail-mpv/
        cp /vlc/docs/STAGE_2_4_6D_STABILITY.md /out/staging/thumbnail-mpv/
    fi
    if [[ $CI_STAGE == 2.4.6E || $CI_STAGE == 2.4.6F ]]; then
        cp /vlc/docs/STAGE_2_4_6E_VISUAL_POLISH.md /out/staging/thumbnail-mpv/
    fi
    if [[ $CI_STAGE == 2.4.6F ]]; then
        mkdir /out/staging/portable
        x86_64-w64-mingw32-gcc -std=c11 -O2 -Wall -Wextra -Werror -municode -mwindows \
            /vlc/extras/package/portable/launcher.c -luser32 -o '/out/staging/Start VLC Timeline Preview.exe'
        cp /vlc/extras/package/portable/{Start-Portable.ps1,DEPENDENCIES.md} /out/staging/portable/
        cp /vlc/extras/package/portable/README.txt /out/staging/README.txt
        cp /vlc/docs/STAGE_2_4_6F_PORTABLE.md /out/staging/portable/
    fi
    # Internal metadata avoids circular ZIP checksums; SDK runtime stays separate.
    python3 /ci/package-diagnostic.py package /out/staging /out/delivery --metadata /out/build-inputs.json
}
ci_new_work() {
    local work
    work=$(realpath -m -- "$1")
    [[ ! -L $1 && $(dirname -- "$work") == $(realpath -- "${RUNNER_TEMP:?}") &&
       $(basename -- "$work") == "vlc-ci1-${GITHUB_RUN_ID:?}-${GITHUB_RUN_ATTEMPT:?}" && ! -e $work ]] ||
        { ci_die 'output must be a new run-specific directory directly in RUNNER_TEMP'; return 2; }
    CI_WORK=$work
    mkdir -- "$work" || return 1
    touch "$work/.ci1-work" || return 1
    mkdir "$work/logs" "$work/metrics" "$work/downloads" "$work/guard" || return 1
}
ci_run() {
    ci_require_hosted || return 2
    local source work driver_sha
    CI_WORK=$2
    CI_PHASE=initialization
    trap 'ci_exit "$?"' EXIT
    ci_new_work "$2" || return "$?"
    work=$CI_WORK
    source=$(realpath -- "$1") || return "$?"
    exec > >(tee "$work/logs/driver.log") 2>&1
    python3 "$CI_DRIVER/package-diagnostic.py" monitor "$work/metrics/host.jsonl" --stop "$work/metrics/host.stop" --disk "$work" &
    CI_MONITOR_PID=$!
    ci_phase source-verification
    ci_source "$source"
    driver_sha=$(git -C "$CI_DRIVER" rev-parse HEAD)
    python3 "$CI_DRIVER/package-diagnostic.py" inputs "$work/build-inputs.json" --source "$source" --driver "$driver_sha" --image "$CI_IMAGE" \
        --contrib "$CI_CONTRIB" --contrib-sha "$CI_CONTRIB_SHA" --sdk-sha "$CI_SDK_SHA"
    ci_phase safety-tests
    bash "$CI_DRIVER/../../test/ci/windows-diagnostic-guards.sh"
    ci_phase host-preparation
    ci_prepare_host
    ci_phase host-qt-tests
    bash "$source/test/modules/gui/qt/run.sh" "$work/hover-tests"
    bash "$source/test/modules/gui/qt/thumbnail/run.sh" "$work/qt-tests"
    if [[ $CI_STAGE == 2.4.6B || $CI_STAGE == 2.4.6C || $CI_STAGE == 2.4.6D || $CI_STAGE == 2.4.6E || $CI_STAGE == 2.4.6F ]]; then
        PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s "$source/prototypes/stage_2_4_6B" -p test_protocol.py -v
    fi
    ci_phase dependencies
    curl --fail --location --proto '=https' --proto-redir '=https' --output "$work/downloads/contrib.tar.zst" \
        "https://artifacts.videolan.org/vlc-3.0/win64/vlc-contrib-x86_64-w64-mingw32-$CI_CONTRIB.tar.zst"
    ci_sha "$work/downloads/contrib.tar.zst" "$CI_CONTRIB_SHA"
    curl --fail --location --proto '=https' --proto-redir '=https' --output "$work/downloads/sdk.7z" "$CI_SDK_URL"
    ci_sha "$work/downloads/sdk.7z" "$CI_SDK_SHA"
    if [[ $CI_STAGE == 2.4.6B || $CI_STAGE == 2.4.6C || $CI_STAGE == 2.4.6D || $CI_STAGE == 2.4.6E || $CI_STAGE == 2.4.6F ]]; then
        curl --fail --location --proto '=https' --proto-redir '=https' --output "$work/downloads/mpv.7z" \
            https://github.com/shinchiro/mpv-winbuild-cmake/releases/download/20261021/mpv-dev-x86_64-20261021-git-e471496c56.7z
        ci_sha "$work/downloads/mpv.7z" efc719eff3dc5954b4a5efc800632808bcec01cc9034940dd14e5eb7fe9ea633
    fi
    ci_phase image-pull
    ci_space "$(df -B1 --output=avail "$CI_WORK" | tail -1 | tr -d ' ')" "$((35 * 1024 * 1024 * 1024))"
    docker pull --platform linux/amd64 "$CI_IMAGE"
    [[ $(docker image inspect --format '{{.Os}}/{{.Architecture}}' "$CI_IMAGE") == linux/amd64 ]]
    docker image inspect --format '{{.Id}} {{json .RepoDigests}}' "$CI_IMAGE" > "$work/logs/image.txt"
    ci_phase fixtures
    python3 "$source/prototypes/stage_2_3/generate_fixtures.py" "$work/fixtures" --font /usr/share/fonts/truetype/dejavu/DejaVuSans.ttf
    if [[ $CI_STAGE == 2.4.6C || $CI_STAGE == 2.4.6D || $CI_STAGE == 2.4.6E || $CI_STAGE == 2.4.6F ]]; then
        PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s "$source/prototypes/stage_2_4_6C" -p test_accuracy.py -v
        python3 "$source/prototypes/stage_2_4_6C/generate_extra.py" "$work/fixtures" --font /usr/share/fonts/truetype/dejavu/DejaVuSans.ttf
    fi
    ci_phase container
    timeout --signal=TERM --kill-after=30s 90m docker run --name "vlc-ci1-${GITHUB_RUN_ID}-${GITHUB_RUN_ATTEMPT}" \
        --cidfile "$work/container.id" --cpus=4 --memory=12g --pids-limit=1024 --user "$(id -u):$(id -g)" \
        --mount "type=bind,src=$source,dst=/vlc" --mount "type=bind,src=$CI_DRIVER,dst=/ci,readonly" \
        --mount "type=bind,src=$work,dst=/out" --env CI_INSIDE=1 --env "VLC_CI_STAGE=$CI_STAGE" --env "VLC_CI_BENCHMARK_ONLY=$CI_BENCHMARK_ONLY" --workdir /vlc "$CI_IMAGE" \
        bash /ci/build-diagnostic.sh inside
    [[ $CI_BENCHMARK_ONLY == false ]] || return 0
    ci_phase final-verification
    python3 "$CI_DRIVER/package-diagnostic.py" verify "$work/delivery"
}

if [[ ${BASH_SOURCE[0]} == "$0" ]]; then
    case "${1:-}" in
        run) [[ $# == 3 ]]; ci_run "$2" "$3" ;;
        inside) [[ $# == 1 ]]; ci_inside ;;
        make-guard) shift; ci_make_guard "$@" ;;
        finish) [[ $# == 2 ]]; ci_finish_entry "$2" ;;
        *) ci_die 'usage: build-diagnostic.sh run SOURCE NEW_WORK | finish WORK (inside/make-guard are internal)' ;;
    esac
fi
