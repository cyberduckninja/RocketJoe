#include "amqp_consumer.hpp"

#include <amqp_tcp_socket.h>

#include <components/log/log.hpp>

using namespace components;

amqp_consumer::amqp_consumer(const std::string& url) : _url(url) {
    if (_url.scheme() != "amqp") {
        throw std::runtime_error("URL scheme must be: amqp, provided: " + std::string{_url.scheme()});
    }
}

std::string amqp_consumer::get_host() const {
    if (_url.host().size()) {
        return _url.host();
    }
    return "localhost";
}

int amqp_consumer::get_port() const {
    std::string port{_url.port()};
    if (port.size()) {
        return stoi(port);
    }
    return 5672;
}

std::string amqp_consumer::get_user() const {
    if (_url.user().size()) {
        return _url.user();
    }
    return "guest";
}

std::string amqp_consumer::get_password() const {
    if (_url.password().size()) {
        return _url.password();
    }
    return "guest";
}

void amqp_consumer::throw_on_amqp_error(amqp_rpc_reply_t x, const char* context) const {
  switch (x.reply_type) {
    case AMQP_RESPONSE_NORMAL:
      return;

    case AMQP_RESPONSE_NONE:
      throw std::runtime_error("%s: missing RPC reply type!\n", context);
      break;

    case AMQP_RESPONSE_LIBRARY_EXCEPTION:
      throw std::runtime_error("%s: %s\n", context, amqp_error_string2(x.library_error));
      break;

    case AMQP_RESPONSE_SERVER_EXCEPTION:
      switch (x.reply.id) {
        case AMQP_CONNECTION_CLOSE_METHOD: {
          amqp_connection_close_t *m =
              (amqp_connection_close_t *)x.reply.decoded;
          throw std::runtime_error("%s: server connection error %uh, message: %.*s\n",
                  context, m->reply_code, (int)m->reply_text.len,
                  (char *)m->reply_text.bytes);
          break;
        }
        case AMQP_CHANNEL_CLOSE_METHOD: {
          amqp_channel_close_t *m = (amqp_channel_close_t *)x.reply.decoded;
          throw std::runtime_error("%s: server channel error %uh, message: %.*s\n",
                  context, m->reply_code, (int)m->reply_text.len,
                  (char *)m->reply_text.bytes);
          break;
        }
        default:
          throw std::runtime_error("%s: unknown server error, method id 0x%08X\n",
                  context, x.reply.id);
          break;
      }
      break;
  }
}

void amqp_consumer::start_loop() {
    amqp_socket_t* socket = NULL;
    amqp_connection_state_t conn;
    int error = 0;

    conn = amqp_new_connection();
    socket = amqp_tcp_socket_new(conn);
    if (!socket) {
        throw std::runtime_error("Cannot create socket");
    }

    error = amqp_socket_open(socket, get_host().c_str(), get_port());
    if (error) {
        throw std::runtime_error("Cannot listen on " + get_host() + ":" + std::to_string(get_port()));
    }

    throw_on_amqp_error(amqp_login(conn, "/", 0, 131072, 0, AMQP_SASL_METHOD_PLAIN, get_user().c_str(), get_password().c_str()), "Cannot login");

    amqp_channel_open(conn, 1);
    throw_on_amqp_error(amqp_get_rpc_reply(conn), "Cannot open channel");

    amqp_queue_declare_ok_t* declared = amqp_queue_declare(
        conn, 1, amqp_empty_bytes, 0, 0, 0, 1, amqp_empty_table);
    throw_on_amqp_error(amqp_get_rpc_reply(conn), "Cannot declare queue");

    amqp_bytes_t queuename = amqp_bytes_malloc_dup(declared->queue);

    std::string exchange = "celery";
    std::string binding_key = "celery";
    amqp_queue_bind(conn, 1, queuename, amqp_cstring_bytes(exchange.c_str()),
        amqp_cstring_bytes(binding_key.c_str()), amqp_empty_table);
    throw_on_amqp_error(amqp_get_rpc_reply(conn), "Cannot bind queue");

    amqp_basic_consume(conn, 1, queuename, amqp_empty_bytes, 0, 1, 0,
        amqp_empty_table);
    throw_on_amqp_error(amqp_get_rpc_reply(conn), "Cannot consume queue");

    get_logger().info("Listening for queue");

    while (true) {
        amqp_rpc_reply_t ret;
        amqp_envelope_t envelope;

        amqp_maybe_release_buffers(conn);
        ret = amqp_consume_message(conn, &envelope, NULL, 0);
        if (ret.reply_type != AMQP_RESPONSE_NORMAL) {
            amqp_channel_close(conn, 1, AMQP_REPLY_SUCCESS);
            amqp_connection_close(conn, AMQP_REPLY_SUCCESS);
            amqp_destroy_connection(conn);
            break;
        }

        // TODO process message - call task (how pass task context here?)

        amqp_destroy_envelope(&envelope);
    }
}

