#!/usr/bin/env bash
# GPL-2.0-or-later. No Docker, sudo, downloads or application builds in this suite.
set -euo pipefail
root=$(cd -- "$(dirname -- "$0")/../.." && pwd -P)
source "$root/ci/windows/build-diagnostic.sh"
scratch=$(mktemp -d)
trap 'rm -rf -- "$scratch"' EXIT
export RUNNER_TEMP=$scratch
reject() {
    if "$@" >"$scratch/rejected.log" 2>&1; then
        echo "Unexpected success: $*" >&2; exit 1
    fi
}
# Even locally configured Docker must never authorize runner cleanup.
unset GITHUB_ACTIONS RUNNER_ENVIRONMENT RUNNER_OS RUNNER_ARCH ImageOS
reject ci_require_hosted
export GITHUB_ACTIONS=true RUNNER_ENVIRONMENT=self-hosted RUNNER_OS=Linux RUNNER_ARCH=X64 ImageOS=ubuntu24
reject ci_require_hosted
export RUNNER_ENVIRONMENT=github-hosted
ci_require_hosted
for path in / /workspace /tmp /var/lib/docker /usr/share/dotnet/ /usr/share/dotnet/../ /opt; do
    reject ci_cleanup_allowed "$path"
done
for path in /usr/local/lib/android /usr/share/dotnet /opt/ghc /usr/local/.ghcup; do ci_cleanup_allowed "$path"; done
ci_space 100 100
ci_space 101 100
reject ci_space 99 100
reject ci_space unknown 100
reject ci_owned_work "$scratch"
mkdir "$scratch/vlc-ci1-123-1"
reject ci_owned_work "$scratch/vlc-ci1-123-1"
touch "$scratch/vlc-ci1-123-1/.ci1-work"
ci_owned_work "$scratch/vlc-ci1-123-1"
ln -s "$scratch/vlc-ci1-123-1" "$scratch/vlc-ci1-123-2"
reject ci_owned_work "$scratch/vlc-ci1-123-2"
export GITHUB_RUN_ID=345 GITHUB_RUN_ATTEMPT=1
reject ci_new_work "$scratch/wrong-name"
ci_new_work "$scratch/vlc-ci1-345-1"
ci_owned_work "$CI_WORK"
reject ci_new_work "$CI_WORK"
printf 'known content\n' > "$scratch/sha-input"
ci_sha "$scratch/sha-input" "$(sha256sum "$scratch/sha-input" | cut -d ' ' -f 1)"
reject ci_sha "$scratch/sha-input" 0000000000000000000000000000000000000000000000000000000000000000
reject ci_source "$root" # CI driver is deliberately NOT the pinned application checkout.

mkdir "$scratch/contrib" "$scratch/state" "$scratch/other"
cat > "$scratch/make" <<'MAKE'
#!/usr/bin/env bash
printf '%s\n' "$*" >> "$CI_TEST_LOG"
exit "${CI_TEST_MAKE_STATUS:-0}"
MAKE
chmod +x "$scratch/make"
export CI_REAL_MAKE=$scratch/make CI_CONTRIB_DIR=$scratch/contrib CI_GUARD_STATE=$scratch/state
export VLC_PREBUILT_CONTRIBS_URL=file:///out/downloads/contrib.tar.zst CI_TEST_LOG=$scratch/make.log
(
    cd "$CI_CONTRIB_DIR"
    reject ci_make_guard .luac
    reject ci_make_guard list
    reject ci_make_guard prebuilt PREBUILT_URL=https://unapproved.invalid/package
    ci_make_guard prebuilt "PREBUILT_URL=$VLC_PREBUILT_CONTRIBS_URL"
    ci_make_guard .luac
    ci_make_guard list
    for target in fetch -j4 -k all package ''; do reject ci_make_guard "$target"; done
    [[ $(wc -l < "$CI_TEST_LOG") == 3 ]]
    export CI_TEST_MAKE_STATUS=7
    reject ci_make_guard prebuilt "PREBUILT_URL=$VLC_PREBUILT_CONTRIBS_URL"
    [[ -f $CI_GUARD_STATE/prebuilt.failed ]]
    reject ci_make_guard .luac
    reject ci_make_guard list
    reject ci_make_guard fetch
)
(cd "$scratch/other" && ci_make_guard normal-application-build)
echo 'PASS: hosted-only cleanup, disk, ownership and fail-closed contrib guards'

python3 - "$root" "$scratch" <<'PY'
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import shutil
import struct
import sys
import unittest
import zipfile

