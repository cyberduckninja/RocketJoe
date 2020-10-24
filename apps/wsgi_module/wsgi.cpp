#include "server.h"
#include "server_https.h"

namespace py = pybind11;
using namespace wsgi_boost;
using namespace std;

PYBIND11_PLUGIN(wsgi) {
    PyEval_InitThreads(); // Initialize GIL

    py::module module{"wsgi", "This module provides WSGI/HTTP server classes"};
    module.attr("__version__") = WSGI_BOOST_VERSION;
    module.attr("__author__") = "Roman Miroshnychenko";
    module.attr("__email__") = "romanvm@yandex.ua";
    module.attr("__license__") = "MIT";
    py::list all;

    py::class_<BaseServer<socket_ptr>>(module, "_BaseServerHttp");

    py::class_<HttpServer<socket_ptr>, BaseServer<socket_ptr>>(
        module,
        "WsgiBoostHttp",
        R"'''(
		WsgiBoostHttp(address='', port=8000, threads=0)

		PEP-3333-compliant multi-threaded WSGI server

		The server can serve both Python WSGI applications and static files.
		For static files gzip compression, 'If-None-Match' and 'If-Modified-Since' headers are supported.

		:param address: Server's address.
		:type address: str
		:param port: Server's port.
		:type port: int
		:param threads: The number of threads for the server to run.
		    Default: 0 (the number of threads is selected automatically depending on system parameters).
		:type threads: int

		Usage:

		.. code-block:: python

			from wsgi_boost import WsgiBoostHttp


			def hello_app(environ, start_response):
				content = b'Hello World!'
				status = '200 OK'
				response_headers = [('Content-type', 'text/plain'),
									('Content-Length', str(len(content)))]
				start_response(status, response_headers)
				return[content]


			httpd = WsgiBoostHttp(port=80, threads=4)
			httpd.set_app(hello_app)
			httpd.add_static_route('^/static', '/var/www/static-files')
			httpd.start()
		)'''")
        .def(py::init<string, unsigned short, unsigned int>(),
             py::arg("address") = string(), py::arg("port") = 8000, py::arg("threads") = 0)
        .def_property_readonly("is_running", &HttpServer<socket_ptr>::is_running, "Get server running status")
        .def_readwrite("use_gzip", &HttpServer<socket_ptr>::use_gzip, "Use gzip compression for static content, default: ``True``")
        .def_readwrite("host_hame", &HttpServer<socket_ptr>::host_name, "Get or set the host name, default: automatically determined")
        .def_readwrite("header_timeout", &HttpServer<socket_ptr>::header_timeout,
                       R"'''(
			Get or set timeout for receiving HTTP request headers

			This is the max. interval for reciving request headers before closing connection.
			Default: 5s
			)'''")
        .def_readwrite("content_timeout", &HttpServer<socket_ptr>::content_timeout,
                       R"'''(
			Get or set timeout for receiving POST/PUT content or sending response

			This is the max. interval for receiving POST/PUT content
			or sending response before closing connection.
			Default: 300s
			)'''")
        .def_readwrite("url_scheme", &HttpServer<socket_ptr>::url_scheme,
                       "Get or set URL scheme -- http or https (default: ``'http'``)")
        .def_readwrite("static_cache_control", &HttpServer<socket_ptr>::static_cache_control,
                       R"'''(
			The value of ``Cache-Control`` HTTP header for static content
			(default: ``'public, max-age=3600'``)
			)'''")
        .def("start", &HttpServer<socket_ptr>::start,
             R"'''(
			Start processing HTTP requests
			
			.. note:: This method blocks the current thread until the server is stopped
				either by calling :meth:`WsgiBoostHttp.stop` or by pressing :kbd:`Ctrl+C`
			)'''")
        .def("stop", &HttpServer<socket_ptr>::stop, "Stop processing HTTP requests")
        .def("add_static_route", &HttpServer<socket_ptr>::add_static_route,
             py::arg("path"), py::arg("content_dir"),
             R"'''(
			Add a route for serving static files

			Allows to serve static files from ``content_dir``

			:param path: a path regex to match URLs for static files
			:type path: str
			:param content_dir: a directory with static files to be served
			:type content_dir: str

			.. note:: ``path`` parameter is a regex that must start with ``^/``, for example :regexp:`^/static``
				Static URL paths have priority over WSGI application paths,
			    that is, if you set a static route for path :regexp:`^/` *all* requests for ``http://example.com/
			    will be directed to that route and a WSGI application will never be reached.
			)'''")
        .def("set_app", &HttpServer<socket_ptr>::set_app, py::arg("app"),
             R"'''(
			Set a WSGI application to be served

			:param app: a WSGI application to be served as an executable object
			:type app: object
			:raises RuntimeError: on attempt to set a WSGI application while the server is running
			)'''");

    all.append("WsgiBoostHttp");

    py::class_<BaseServer<ssl_socket_ptr>>(module, "_BaseServerHttps");

    py::class_<HttpsServer<ssl_socket_ptr>, BaseServer<ssl_socket_ptr>>(
        module,
        "WsgiBoostHttps",
        R"'''(
		WsgiBoostHttps(cert_chain, private_key, dh='', address='', port=4443, threads=0)

		PEP-3333-compliant multi-threaded WSGI server with HTTPS support
		
		:param cert_chain: path to *full* certificate chain file
		:type cert_chain: str
		:param private_key: path to private key file
		:type private_key: str
		:param dh: path to Diffie-Hellman parameters file (optional)
		:type dt: str
		:param address: server's address.
		:type address: str
		:param port: Server's port.
		:type port: int
		:param threads: The number of threads for the server to run.
		    Default: 0 (the number of threads is selected automatically depending on system parameters).
		:type threads: int

		Usage:

		.. code-block:: python

			from wsgi_boost import WsgiBoostHttps


			def hello_app(environ, start_response):
				content = b'Hello World!'
				status = '200 OK'
				response_headers = [('Content-type', 'text/plain'),
									('Content-Length', str(len(content)))]
				start_response(status, response_headers)
				return[content]


			httpd = WsgiBoostHttps('fullchain.pem', 'privkey.pem', port=443, threads=4)
			httpd.set_app(hello_app)
			httpd.add_static_route('^/static', '/var/www/static-files')
			httpd.start()
		)'''")
        .def(py::init<string, string, string, string, unsigned short, unsigned int>(),
             py::arg("cert_chain"), py::arg("private_key"), py::arg("dh") = string(),
             py::arg("address") = string(), py::arg("port") = 4443, py::arg("threads") = 0)
        .def_property_readonly("is_running", &HttpsServer<ssl_socket_ptr>::is_running)
        .def_readwrite("use_gzip", &HttpsServer<ssl_socket_ptr>::use_gzip)
        .def_readwrite("host_hame", &HttpsServer<ssl_socket_ptr>::host_name)
        .def_readwrite("header_timeout", &HttpsServer<ssl_socket_ptr>::header_timeout)
        .def_readwrite("content_timeout", &HttpsServer<ssl_socket_ptr>::content_timeout)
        .def_readwrite("url_scheme", &HttpsServer<ssl_socket_ptr>::url_scheme, "Default: ``'https'``")
        .def_readwrite("static_cache_control", &HttpsServer<ssl_socket_ptr>::static_cache_control)
        .def_readwrite("redirect_http", &HttpsServer<ssl_socket_ptr>::redirect_http,
                       "Enable redirecting HTTP requests to HTTPS port (default:: ``False``)")
        .def_readwrite("redirect_http_port", &HttpsServer<ssl_socket_ptr>::redirect_http_port,
                       "HTTP port to redirect requests from (default: ``80``)")
        .def("start", &HttpsServer<ssl_socket_ptr>::start)
        .def("stop", &HttpsServer<ssl_socket_ptr>::stop)
        .def("add_static_route", &HttpsServer<ssl_socket_ptr>::add_static_route,
             py::arg("path"), py::arg("content_dir"))
        .def("set_app", &HttpsServer<ssl_socket_ptr>::set_app, py::arg("app"));

    all.append("WsgiBoostHttps");

    py::class_<InputStream<Connection<ssl_socket_ptr>>>(module, "HttpsInputStream", "wsgi.input")
        .def("readlines", &InputStream<Connection<ssl_socket_ptr>>::readlines, py::arg("sizehint") = -1)
        .def("__iter__", &InputStream<Connection<ssl_socket_ptr>>::iter, py::return_value_policy::reference_internal)
        .def("read", &InputStream<Connection<ssl_socket_ptr>>::read, py::arg("size") = -1)
        .def("readline", &InputStream<Connection<ssl_socket_ptr>>::readline, py::arg("size") = -1)
        .def("__next__", &InputStream<Connection<ssl_socket_ptr>>::next);

    py::class_<InputStream<Connection<socket_ptr>>>(module, "HttpInputStream", "wsgi.input")
        .def("readlines", &InputStream<Connection<socket_ptr>>::readlines, py::arg("sizehint") = -1)
        .def("__iter__", &InputStream<Connection<socket_ptr>>::iter, py::return_value_policy::reference_internal)
        .def("read", &InputStream<Connection<socket_ptr>>::read, py::arg("size") = -1)
        .def("readline", &InputStream<Connection<socket_ptr>>::readline, py::arg("size") = -1)
        .def("__next__", &InputStream<Connection<socket_ptr>>::next);

    py::class_<ErrorStream>(module, "ErrorStream", "wsgi.errors")
        .def("write", &ErrorStream::write)
        .def("writelines", &ErrorStream::writelines)
        .def("flush", &ErrorStream::flush);

    py::class_<file_wrapper>(module, "file_wrapper", "wsgi.file_wrapper")
        .def("__call__", &file_wrapper::call, py::arg("file"), py::arg("block_size") = 8192,
             py::return_value_policy::reference_internal)
        .def("read", &file_wrapper::read, (py::arg("size") = -1))
        .def("__iter__", &file_wrapper::iter, py::return_value_policy::reference_internal)
        .def("__next__", &file_wrapper::next)
        .def("close", &file_wrapper::close);

    module.attr("__all__") = all;
    return module.ptr();
}
