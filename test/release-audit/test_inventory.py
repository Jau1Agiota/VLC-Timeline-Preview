import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest import mock

SCRIPT = Path(__file__).resolve().parents[2] / 'tools/release-audit/inventory.py'
spec = importlib.util.spec_from_file_location('inventory', SCRIPT)
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)


class InventoryTests(unittest.TestCase):
    def test_files_do_not_imply_clearance(self):
        with tempfile.TemporaryDirectory() as d:
            (Path(d) / 'notice.txt').write_text('license text')
            result = audit.inventory(d)
            self.assertFalse(result['license_clearance'])
            self.assertIsNone(result['files'][0]['license'])
            self.assertEqual(result['files'][0]['filename'], 'notice.txt')

    def test_symlink_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            (Path(d) / 'link').symlink_to('/etc/passwd')
            with self.assertRaises(ValueError):
                audit.inventory(d)

    def test_imports_are_recorded_without_static_claim(self):
        with tempfile.TemporaryDirectory() as d:
            (Path(d) / 'runtime.dll').write_bytes(b'MZexample')
            with mock.patch.object(audit.subprocess, 'run', return_value=
                                   subprocess.CompletedProcess([], 0, 'DLL Name: vulkan-1.dll\n')):
                record = audit.inventory(d)['files'][0]
            self.assertEqual(record['pe_imports'], ['vulkan-1.dll'])
            self.assertEqual(record['redistribution_status'], 'unresolved')

    def test_bad_pe_fails_closed(self):
        with tempfile.TemporaryDirectory() as d:
            (Path(d) / 'bad.exe').write_bytes(b'MZinvalid')
            with self.assertRaises(subprocess.CalledProcessError):
                audit.inventory(d)

    def test_existing_output_not_overwritten(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d) / 'package'
            root.mkdir()
            output = Path(d) / 'inventory.json'
            output.write_text('preserve')
            result = subprocess.run(['python3', str(SCRIPT), str(root), str(output)], capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(output.read_text(), 'preserve')

    def test_output_inside_package_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            output = Path(d) / 'inventory.json'
            result = subprocess.run(['python3', str(SCRIPT), d, str(output)], capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertFalse(output.exists())


if __name__ == '__main__':
    unittest.main()
