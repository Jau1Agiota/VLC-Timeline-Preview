#!/usr/bin/env python3
"""GPL-2.0-or-later. Package own executable/source, never the unreviewed GPL runtime."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import zipfile


def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def package(binary,output,repository):
    output=Path(output)
    if output.exists():raise ValueError('output must be new')
    root=Path(repository).resolve();prototype=root/'prototypes/stage_2_4_6'
    revision=subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip()
    output.mkdir();stage=output/'libmpv-stage246A-win64';stage.mkdir()
    shutil.copy2(binary,stage/'mpv-preview.exe')
    for name in ('preview.c','request_policy.h','dependencies.json','Get-Runtime.ps1','Run-Preview.ps1',
                 'benchmark.py','build.sh','README.md','THIRD_PARTY.md'):
        shutil.copy2(prototype/name,stage/name)
    shutil.copy2(root/'COPYING',stage/'COPYING')
    compiler=Path(binary).with_name('compiler.txt')
    if compiler.exists():shutil.copy2(compiler,stage/'compiler.txt')
    manifest=dict(source_commit=revision,runtime_included=False,platform='Windows x64',
                  required_runtime=json.loads((prototype/'dependencies.json').read_text()),
                  files_sha256={p.name:sha(p) for p in stage.iterdir()})
    (stage/'MANIFEST.json').write_text(json.dumps(manifest,indent=2)+'\n')
    archive=output/f'libmpv-stage246A-win64-{revision[:12]}.zip'
    with zipfile.ZipFile(archive,'x',zipfile.ZIP_DEFLATED) as z:
        for path in stage.iterdir():z.write(path,stage.name+'/'+path.name)
    with zipfile.ZipFile(archive) as z:
        if z.testzip():raise ValueError('ZIP CRC failure')
    (output/'SHA256SUMS').write_text(sha(archive)+'  '+archive.name+'\n')
    # Avoid shipping a second uncompressed tree in the artifact.
    shutil.rmtree(stage)
    print('::notice title=Experiment package::'+json.dumps(dict(zip=archive.name,sha256=sha(archive),source=revision)))


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('binary');p.add_argument('output');p.add_argument('repository')
    a=p.parse_args();package(a.binary,a.output,a.repository)
