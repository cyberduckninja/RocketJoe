#!/usr/bin/env python
# coding: utf-8
"""
Test suite for testing core WsgiBoostServer functionality
"""

from __future__ import print_function
import os
import sys
import threading
import time
import unittest
from wsgiref.validate import validator
from io import BytesIO
import requests

cwd = os.path.dirname(os.path.abspath(__file__))
os.chdir(cwd)
sys.path.insert(0, os.path.dirname(cwd))

import wsgi_boost


class App(object):
    """
    Test WSGI application
    """

    def __call__(self, environ, start_response):
        self.environ = environ
        self.start_response = start_response
        headers = [('Content-type', 'text/plain')]
        if self.environ['PATH_INFO'] == '/test_http_header':
            content = self.test_http_header()
        elif self.environ['PATH_INFO'] == '/test_query_string':
            content = self.test_query_string()
        elif self.environ['PATH_INFO'] == '/test_input_read':
            content = self.test_input_read()
        elif self.environ['PATH_INFO'] == '/test_input_read_limited':
            content = self.test_input_read_limited()
        elif self.environ['PATH_INFO'] == '/test_input_readline':
            content = self.test_input_readline()
        elif self.environ['PATH_INFO'] == '/test_input_readlines':
            content = self.test_input_readlines()
        elif self.environ['PATH_INFO'] == '/test_input_iterator':
            content = self.test_input_iterator()
        elif self.environ['PATH_INFO'] == '/test_write':
            content = b'Write OK'
        elif self.environ['PATH_INFO'] == '/test_transfer_chunked':
            content = b'Transfer chunked OK'
        elif self.environ['PATH_INFO'] == '/test_file_wrapper':
            fo = BytesIO(b'File wrapper OK')
            content = self.environ['wsgi.file_wrapper'](fo)
        else:
            print(self.environ['wsgi.input'].read(-1))
            print(self.environ['wsgi.input'].readline())
            print(self.environ['wsgi.input'].readlines())
            self.environ['wsgi.errors'].write('foo\n')
            self.environ['wsgi.errors'].writelines(['ham\n', 'spam\n'])
            self.environ['wsgi.errors'].flush()
            content = b'App OK'
        if isinstance(content, bytes) and content != b'Transfer chunked OK':
            headers.append(('Content-Length', str(len(content))))
        write = start_response('200 OK', headers)
        if content == b'Write OK':
            write(content)
            content = b''
        if isinstance(content, bytes):
            content = [content]
        return content

    def test_http_header(self):
        assert self.environ['HTTP_FOO'] == 'bar'
        return b'HTTP header OK'

    def test_query_string(self):
        assert self.environ['QUERY_STRING'] == 'foo=bar'
        return b'Query string OK'

    def test_input_read(self):
        content = self.environ['wsgi.input'].read()
        assert len(content) == 4194
        return b'Input read OK'

    def test_input_read_limited(self):
        content = self.environ['wsgi.input'].read(50)
        assert len(content) == 50
        return b'Input read limited OK'

    def test_input_readline(self):
        line = self.environ['wsgi.input'].readline()
        assert line == b'Mark Twain. The Awful German Language\n'
        return b'Input readline OK'

    def test_input_readlines(self):
        lines = self.environ['wsgi.input'].readlines()
        assert isinstance(lines, list)
        assert len(lines) == 61
        content = b''.join(lines)
        assert len(content) == 4194
        return b'Input readlines OK'

    def test_input_iterator(self):
        lines = [l for l in self.environ['wsgi.input']]
        assert len(lines) == 61
        return b'Input iterator OK'


