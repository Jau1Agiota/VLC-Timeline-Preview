#!/usr/bin/env python3
"""GPL-2.0-or-later. Static PE checks; not a clean Windows dependency certificate."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess


def pe(path):
    return subprocess.check_output(['objdump','-p',str(path)],text=True,timeout=30)


def exports(text):
    part=text.split('[Ordinal/Name Pointer] Table',1)[1].split('\n\n',1)[0]
    return sorted(set(re.findall(r'\b(mpv_\w+)\s*$',part,re.M)))


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('dll',type=Path);p.add_argument('baseline',type=Path)
    p.add_argument('workers',type=Path);p.add_argument('new_output',type=Path)
    a=p.parse_args();text=pe(a.dll);names=exports(text)
    if not names:raise ValueError('no mpv exports parsed')
    workers={}
    for path in sorted(a.workers.glob('*worker.exe')):
        part=pe(path).split('DLL Name: libmpv-2.dll',1)[1].split('DLL Name:',1)[0]
        required=sorted(set(re.findall(r'\b(mpv_\w+)\b',part)))
        if not required:raise ValueError('no worker imports parsed')
        workers[path.name]=dict(sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                               required_exports=required,missing_exports=sorted(set(required)-set(names)))
    if not workers:raise ValueError('no workers inspected')
    imports=re.findall(r'DLL Name:\s*(\S+)',text)
    report=dict(candidate_sha256=hashlib.sha256(a.dll.read_bytes()).hexdigest(),size_bytes=a.dll.stat().st_size,
                direct_imports=imports,candidate_exports=names,
                baseline_exports_missing=sorted(set(exports(pe(a.baseline)))-set(names)),workers=workers,
                normal_import_vulkan_absent='vulkan-1.dll' not in [x.lower() for x in imports],
                recursive_windows_os_closure_verified=False,native_windows_tested=False)
    with a.new_output.open('x') as stream:json.dump(report,stream,indent=2);stream.write('\n')
    if not report['normal_import_vulkan_absent'] or any(row['missing_exports'] for row in workers.values()):
        raise SystemExit('candidate static compatibility check failed')


if __name__=='__main__':main()
