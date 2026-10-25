#!/usr/bin/env python3
"""Read-only package inventory. No license or redistribution inference from imports."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess


def inventory(root):
    root = Path(root).resolve(strict=True)
    if not root.is_dir():
        raise ValueError('package root must be a directory')
    records = []
    for path in sorted(root.rglob('*')):
        if path.is_symlink():
            raise ValueError('symlinks are not permitted in package inventory')
        if not path.is_file():
            continue
        h = hashlib.sha256()
        with path.open('rb') as stream:
            magic = stream.read(2)
            stream.seek(0)
            for block in iter(lambda: stream.read(1024 * 1024), b''):
                h.update(block)
        record = dict(filename=path.relative_to(root).as_posix(),
                      size_bytes=path.stat().st_size, sha256=h.hexdigest(),
                      version=None, origin=None, source_revision=None,
                      license=None, copyright_notices=None,
                      corresponding_source=None, redistribution_status='unresolved',
                      static_components='not identifiable from import table',
                      included=True, pe_imports=[])
        if magic == b'MZ':
            result = subprocess.run(['objdump', '-p', str(path)], capture_output=True,
                                    text=True, timeout=30, check=True)
            record['pe_imports'] = sorted(set(re.findall(r'DLL Name:\s*(\S+)', result.stdout)))
            record['import_resolution'] = 'requires recursive package/OS closure review'
        records.append(record)
    return dict(schema_version=1, scope='all regular files under supplied package root',
                license_clearance=False, dependency_closure_verified=False, files=records)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('package_root')
    parser.add_argument('new_output', help='must not exist and must be outside package root')
    args = parser.parse_args()
    root = Path(args.package_root).resolve(strict=True)
    output = Path(args.new_output).resolve()
    if output == root or root in output.parents:
        parser.error('output must be outside the inspected package')
    data = inventory(root)
    with output.open('x', encoding='utf-8') as stream:
        json.dump(data, stream, indent=2)
        stream.write('\n')


if __name__ == '__main__':
    main()
