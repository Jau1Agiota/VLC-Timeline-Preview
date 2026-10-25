#!/usr/bin/env python3
"""GPL-2.0-or-later. Assemble source-only materials; does not clear redistribution."""
import argparse
import hashlib
import json
from pathlib import Path
import tarfile
from build import fetch

HERE=Path(__file__).resolve().parent


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('downloads',type=Path)
    parser.add_argument('new_tar',type=Path)
    parser.add_argument('--fetch-missing',action='store_true')
    parser.add_argument('--build-record',type=Path)
    args=parser.parse_args()
    if args.new_tar.exists() or args.new_tar.is_symlink():
        parser.error('output must not exist')
    lock=json.loads((HERE/'inputs.lock.json').read_text())
    extras=json.loads((HERE/'toolchain-sources.lock.json').read_text())
    rows=[v for v in lock['inputs'].values() if v['kind']=='source']+list(extras.values())
    for row in rows:
        p=args.downloads/row['filename']
        if not p.exists() and args.fetch_missing:
            fetch(row,p,None)
        if hashlib.sha256(p.read_bytes()).hexdigest()!=row['sha256']:
            raise ValueError('source mismatch: '+row['filename'])
    with args.new_tar.open('xb') as raw,tarfile.open(fileobj=raw,mode='w') as archive:
        for row in rows:
            archive.add(args.downloads/row['filename'],arcname='sources/'+row['filename'],recursive=False)
        for p in HERE.iterdir():
            if p.suffix in ['.py','.json','.md']:
                archive.add(p,arcname='ci/runtime/'+p.name,recursive=False)
        if args.build_record:
            for name in ['commands.json','cross.ini','inputs.lock.json','CANDIDATE.json']:
                archive.add(args.build_record/name,arcname='build-record/'+name,recursive=False)
            for path in (args.build_record/'logs').glob('*.log'):
                archive.add(path,arcname='build-record/logs/'+path.name,recursive=False)
            for path in (args.build_record/'build').glob('*/meson-info/intro-buildoptions.json'):
                archive.add(path,arcname='build-record/config/'+path.parent.parent.name+'.json',recursive=False)
            for name in ['config.h','config_components.h','ffbuild/config.mak']:
                archive.add(args.build_record/'build/ffmpeg'/name,arcname='build-record/ffmpeg/'+name,recursive=False)
    h=hashlib.sha256()
    with args.new_tar.open('rb') as stream:
        for block in iter(lambda:stream.read(1048576),b''):h.update(block)
    print(json.dumps(dict(filename=args.new_tar.name,sha256=h.hexdigest(),size_bytes=args.new_tar.stat().st_size,
                         source_archives=len(rows),redistribution_review_complete=False),indent=2))


if __name__=='__main__':main()