sys.dont_write_bytecode = True
root, scratch = map(Path, sys.argv[1:])
spec = importlib.util.spec_from_file_location('ci_package', root / 'ci/windows/package-diagnostic.py')
p = importlib.util.module_from_spec(spec)
spec.loader.exec_module(p)

class Packaging(unittest.TestCase):
    def setUp(self):
        self.base = scratch / self.id().split('.')[-1]
        self.base.mkdir()
        self.staging = self.base / 'staging'
        self.staging.mkdir()
        self.out = self.base / 'delivery'
        self.meta = self.base / 'inputs.json'
        self.metadata = dict(source_commit=p.SOURCE, driver_commit='a' * 40, image=p.IMAGE,
                             contrib_commit=p.CONTRIB, contrib_sha256=p.CONTRIB_SHA,
                             sdk_sha256=p.SDK_SHA, run_id='123', run_attempt='1')
        self.meta.write_text(json.dumps(self.metadata))
        binary = bytearray(154)
        binary[:2] = b'MZ'
        struct.pack_into('<I', binary, 0x3c, 128)
        binary[128:132] = b'PE\0\0'
        struct.pack_into('<H', binary, 132, 0x8664)
        struct.pack_into('<H', binary, 152, 0x20b)
        for name in p.BINARIES:
            file = self.staging / name
            file.parent.mkdir(parents=True, exist_ok=True)
            file.write_bytes(binary + (p.MARKER if name.endswith('libqt_plugin.dll') else b''))
        for name in ('COPYING.txt', 'thumbnail-preview/COPYING.txt',
                     'thumbnail-preview/plugins/codec/official.dll',
                     'thumbnail-preview/plugins/plugins.dat', 'plugins/plugins.dat',
                     'STAGE_2_4_2_SHA256SUMS'):
            file = self.staging / name
            file.parent.mkdir(parents=True, exist_ok=True)
            file.write_bytes(b'fixture content\n')

    def build(self):
        with contextlib.redirect_stdout(io.StringIO()):
            p.package(self.staging, self.out, self.meta)
        return json.loads((self.out / 'PACKAGE_MANIFEST.json').read_text())

    def test_package_roundtrip_and_preservation(self):
        before = {f.relative_to(self.staging): p.digest(f) for f in self.staging.rglob('*') if f.is_file()}
        record = self.build()
        with zipfile.ZipFile(self.out / record['zip']) as z:
            names = {n.removeprefix(record['root'] + '/') for n in z.namelist()}
            self.assertNotIn('plugins/plugins.dat', names)
            self.assertNotIn('STAGE_2_4_2_SHA256SUMS', names)
            self.assertIn('thumbnail-preview/plugins/plugins.dat', names)
            self.assertIn('thumbnail-preview/COPYING.txt', names)
            manifest = json.loads(z.read(record['root'] + '/BUILD_MANIFEST.json'))
            self.assertFalse(manifest['native_windows_gui_tested'])
            self.assertFalse(manifest['frame_pts_accuracy_verified'])
        self.assertEqual(before, {f.relative_to(self.staging): p.digest(f) for f in self.staging.rglob('*') if f.is_file()})
        with self.assertRaises(ValueError): p.package(self.staging, self.out, self.meta)

    def test_no_recursive_output(self):
        with self.assertRaises(ValueError): p.package(self.staging, self.staging / 'nested', self.meta)

    def test_missing_private_runtime(self):
        (self.staging / 'thumbnail-preview/libvlc.dll').unlink()
        with self.assertRaises(ValueError): self.build()

    def test_missing_license(self):
        (self.staging / 'thumbnail-preview/COPYING.txt').unlink()
        with self.assertRaises(ValueError): self.build()

    def test_missing_instrumentation(self):
        (self.staging / 'plugins/gui/libqt_plugin.dll').write_bytes((self.staging / 'vlc.exe').read_bytes())
        with self.assertRaises(ValueError): self.build()

    def test_wrong_architecture(self):
        file = self.staging / 'vlc.exe'
        data = bytearray(file.read_bytes()); struct.pack_into('<H', data, 132, 0x14c); file.write_bytes(data)
        with self.assertRaises(ValueError): self.build()

    def test_symlink(self):
        (self.staging / 'link').symlink_to(self.staging / 'vlc.exe')
        with self.assertRaises(ValueError): self.build()

    def test_build_material(self):
        (self.staging / 'win64').mkdir(); (self.staging / 'win64/build.log').touch()
        with self.assertRaises(ValueError): self.build()

    def test_wrong_dependency_pin(self):
        self.metadata['sdk_sha256'] = '0' * 64
        self.meta.write_text(json.dumps(self.metadata))
        with self.assertRaises(ValueError): self.build()

    def test_checksum_corruption(self):
        record = self.build()
        with (self.out / record['zip']).open('ab') as stream: stream.write(b'changed')
        with self.assertRaises(ValueError): p.verify(self.out)

    def test_checksum_targets(self):
        self.build()
        (self.out / 'SHA256SUMS').write_text('0' * 64 + '  ../other\n')
        with self.assertRaises(ValueError): p.verify(self.out)

    def test_expected_producer_manifest(self):
        self.build()
        expected = self.base / 'producer.json'; shutil.copyfile(self.out / 'PACKAGE_MANIFEST.json', expected)
        record = json.loads(expected.read_text()); record['driver_commit'] = 'b' * 40
        (self.out / 'PACKAGE_MANIFEST.json').write_text(json.dumps(record))
        with self.assertRaises(ValueError): p.verify(self.out, expected)

    def test_archive_path_guards(self):
        for name in ('../bad', '/absolute', 'a/../b', 'a\\b', 'C:/bad', 'a//b', './bad', ''):
            with self.subTest(name=name), self.assertRaises(ValueError): p.safe_name(name)

    def test_rehashed_malicious_archive(self):
        record = self.build()
        archive = self.out / record['zip']
        with zipfile.ZipFile(archive, 'a') as z: z.writestr('../outside', b'bad')
        record['sha256'] = p.digest(archive); record['size_bytes'] = archive.stat().st_size
        (self.out / 'PACKAGE_MANIFEST.json').write_bytes(p.json_bytes(record))
        (self.out / 'SHA256SUMS').write_text(record['sha256'] + '  ' + archive.name + '\n' +
            p.digest(self.out / 'PACKAGE_MANIFEST.json') + '  PACKAGE_MANIFEST.json\n')
        with self.assertRaises(ValueError): p.verify(self.out)

    def test_failed_command_measurements(self):
        measurements = self.base / 'failed-time.json'
        self.assertEqual(p.timed(measurements, [sys.executable, '-c', 'raise SystemExit(23)']), 23)
        record = json.loads(measurements.read_text())
        self.assertEqual(record['exit_code'], 23)
        self.assertGreater(record['wall_seconds'], 0)
        self.assertGreater(record['largest_child_max_rss_bytes'], 0)

    def test_workflow_policy(self):
        import re
        workflow = (root / '.github/workflows/windows-diagnostic-pilot.yml').read_text()
        triggers = workflow.split('on:\n', 1)[1].split('\npermissions:', 1)[0]
        self.assertEqual(triggers.strip(), 'workflow_dispatch:')
        self.assertRegex(workflow, r'permissions:\n  contents: read\n')
        self.assertNotRegex(workflow, r'contents: write|secrets\.|gh release|workflow_run:|pull_request:')
        uses = re.findall(r'uses: ([^\s#]+)', workflow)
        self.assertTrue(uses)
        self.assertTrue(all(re.fullmatch(r'actions/[a-z-]+@[0-9a-f]{40}', use) for use in uses))
        self.assertIn('ref: ${{ github.workflow_sha }}', workflow)
        self.assertIn(p.SOURCE, workflow)
        self.assertIn('--expected-manifest', workflow)

    def test_failure_telemetry(self):
        metrics = self.base / 'metrics'; metrics.mkdir()
        stop = metrics / 'stop'; stop.touch()
        p.monitor(metrics / 'host.jsonl', stop, [str(self.base)])
        p.snapshot(metrics / 'events.jsonl', [str(self.base)], 'failed-phase', 17)
        with (metrics / 'events.jsonl').open('a') as f: f.write('{truncated')
        (metrics / 'run-status.json').write_text('{"exit_code":17,"phase":"failed-phase"}')
        p.summarize(metrics)
        summary = json.loads((metrics / 'summary.json').read_text())
        self.assertEqual(summary['run-status.json']['exit_code'], 17)
        self.assertEqual(summary['events']['last_sample']['phase'], 'failed-phase')
        self.assertGreater(summary['host']['minimum_disk_available_bytes'], 0)
        self.assertTrue(summary['host']['monitor_finished'])
        self.assertFalse(summary['container']['measurements_available'])

unittest.main(argv=[sys.argv[0]], verbosity=2)
PY
