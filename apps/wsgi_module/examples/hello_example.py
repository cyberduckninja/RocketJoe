#!/usr/bin/env python
"""
This example demonstrates serving a very simple WSGI application
"""

import os
import sys

cwd = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(cwd))

import wsgi_boost


def hello_app(environ, start_response):
    """Very simple WSGI application"""
    content = b'Hello World!'
    response_headers = [
        ('Content-type', 'text/plain'),
        ('Content-Length', str(len(content)))
    ]
    start_response('200 OK', response_headers)
    return [content]


if __name__ == '__main__':
    # Create a server on the default port 8000 with 4 threads
    httpd = wsgi_boost.WsgiBoostHttp(threads=4)
    httpd.set_app(hello_app)
    httpd.start()
