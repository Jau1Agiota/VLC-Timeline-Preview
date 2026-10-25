import importlib.util
import io
import json
import os
from pathlib import Path
import unittest
from unittest import mock
import urllib.error
import zipfile

ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('original_provenance',ROOT/'ci/windows/original-runtime-provenance.py')
p=importlib.util.module_from_spec(spec);spec.loader.exec_module(p)


class Response(io.BytesIO):
    status=200


class Provenance(unittest.TestCase):
    def test_no_credential_has_no_fallback(self):
        with mock.patch.dict(os.environ,{},clear=True):
            result=p.collect()
        self.assertEqual(result['status'],'credential unavailable')
        self.assertFalse(result['source_closure_verified'])

    def test_storage_request_has_no_authorization(self):
        raw=io.BytesIO()
        with zipfile.ZipFile(raw,'w') as z:
            z.writestr('Building mpv (x86_64)/log.txt','HEAD is now at abcdef123456\n')
        redirect=urllib.error.HTTPError(p.URL,302,'redirect',{'Location':'https://results-receiver.actions.githubusercontent.com/signed-fixture'},None)
        opener=mock.Mock();opener.open.side_effect=[redirect,Response(raw.getvalue())]
        with mock.patch.dict(os.environ,{'GITHUB_TOKEN':'fixture-not-a-credential'}),mock.patch.object(p.urllib.request,'build_opener',return_value=opener):
            result=p.collect()
        calls=opener.open.call_args_list
        self.assertIsNotNone(calls[0].args[0].get_header('Authorization'))
        self.assertIsNone(calls[1].args[0].get_header('Authorization'))
        self.assertFalse(result['source_closure_verified'])
        self.assertEqual(result['storage_http'],200)
        self.assertNotIn('signed-fixture',json.dumps(result))
        self.assertNotIn('fixture-not-a-credential',json.dumps(result))

    def test_unapproved_redirect_fails_closed(self):
        redirect=urllib.error.HTTPError(p.URL,302,'redirect',{'Location':'http://example.invalid'},None)
        opener=mock.Mock();opener.open.side_effect=redirect
        with mock.patch.dict(os.environ,{'GITHUB_TOKEN':'fixture'}),mock.patch.object(p.urllib.request,'build_opener',return_value=opener):
            result=p.collect()
        self.assertIn('unapproved redirect',result['error'])
        self.assertEqual(opener.open.call_count,1)

    def test_http_failure_does_not_expose_url(self):
        opener=mock.Mock();opener.open.side_effect=urllib.error.HTTPError(p.URL,403,'Denied',{},None)
        with mock.patch.dict(os.environ,{'GITHUB_TOKEN':'fixture'}),mock.patch.object(p.urllib.request,'build_opener',return_value=opener):
            result=p.collect()
        self.assertEqual(result['api_http'],403)
        self.assertNotIn('fixture',json.dumps(result))

    def test_bad_archive_never_becomes_clearance(self):
        opener=mock.Mock();opener.open.return_value=Response(b'not a zip')
        with mock.patch.dict(os.environ,{'GITHUB_TOKEN':'fixture'}),mock.patch.object(p.urllib.request,'build_opener',return_value=opener):
            result=p.collect()
        self.assertEqual(result['error'],'BadZipFile')
        self.assertFalse(result['source_closure_verified'])


if __name__=='__main__':unittest.main()
