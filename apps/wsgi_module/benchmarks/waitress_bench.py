#!/usr/bin/env python
"""
Waitress benchmark application
"""

from waitress import serve
from test_app import app

if __name__ == '__main__':
    serve(app, host='0.0.0.0', port=8000, threads=8)