class ValidateWsgiServerComplianceTestCase(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls._httpd = wsgi_boost.WsgiBoostHttp(threads=1)
        app = App()
        cls._httpd.set_app(validator(app))
        cls._server_thread = threading.Thread(target=cls._httpd.start)
        cls._server_thread.daemon = True
        cls._server_thread.start()
        time.sleep(0.5)

    @classmethod
    def tearDownClass(cls):
        cls._httpd.stop()
        cls._server_thread.join()
        del cls._httpd
        print()

    def test_validate_wsgi_server_compliance(self):
        resp = requests.post('http://127.0.0.1:8000/', json={'foo': 'bar'})
        self.assertEqual(resp.status_code, 200)
        self.assertEqual(resp.text, 'App OK')


class WsgiServerFunctionsTestCase(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls._httpd = wsgi_boost.WsgiBoostHttp(threads=1)
        app = App()
        cls._httpd.set_app(app)
        cls._server_thread = threading.Thread(target=cls._httpd.start)
        cls._server_thread.daemon = True
        cls._server_thread.start()
        time.sleep(0.5)
        with open('german.txt', mode='r') as fo:
            cls._data = fo.read()

    @classmethod
    def tearDownClass(cls):
        cls._httpd.stop()
        cls._server_thread.join()
        del cls._httpd
        print()

    def test_http_header(self):
        resp = requests.get('http://127.0.0.1:8000/test_http_header', headers={'Foo': 'bar'})
        self.assertEqual(resp.status_code, 200)
        self.assertEqual(resp.text, 'HTTP header OK')

    def test_query_string(self):
        resp = requests.get('http://127.0.0.1:8000/test_query_string', params={'foo': 'bar'})
        self.assertEqual(resp.status_code, 200)
        self.assertEqual(resp.text, 'Query string OK')

    def test_write_function(self):
        resp = requests.get('http://127.0.0.1:8000/test_write')
        self.assertEqual(resp.status_code, 200)
        self.assertEqual(resp.text, 'Write OK')

    def test_input_read(self):
        resp = requests.post('http://127.0.0.1:8000/test_input_read', data=self._data)
        self.assertEqual(resp.status_code, 200)
        self.assertEqual(resp.text, 'Input read OK')

    def test_input_read_limited(self):
        resp = requests.post('http://127.0.0.1:8000/test_input_read_limited', data=self._data)
        self.assertEqual(resp.status_code, 200)
        self.assertEqual(resp.text, 'Input read limited OK')

    def test_input_readline(self):
        resp = requests.post('http://127.0.0.1:8000/test_input_readline', data=self._data)
        self.assertEqual(resp.status_code, 200)
        self.assertEqual(resp.text, 'Input readline OK')

    def test_input_readlines(self):
        resp = requests.post('http://127.0.0.1:8000/test_input_readlines', data=self._data)
        self.assertEqual(resp.status_code, 200)
        self.assertEqual(resp.text, 'Input readlines OK')

    def test_input_iterator(self):
        resp = requests.post('http://127.0.0.1:8000/test_input_iterator', data=self._data)
        self.assertEqual(resp.status_code, 200)
        self.assertEqual(resp.text, 'Input iterator OK')

    def test_transfer_chunked(self):
        resp = requests.get('http://127.0.0.1:8000/test_transfer_chunked')
        self.assertEqual(resp.status_code, 200)
        self.assertEqual(resp.headers['Transfer-Encoding'], 'chunked')
        self.assertEqual(resp.text, 'Transfer chunked OK')

    def test_file_wrapper(self):
        resp = requests.get('http://127.0.0.1:8000/test_file_wrapper')
        self.assertEqual(resp.status_code, 200)
        self.assertEqual(resp.text, 'File wrapper OK')


class ServingStaticFilesTestCase(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls._httpd = wsgi_boost.WsgiBoostHttp(threads=1)
        cls._httpd.add_static_route('^/static', cwd)
        cls._httpd.add_static_route('^/invalid_dir', '/foo/bar/baz/')
        cls._server_thread = threading.Thread(target=cls._httpd.start)
        cls._server_thread.daemon = True
        cls._server_thread.start()
        time.sleep(0.5)

    @classmethod
    def tearDownClass(cls):
        cls._httpd.stop()
        cls._server_thread.join()
        del cls._httpd
        print()

    def test_forbidden_http_methods(self):
        resp = requests.post('http://127.0.0.1:8000/static')
        self.assertEqual(resp.status_code, 405)

    def test_invalid_content_directory(self):
        resp = requests.get('http://127.0.0.1:8000/invalid_dir')
        self.assertEqual(resp.status_code, 500)
        self.assertTrue('Invalid static content directory is configured' in resp.text)

    def test_accessing_static_file(self):
        resp = requests.get('http://127.0.0.1:8000/static/index.html')
        self.assertEqual(resp.status_code, 200)
        self.assertTrue('Welcome to Our Company' in resp.text)
        resp = requests.get('http://127.0.0.1:8000/static/foo/bar.html')
        self.assertEqual(resp.status_code, 404)

    def test_gzipping_static_files(self):
        html_size = os.path.getsize('index.html')
        resp = requests.get('http://127.0.0.1:8000/static/index.html')
        self.assertTrue(html_size > int(resp.headers['Content-Length']))
        png_size = os.path.getsize('profile_pic.png')
        resp = requests.get('http://127.0.0.1:8000/static/profile_pic.png')
        self.assertEqual(png_size, int(resp.headers['Content-Length']))

    def test_not_modified_response(self):
        posix_time = 1419175200
        etag = '"' + hex(posix_time)[2:] + '"'
        os.utime(os.path.join(cwd, 'index.html'), (posix_time, posix_time))
        resp = requests.get('http://127.0.0.1:8000/static/index.html',
                            headers={'If-Modified-Since': 'Sun, 21 Dec 2014 15:19:00 GMT'})
        self.assertEqual(resp.status_code, 200)
        self.assertEqual(resp.headers.get("ETag"), etag)
        resp = requests.get('http://127.0.0.1:8000/static/index.html', headers={'If-None-Match': etag})
        self.assertEqual(resp.status_code, 304)
        resp = requests.get('http://127.0.0.1:8000/static/index.html',
                            headers={'If-Modified-Since': 'Sun, 21 Dec 2014 15:20:00 GMT'})
        self.assertEqual(resp.status_code, 304)
        resp = requests.get('http://127.0.0.1:8000/static/index.html',
                            headers={'If-Modified-Since': 'Mon, 21 Dec 2014 15:21:00 GMT'})
        self.assertEqual(resp.status_code, 304)

    def test_range_header(self):
        resp = requests.get('http://127.0.0.1:8000/static/profile_pic.png', headers={'Range': 'bytes=1024-2048'})
        self.assertEqual(resp.status_code, 206)
        self.assertEqual(resp.headers['Content-Range'], 'bytes 1024-2048/22003')
        self.assertEqual(resp.headers['Content-Length'], '1024')
        self.assertEqual(len(resp.content), 1024)
        resp = requests.get('http://127.0.0.1:8000/static/profile_pic.png', headers={'Range': 'bytes=1024-30000'})
        self.assertEqual(resp.status_code, 416)


try:
    wsgi_boost.WsgiBoostHttps
except AttributeError:
    pass
else:
    class HttpsServerTestCase(unittest.TestCase):
        @classmethod
        def setUpClass(cls):
            cert = os.path.join(cwd, 'server.crt')
            key = os.path.join(cwd, 'server.key')
            dh = os.path.join(cwd, 'dh2048.pem')
            cls._httpd = wsgi_boost.WsgiBoostHttps(cert, key, dh, threads=1)
            app = App()
            cls._httpd.redirect_http = True
            cls._httpd.redirect_http_port = 8000
            cls._httpd.set_app(app)
            cls._server_thread = threading.Thread(target=cls._httpd.start)
            cls._server_thread.daemon = True
            cls._server_thread.start()
            time.sleep(0.5)

        @classmethod
        def tearDownClass(cls):
            cls._httpd.stop()
            cls._server_thread.join()
            del cls._httpd
            print()

        def test_https_server(self):
            resp = requests.get('https://127.0.0.1:4443/', verify=False)
            self.assertEqual(resp.status_code, 200)
            self.assertEqual(resp.text, 'App OK')

        def test_http_redirect(self):
            resp = requests.get('http://127.0.0.1:8000/', verify=False)
            self.assertEqual(resp.status_code, 200)
            self.assertEqual(resp.text, 'App OK')

if __name__ == '__main__':
    unittest.main()
