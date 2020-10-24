#!/usr/bin/env python
"""
This example demonstrates serving a Flask application
that returns a rendered web-page with static content (Bootstrap CSS and a picture)
"""

import os
import sys
from flask import Flask, render_template
import wsgi_boost

cwd = os.path.dirname(os.path.abspath(__file__))
app = Flask(__name__)


# Simple Flask application
@app.route('/')
@app.route('/<name>')
def hello_world(name='Roman'):
    return render_template('template.html', name=name)


if __name__ == '__main__':
    # Create a server on the default port 8000 with 4 threads
    httpd = wsgi_boost.WsgiBoostHttp(threads=4)
    # Serve static files by "static/*" path from "static" subfolder.
    # Also see template.html.
    httpd.add_static_route(r'^/static', os.path.join(cwd, 'static'))
    # Set Flask WSGI application to be served
    httpd.set_app(app)
    httpd.start()
