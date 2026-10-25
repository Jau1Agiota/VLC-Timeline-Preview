#!/usr/bin/env python3
"""GPL-2.0-or-later. Bounded near-end and failure cases using unchanged workers."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import time
from matrix import parse


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['candidate','baseline','bridge','fixtures','new-output','wine','wine-env']:
        p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();a.new_output.mkdir(exist_ok=False)
    env=dict(os.environ,**json.loads(a.wine_env.read_text()));records=[]
    for label,folder in [('baseline',a.baseline),('candidate',a.candidate)]:
        for name,target,expected in [('vfr.mp4',29950,True),('vfr.mkv',29950,True),
                                     ('ordinary.mp4',0,True),('audio_only.mp4',1000,False),
                                     ('corrupt.mp4',1000,False),('missing.mp4',1000,False)]:
            start=time.monotonic();row=dict(runtime=label,media=name,target=target,expected_image=expected)
            try:
                result=subprocess.run([str(a.wine),str(a.bridge),'Z:'+str((folder/'mpv-thumbnail-accurate-worker.exe').resolve()),
                                       'Z:'+str((a.fixtures/name).resolve()),str(target)],env=env,capture_output=True,timeout=35)
                (a.new_output/f'{label}-{name}.stderr').write_bytes(result.stderr)
                row.update(exit_code=result.returncode,elapsed_ms=(time.monotonic()-start)*1000)
                if result.returncode==0:
                    frames=parse(result.stdout,[target]);row['frame']=frames[0][0]
                    row['passed']=expected
                else:row['passed']=not expected and not result.stdout
            except subprocess.TimeoutExpired:
                row.update(passed=False,error='outer timeout')
            records.append(row);(a.new_output/'results.json').write_text(json.dumps(records,indent=2)+'\n')
    print(json.dumps(records,indent=2))
    return 0 if all(row['passed'] for row in records) else 1


if __name__=='__main__':raise SystemExit(main())
