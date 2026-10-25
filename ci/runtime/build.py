#!/usr/bin/env python3
"""Isolated Windows runtime experiment; never writes an existing build directory."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tarfile
import time
import urllib.request

HERE = Path(__file__).resolve().parent


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1048576), b''):
            h.update(block)
    return h.hexdigest()


def fetch(row, output, cache):
    cached = cache / row['filename'] if cache else None
    if cached and cached.is_file():
        shutil.copyfile(cached, output)
    else:
        if not row['url'].startswith('https://'):
            raise ValueError('HTTPS input required')
        with urllib.request.urlopen(row['url'], timeout=90) as response, output.open('xb') as stream:
            shutil.copyfileobj(response, stream)
    if sha(output) != row['sha256']:
        raise ValueError('input checksum mismatch: ' + row['filename'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('new_root')
    parser.add_argument('--download-cache', type=Path)
    parser.add_argument('--jobs', type=int, default=4)
    args = parser.parse_args()
    root = Path(args.new_root).absolute()
    if root.exists() or root.is_symlink() or any(c.isspace() for c in str(root)):
        parser.error('new_root must not exist and must have no whitespace')
    if not 1 <= args.jobs <= 8:
        parser.error('jobs must be between 1 and 8')
    lock = json.loads((HERE / 'inputs.lock.json').read_text())
    if platform.python_version() != lock['python_version']:
        parser.error('bootstrap Python version must match lock: ' + lock['python_version'])
    root.mkdir(parents=False)
    start = time.monotonic()
    for name in ['downloads', 'src', 'tools', 'build', 'prefix', 'logs']:
        (root / name).mkdir()
    shutil.copyfile(HERE / 'inputs.lock.json', root / 'inputs.lock.json')
    commands = []
    env = dict(os.environ)
    for key in ['CFLAGS', 'CXXFLAGS', 'CPPFLAGS', 'LDFLAGS', 'CC', 'CXX', 'AR',
                'PKG_CONFIG_PATH', 'CMAKE_PREFIX_PATH', 'PKG_CONFIG_SYSROOT_DIR',
                'CPATH', 'C_INCLUDE_PATH', 'CPLUS_INCLUDE_PATH', 'LIBRARY_PATH',
                'CMAKE_LIBRARY_PATH', 'CMAKE_INCLUDE_PATH']:
        env.pop(key, None)

    def run(name, command, cwd=None):
        commands.append(dict(name=name, command=list(map(str, command)), cwd=str(cwd or root)))
        (root / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
        with (root / 'logs' / (name + '.log')).open('ab') as stream:
            subprocess.run(list(map(str, command)), cwd=cwd or root, env=env,
                           stdout=stream, stderr=subprocess.STDOUT, check=True, timeout=1800)

    try:
        for name, row in lock['inputs'].items():
            dest = root / 'downloads' / row['filename']
            fetch(row, dest, args.download_cache)
            if row['kind'] in ['source', 'toolchain']:
                parent = root / ('tools' if row['kind'] == 'toolchain' else 'src')
                with tarfile.open(dest) as archive:
                    top = archive.getmembers()[0].name.split('/')[0]
                    archive.extractall(parent, filter='data')
                if row['kind'] == 'source':
                    (parent / top).rename(parent / name)
        wheels = [root / 'downloads' / row['filename'] for row in lock['inputs'].values() if row['kind'] == 'wheel']
        run('python-tools', [sys.executable, '-m', 'pip', 'install', '--no-index', '--no-deps',
                             '--target', root / 'tools/python', *wheels])
        tc = root / 'tools/llvm-mingw-20251021-ucrt-ubuntu-22.04-x86_64'
        env.update(PYTHONPATH=str(root / 'tools/python'),
                   PATH=str(root / 'tools/python/bin') + ':' + str(tc / 'bin') + ':' + env['PATH'])
        meson = [sys.executable, '-m', 'mesonbuild.mesonmain']
        run('pkgconf', [*meson, 'setup', root / 'build/pkgconf', root / 'src/pkgconf',
                        '--prefix', root / 'tools/native', '--buildtype', 'release',
                        '--default-library', 'static', '--wrap-mode', 'nofallback'])
        run('pkgconf', [*meson, 'compile', '-C', root / 'build/pkgconf', '-j', args.jobs])
        run('pkgconf', [*meson, 'install', '-C', root / 'build/pkgconf'])
        pkg = root / 'tools/native/bin/pkgconf'
        prefix = root / 'prefix'
        env.update(PKG_CONFIG=str(pkg), PKG_CONFIG_PATH='',
                   PKG_CONFIG_LIBDIR=f'{prefix}/lib/pkgconfig:{prefix}/share/pkgconfig',
                   CMAKE_PREFIX_PATH=str(prefix))
        bins = dict(c='x86_64-w64-mingw32-clang', cpp='x86_64-w64-mingw32-clang++',
                    ar='llvm-ar', strip='llvm-strip', windres='x86_64-w64-mingw32-windres')
        text = '[binaries]\n' + ''.join(f'{key} = {str(tc / "bin" / value)!r}\n' for key, value in bins.items())
        text += f'pkg-config = {str(pkg)!r}\n'
        text += "[host_machine]\nsystem = 'windows'\ncpu_family = 'x86_64'\ncpu = 'x86_64'\nendian = 'little'\n[properties]\nneeds_exe_wrapper = true\n"
        text += f"[built-in options]\nc_args = ['-I{prefix}/include']\ncpp_args = ['-I{prefix}/include']\ncpp_link_args = ['-static-libstdc++']\n"
        (root / 'cross.ini').write_text(text)
        shutil.copytree(root / 'src/vulkan-headers/include', prefix / 'include', dirs_exist_ok=True)
        for name in ['fast_float', 'glad']:
            shutil.copytree(root / 'src' / name, root / 'src/libplacebo/3rdparty' / name, dirs_exist_ok=True)

        def build(name, options, shared=False):
            run(name, [*meson, 'setup', root / 'build' / name, root / 'src' / name,
                       '--cross-file', root / 'cross.ini', '--prefix', prefix, '--libdir', 'lib',
                       '--buildtype', 'release', '--default-library', 'shared' if shared else 'static',
                       '--wrap-mode', 'nofallback', *options])
            run(name, [*meson, 'compile', '-C', root / 'build' / name, '-j', args.jobs])
            run(name, [*meson, 'install', '-C', root / 'build' / name])

        build('freetype', ['--auto-features', 'disabled', '-Dzlib=disabled'])
        build('fribidi', ['-Ddocs=false', '-Dbin=false', '-Dtests=false'])
        build('harfbuzz', ['--auto-features', 'disabled', '-Dfreetype=enabled', '-Dtests=disabled', '-Dutilities=disabled'])
        build('libass', ['--auto-features', 'disabled', '-Ddirectwrite=enabled'])
        build('libplacebo', ['--auto-features', 'disabled', '-Dvulkan=disabled', '-Dvk-proc-addr=disabled',
                            '-Dopengl=disabled', '-Dgl-proc-addr=disabled', '-Dd3d11=disabled',
                            '-Dshaderc=disabled', '-Dglslang=disabled', '-Ddemos=false', '-Dtests=false'])
        ffbuild = root / 'build/ffmpeg'
        ffbuild.mkdir()
        run('ffmpeg', [root / 'src/ffmpeg/configure', f'--prefix={prefix}', f'--pkg-config={pkg}',
                       '--pkg-config-flags=--static', f'--cross-prefix={tc}/bin/x86_64-w64-mingw32-',
                       f'--cc={tc}/bin/x86_64-w64-mingw32-clang', f'--cxx={tc}/bin/x86_64-w64-mingw32-clang++',
                       f'--ar={tc}/bin/llvm-ar', f'--ranlib={tc}/bin/llvm-ranlib', '--enable-cross-compile',
                       '--target-os=mingw32', '--arch=x86_64', '--disable-autodetect', '--disable-programs',
                       '--disable-doc', '--disable-network', '--disable-vulkan', '--disable-hwaccels',
                       '--disable-x86asm', '--disable-everything', '--enable-static', '--disable-shared',
                       '--enable-decoder=h264,hevc,mpeg4,mjpeg,rawvideo', '--enable-parser=h264,hevc,mpeg4video',
                       '--enable-demuxer=mov,matroska', '--enable-protocol=file',
                       '--enable-filter=null,anull,format,scale,aformat,aresample', '--enable-gpl', '--enable-version3'], ffbuild)
        run('ffmpeg', ['make', f'-j{args.jobs}'], ffbuild)
        run('ffmpeg', ['make', 'install'], ffbuild)
        build('mpv', ['--auto-features', 'disabled', '--prefer-static', '-Dlibmpv=true', '-Dcplayer=false',
                      '-Dgpl=true', '-Dbuild-date=false', '-Dvulkan=disabled', '-Dgl=disabled',
                      '-Dplain-gl=disabled', '-Dd3d11=disabled', '-Dwin32-threads=enabled',
                      "-Dc_link_args=['-static', '-lc++']", '-Dcpp_link_args=-static'], shared=True)
        dll = prefix / 'bin/libmpv-2.dll'
        (root / 'CANDIDATE.json').write_text(json.dumps(dict(sha256=sha(dll), size_bytes=dll.stat().st_size,
            duration_seconds=time.monotonic()-start, native_windows_tested=False, redistribution_cleared=False), indent=2)+'\n')
        print(dll)
    finally:
        (root / 'elapsed.json').write_text(json.dumps(dict(seconds=time.monotonic()-start))+'\n')


if __name__ == '__main__':
    main()
