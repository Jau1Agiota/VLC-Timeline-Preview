#!/usr/bin/env python3
"""Read-only published provider log recovery. Never uploads or inspects runtime DLLs."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import urllib.error
import urllib.parse
import urllib.request
import zipfile

URL='https://api.github.com/repos/shinchiro/mpv-winbuild-cmake/actions/runs/39518569492/logs'


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None


def collect():
    token=os.environ.get('GITHUB_TOKEN')
    if not token:
        return dict(status='credential unavailable',source_closure_verified=False)
    request=urllib.request.Request(URL,headers={'Authorization':'Bearer '+token,
        'Accept':'application/vnd.github+json','X-GitHub-Api-Version':'2022-11-28'})
    opener=urllib.request.build_opener(NoRedirect)
    status=dict(provider_run=39518569492,source_closure_verified=False)
    try:
        try:
            response=opener.open(request,timeout=45)
        except urllib.error.HTTPError as error:
            status['api_http']=error.code
            if error.code not in [301,302,303,307,308]:
                return dict(status,error='provider API request failed')
            location=error.headers.get('Location','')
            parsed=urllib.parse.urlsplit(location)
            if parsed.scheme!='https' or not parsed.hostname or not parsed.hostname.endswith(('.actions.githubusercontent.com','.blob.core.windows.net')):
                return dict(status,error='unapproved redirect host; credentials not forwarded')
            # Signed storage request has no Authorization header. Never log its URL.
            response=opener.open(urllib.request.Request(location),timeout=60)
        with response:
            status['storage_http']=response.status
            data=response.read(64*1024*1024+1)
        if len(data)>64*1024*1024:
            return dict(status,error='log archive exceeded 64 MiB limit')
        import io
        archive=zipfile.ZipFile(io.BytesIO(data))
        status.update(archive_sha256=hashlib.sha256(data).hexdigest(),archive_size_bytes=len(data),
                      matching_log_files=[],unattributed_revision_observations=[])
        total=0
        for item in archive.infolist():
            if 'x86_64' not in item.filename or 'x86_64_v3' in item.filename:
                continue
            total+=item.file_size
            if item.file_size>16*1024*1024 or total>256*1024*1024:
                raise ValueError('uncompressed log size limit')
            status['matching_log_files'].append(item.filename)
            text=archive.read(item).decode('utf-8',errors='replace')
            for line in text.splitlines():
                if 'HEAD is now at' not in line or re.search(r'token|secret|authorization',line,re.I):
                    continue
                # Observations are not a source lock: project/revision association
                # and static linking membership must be established separately.
                match=re.search(r'HEAD is now at\s+([a-f0-9]{7,40})',line)
                if match and len(status['unattributed_revision_observations'])<30:
                    status['unattributed_revision_observations'].append({'log':item.filename,'commit_prefix':match.group(1)})
        return status
    except urllib.error.HTTPError as error:
        return dict(status,storage_http=error.code,error='log download denied; signed URL omitted')
    except (urllib.error.URLError,TimeoutError,ValueError,zipfile.BadZipFile) as error:
        return dict(status,error=type(error).__name__)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('new_output',type=Path)
    args=parser.parse_args()
    if args.new_output.exists():parser.error('output must not exist')
    result=collect()
    with args.new_output.open('x') as stream:json.dump(result,stream,indent=2);stream.write('\n')
    # Small public metadata only; never a token, signed URL, archive or raw log.
    message=json.dumps(result,separators=(',',':'))
    print('::notice title=Original runtime published provenance::'+message.replace('%','%25').replace('\r','%0D').replace('\n','%0A'))


if __name__=='__main__':main()
