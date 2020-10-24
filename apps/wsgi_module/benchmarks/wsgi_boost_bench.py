#!/usr/bin/env python
"""
WsgiBoostServer benchmark
"""

from wsgi_boost import WsgiBoostHttp
from test_app import app

if __name__ == '__main__':
    httpd = WsgiBoostHttp(threads=8)
    httpd.set_app(app)
    httpd.start()
