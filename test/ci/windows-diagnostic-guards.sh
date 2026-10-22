#!/usr/bin/env bash
# GPL-2.0-or-later. No Docker, sudo, downloads or application builds in this suite.
set -euo pipefail
root=$(cd -- "$(dirname -- "$0")/../.." && pwd -P)
source "$root/ci/windows/build-diagnostic.sh"
scratch=$(mktemp -d)
trap 'rm -rf -- "$scratch"' EXIT
export RUNNER_TEMP=$scratch GITHUB_RUN_ID=123 GITHUB_RUN_ATTEMPT=1
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
import os
import subprocess
import time
from unittest import mock
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
        self.assertTrue(triggers.startswith('  workflow_dispatch:'))
        self.assertEqual(re.findall(r'(?m)^  ([a-z_]+):', triggers), ['workflow_dispatch'])
        self.assertIn("default: '2.4.3'", triggers)
        self.assertIn("options: ['2.4.3', '2.4.4']", triggers)
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

class Finalization(unittest.TestCase):
    def setUp(self):
        self.base = scratch / self.id().split('.')[-1]
        self.base.mkdir()
        self.runner = self.base / 'runner'; self.runner.mkdir()
        self.work = self.runner / 'vlc-ci1-789-1'; self.work.mkdir()
        (self.work / '.ci1-work').touch()
        self.metrics = self.work / 'metrics'; self.metrics.mkdir()
        (self.work / 'logs').mkdir()
        self.env = dict(os.environ, GITHUB_ACTIONS='true', RUNNER_ENVIRONMENT='github-hosted',
                        RUNNER_OS='Linux', RUNNER_ARCH='X64', ImageOS='ubuntu24',
                        RUNNER_TEMP=str(self.runner), GITHUB_RUN_ID='789', GITHUB_RUN_ATTEMPT='1')
        self.env.pop('CI_INSIDE', None)
        self.env.pop('CI_MONITOR_PID', None)
        self.driver = root / 'ci/windows/build-diagnostic.sh'

    def shell(self, code, env=None):
        import shlex
        return subprocess.run(['bash', '-c', 'source ' + shlex.quote(str(self.driver)) + '\n' + code],
                              env=env or self.env, capture_output=True, text=True, timeout=12)

    def finish(self, status=None, docker=None):
        with mock.patch.dict(os.environ, self.env, clear=True):
            if docker:
                with mock.patch.object(p.subprocess, 'run', side_effect=docker):
                    return p.finalize(self.work, 'host', status, 'test-phase')
            return p.finalize(self.work, 'host', status, 'test-phase')

    def test_guards_fail_closed_with_errexit_disabled(self):
        mocks = "docker() { echo CALLED; }; python3() { echo CALLED; }; touch() { echo CALLED; }; sudo() { echo CALLED; };"
        before = sorted(str(x) for x in self.work.rglob('*'))
        for guard in ('hosted', 'ownership'):
            env = dict(self.env)
            if guard == 'hosted': env['RUNNER_ENVIRONMENT'] = 'self-hosted'
            else: env['GITHUB_RUN_ID'] = '999'
            for action in (f'ci_finish "{self.work}"', f'CI_WORK="{self.work}"; ci_prepare_host'):
                result = self.shell('set +e; ' + mocks + action, env)
                self.assertEqual(result.returncode, 2, result.stderr)
                self.assertNotIn('CALLED', result.stdout)
        self.assertEqual(before, sorted(str(x) for x in self.work.rglob('*')))

    def test_direct_python_guard_before_operations(self):
        (self.work / 'container.id').write_text('a' * 64)
        with mock.patch.dict(os.environ, dict(self.env, GITHUB_RUN_ID='999'), clear=True), \
             mock.patch.object(p.subprocess, 'run') as docker, mock.patch.object(p, 'snapshot') as snapshot:
            with self.assertRaises(ValueError): p.finalize(self.work, 'host', 37)
            docker.assert_not_called(); snapshot.assert_not_called()
        self.assertEqual(list(self.metrics.iterdir()), [])

    def test_symlink_evidence_rejected(self):
        outside = self.base / 'untouched'; outside.write_text('preserve')
        (self.metrics / 'docker-state.json').symlink_to(outside)
        result = self.shell(f'set +e; ci_finish "{self.work}"')
        self.assertEqual(result.returncode, 2)
        self.assertEqual(outside.read_text(), 'preserve')
        self.assertFalse((self.metrics / 'run-status.json').exists())

    def test_container_identity_symlink_rejected(self):
        outside = self.base / 'other-container'; outside.write_text('a' * 64)
        (self.work / 'container.id').symlink_to(outside)
        result = self.shell(f'set +e; ci_finish "{self.work}"')
        self.assertEqual(result.returncode, 2)
        self.assertEqual(list(self.metrics.iterdir()), [])

    def test_double_finish_preserves_original_build_failure(self):
        result = self.shell(f'CI_WORK="{self.work}"; CI_PHASE=test-failure; ci_exit 37')
        self.assertEqual(result.returncode, 37, result.stderr)
        result = self.shell(f'ci_finish_entry "{self.work}"')
        self.assertEqual(result.returncode, 0, result.stderr)
        summary = json.loads((self.metrics / 'summary.json').read_text())
        self.assertEqual(summary['run-status.json']['exit_code'], 37)
        self.assertEqual(len(summary['teardown']), 2)

    def test_teardown_failure_does_not_replace_build_exit(self):
        result = self.shell(f'CI_WORK="{self.work}"; ci_finish() {{ return 91; }}; ci_exit 37')
        self.assertEqual(result.returncode, 37)

    def test_failed_inspection_preserves_evidence_and_error_history(self):
        (self.work / 'container.id').write_text('a' * 64)
        state = {'Running': False, 'OOMKilled': True, 'ExitCode': 137}
        def success(argv, **kwargs):
            self.assertEqual(kwargs['timeout'], 10)
            return subprocess.CompletedProcess(argv, 0, json.dumps(state).encode(), b'')
        self.assertEqual(self.finish(37, success), 0)
        previous = (self.metrics / 'docker-state.json').read_bytes()
        def failure(argv, **kwargs):
            self.assertIn(kwargs['timeout'], (10, 25))
            return subprocess.CompletedProcess(argv, 1, b'', b'controlled daemon failure')
        self.assertEqual(self.finish(None, failure), 1)
        self.assertEqual(previous, (self.metrics / 'docker-state.json').read_bytes())
        summary = json.loads((self.metrics / 'summary.json').read_text())
        self.assertEqual(summary['run-status.json']['exit_code'], 37)
        self.assertTrue(summary['teardown'][-1]['errors'])
        self.assertEqual(self.finish(None, success), 1)
        summary = json.loads((self.metrics / 'summary.json').read_text())
        self.assertTrue(summary['teardown'][1]['errors'])
        self.assertEqual(summary['run-status.json']['exit_code'], 37)

    def test_container_status_cannot_mask_host_failure(self):
        # Exercise scope separation without a real /out mount or Wine/Docker.
        with mock.patch.object(p, 'reporting_owned'):
            self.assertEqual(p.finalize(self.work, 'container', 0, 'compose'), 0)
        self.assertFalse((self.metrics / 'run-status.json').exists())
        self.assertEqual(self.finish(37), 0)
        summary = json.loads((self.metrics / 'summary.json').read_text())
        self.assertEqual(summary['run-status.json']['exit_code'], 37)
        self.assertEqual(summary['container-run-status.json']['exit_code'], 0)

    def test_docker_clients_have_deadlines(self):
        (self.work / 'container.id').write_text('a' * 64)
        calls = []
        def timeout(argv, **kwargs):
            calls.append((argv[1], kwargs['timeout']))
            raise subprocess.TimeoutExpired(argv, kwargs['timeout'])
        self.assertEqual(self.finish(42, timeout), 1)
        self.assertEqual(calls, [('inspect', 10), ('stop', 25), ('inspect', 10)])
        self.assertEqual(json.loads((self.metrics / 'run-status.json').read_text())['exit_code'], 42)

    def test_empty_container_id_does_not_skip_reporting(self):
        (self.work / 'container.id').touch()
        with mock.patch.object(p.subprocess, 'run') as docker:
            self.assertEqual(self.finish(38), 1)
            docker.assert_not_called()
        summary = json.loads((self.metrics / 'summary.json').read_text())
        self.assertEqual(summary['run-status.json']['exit_code'], 38)
        self.assertIn('invalid container ID', summary['teardown'][-1]['errors'][0])

    def test_atomic_replacement_failure_keeps_previous_record(self):
        destination = self.metrics / 'docker-state.json'
        destination.write_text('{"Running":false}')
        with mock.patch.object(p.os, 'replace', side_effect=OSError('controlled storage failure')):
            with self.assertRaises(OSError): p.atomic_json(destination, {'Running': True})
        self.assertEqual(destination.read_text(), '{"Running":false}')
        self.assertEqual(sorted(x.name for x in self.metrics.iterdir()), ['docker-state.json'])

    def test_partial_initialization_repaired(self):
        self.metrics.rmdir(); (self.work / 'logs').rmdir()
        result = self.shell(f'ci_finish_entry "{self.work}"')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue((self.metrics / 'summary.json').is_file())

    def test_initialization_failure_has_exit_trap(self):
        shutil.rmtree(self.work)
        result = self.shell('ci_new_work() { mkdir "$1"; touch "$1/.ci1-work"; return 19; }; '
                            f'ci_run /missing-source "{self.work}"')
        self.assertEqual(result.returncode, 19, result.stderr)
        self.assertEqual(json.loads((self.metrics / 'run-status.json').read_text())['exit_code'], 19)

    def test_missing_output_after_early_checkout_failure(self):
        shutil.rmtree(self.work)
        result = self.shell(f'ci_finish_entry "{self.work}"')
        self.assertEqual(result.returncode, 0, result.stderr)
        record = json.loads((self.metrics / 'run-status.json').read_text())
        self.assertIsNone(record['exit_code'])
        self.assertEqual(record['phase'], 'before-driver')

    def test_storage_failure_console_only_preserves_build_status(self):
        self.metrics.rmdir(); (self.work / 'logs').rmdir()
        result = self.shell('mkdir() { return 28; }; '
                            f'CI_WORK="{self.work}"; CI_PHASE=initialization; ci_exit 73')
        self.assertEqual(result.returncode, 73, result.stderr)
        self.assertIn('console-only', result.stderr)
        self.assertFalse(self.metrics.exists())

    def test_nonowned_partial_directory_is_not_repaired(self):
        (self.work / '.ci1-work').unlink(); self.metrics.rmdir()
        result = self.shell(f'set +e; ci_finish_entry "{self.work}"')
        self.assertEqual(result.returncode, 2)
        self.assertFalse(self.metrics.exists())

    def test_graceful_monitor_joined_before_summary(self):
        output = self.metrics / 'host.jsonl'
        child = subprocess.Popen([sys.executable, str(root / 'ci/windows/package-diagnostic.py'),
                                  'monitor', str(output), '--stop', str(self.metrics / 'host.stop'),
                                  '--disk', str(self.work)], env=self.env, stdout=subprocess.DEVNULL,
                                  stderr=subprocess.PIPE)
        try:
            deadline = time.monotonic() + 3
            while not Path(str(output) + '.owner.json').exists() and time.monotonic() < deadline: time.sleep(0.02)
            (self.metrics / 'host.stop').touch()
            self.assertEqual(self.finish(37), 0)
            self.assertEqual(child.wait(timeout=1), 0)
            summary = json.loads((self.metrics / 'summary.json').read_text())
            self.assertTrue(summary['host']['monitor_finished'])
            self.assertGreaterEqual(summary['host']['samples'], 2)
        finally:
            if child.poll() is None: child.kill(); child.wait(timeout=2)
            child.stderr.close()

    def test_monitor_timeout_is_bounded_and_reported(self):
        output = self.metrics / 'host.jsonl'
        pid, errors, signals = 23456, [], []
        owner = {'pid': pid, 'start': '123'}
        p.atomic_json(str(output) + '.owner.json', owner)
        command = b'python3\0' + str(Path(p.__file__).resolve()).encode() + b'\0monitor\0' + str(output).encode() + b'\0'
        clock = [0]
        def tick(): clock[0] += 1; return clock[0]
        original_read = Path.read_bytes
        def read(path): return command if str(path) == f'/proc/{pid}/cmdline' else original_read(path)
        with mock.patch.object(p, 'process_identity', return_value='123'), \
             mock.patch.object(Path, 'read_bytes', read), mock.patch.object(p.time, 'monotonic', side_effect=tick), \
             mock.patch.object(p.time, 'sleep'), mock.patch.object(p.os, 'kill', side_effect=lambda pid, sig: signals.append(sig)):
            p.stop_monitor(self.metrics, 'host', None, errors.append, grace=2, escalation=1)
        self.assertEqual(signals, [p.signal.SIGTERM, p.signal.SIGKILL])
        self.assertTrue(any('remains alive' in error for error in errors))
        self.assertFalse(Path(str(output) + '.done').exists())

    def test_monitor_exiting_during_identity_check_is_safe(self):
        output = self.metrics / 'host.jsonl'
        p.atomic_json(str(output) + '.owner.json', {'pid': 23456, 'start': '123'})
        Path(str(output) + '.done').touch()
        errors = []
        with mock.patch.object(p, 'process_identity', side_effect=['123', None]), \
             mock.patch.object(Path, 'read_bytes', side_effect=FileNotFoundError), \
             mock.patch.object(p.os, 'kill') as kill:
            p.stop_monitor(self.metrics, 'host', None, errors.append)
            kill.assert_not_called()
        self.assertEqual(errors, [])

    def test_monitor_identity_mismatch_never_signals(self):
        output = self.metrics / 'host.jsonl'
        p.atomic_json(str(output) + '.owner.json', {'pid': 23456, 'start': '123'})
        with mock.patch.object(p, 'process_identity', return_value='123'), \
             mock.patch.object(Path, 'read_bytes', return_value=b'unrelated-process\0'), \
             mock.patch.object(p.os, 'kill') as kill:
            with self.assertRaises(ValueError): p.stop_monitor(self.metrics, 'host', None, lambda _: None)
            kill.assert_not_called()

    def test_abnormal_monitor_exit_reported(self):
        output = self.metrics / 'host.jsonl'
        p.atomic_json(str(output) + '.owner.json', {'pid': 23456, 'start': '123'})
        with mock.patch.object(p, 'process_identity', return_value=None):
            self.assertEqual(self.finish(37), 1)
        summary = json.loads((self.metrics / 'summary.json').read_text())
        self.assertTrue(any('without completion' in error for error in summary['teardown'][-1]['errors']))

unittest.main(argv=[sys.argv[0]], verbosity=2)
PY
