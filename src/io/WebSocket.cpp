//
// Created by Yeo Shu Heng on 15/6/25.
//

#include <string>

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/ssl.hpp>
#include <spdlog/spdlog.h>

namespace beast = boost::beast;
namespace net = boost::asio;
namespace websocket = beast::websocket;
namespace ssl = boost::asio::ssl;

using tcp = net::ip::tcp;

typedef websocket::stream<beast::ssl_stream<tcp::socket>> ws;

class WebSocket : public std::enable_shared_from_this<WebSocket> {

    std::string host;
    std::string port;
    std::string target;

    ws socket;
    beast::flat_buffer buffer;

    net::io_context &ioc;
    ssl::context &soc;

    tcp::resolver resolver;

    void static log_error(const std::string& stage, const beast::error_code &ec) {
        spdlog::error("socket error in stage {}, {}", stage, ec.message());
    }

    void read() {
        socket.async_read(buffer,
            [self = shared_from_this()](const beast::error_code &ec, std::size_t bytes_transferred) {
                if (ec) {return self->log_error("read", ec);}
                self->handle_response(beast::buffers_to_string(self->buffer.data()));
                self->buffer.consume(bytes_transferred);
                self->read();
            });
    }

protected:
    virtual void handle_response(const std::string &msg) {
        spdlog::info(msg);
    }

    virtual ~WebSocket() = default;

public:
    WebSocket(boost::asio::io_context &ioc, ssl::context &soc, const std::string &host, const std::string &port, const std::string &target)
        : host(host), port(port), target(target),
        socket(ioc, soc), ioc(ioc), soc(soc), resolver(ioc) {}

    void connect() {
        beast::error_code ec;
        const auto results = resolver.resolve(host, port, ec);
        if (ec) {
            log_error("resolve", ec);
            throw beast::system_error(ec);
        }
        net::connect(socket.next_layer().next_layer(), results, ec);
        if (ec) {
            log_error("connect", ec);
            throw beast::system_error(ec);
        }
        if (!SSL_set_tlsext_host_name(socket.next_layer().native_handle(), host.c_str())) {
            beast::error_code sni_ec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
            log_error("SNI setup", sni_ec);
            throw beast::system_error(sni_ec);
        }
        socket.next_layer().handshake(ssl::stream_base::client, ec);
        if (ec) {
            log_error("ssl handshake", ec);
            throw beast::system_error(ec);
        }
        socket.handshake(host, target, ec);
        if (ec) {
            log_error("websocket handshake", ec);
            throw beast::system_error(ec);
        }
        spdlog::info("socket [{}] connected", to_string());
        read();
    }

    void send(const std::string &msg) {
        socket.async_write(boost::asio::buffer(msg),
            [self = shared_from_this()] (const beast::error_code &ec, std::size_t bytes_transferred) {
                if (ec) {return WebSocket::log_error("write", ec);}
            });
    }

    virtual std::string to_string() const {
        return std::format("host = {} port = {} target = {}", host, port, target);
    }

    [[nodiscard]] std::string get_host() const {return host;}
    [[nodiscard]] std::string get_port() const {return port;}
    [[nodiscard]] std::string get_target() const {return target;}
};