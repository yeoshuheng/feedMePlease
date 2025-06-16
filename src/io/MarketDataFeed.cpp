//
// Created by Yeo Shu Heng on 15/6/25.
//

#include "WebSocket.cpp"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

class MarketDataFeed : public WebSocket {

    std::string feed_name;

    std::function<void(const std::string&)> callback;

protected:
    void handle_response(const std::string &msg) override {
        callback(msg);
    }

public:
    MarketDataFeed(const std::string &feed_name, boost::asio::io_context &ioc, ssl::context &soc,
        const std::string &host, const std::string &port,
        const std::string &target, const std::function<void(const std::string&)> &callback)
        : WebSocket(ioc, soc, host, port, target), feed_name(feed_name), callback(callback) {}

    std::string to_string() const override {
        return std::format("host = {} port = {} target = {}", get_host(), get_port(), get_target());
    }
};