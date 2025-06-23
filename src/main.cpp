//
// Created by Yeo Shu Heng on 15/6/25.
//

#include "MarketDataFeedHandler.cpp"
#include <spdlog/spdlog.h>
#include "spdlog/sinks/stdout_color_sinks-inl.h"

int main() {

    const auto console = spdlog::stderr_color_mt("console");
    console->set_level(spdlog::level::info);
    spdlog::set_pattern("[%H:%M:%S] [%^%l%$] %v");
    spdlog::info("logger initialised.");

    net::io_context ioc;

    ssl::context soc(ssl::context::tlsv12_client);
    soc.load_verify_file("/opt/homebrew/etc/openssl@3/cert.pem");
    soc.set_default_verify_paths();

    boost::lockfree::queue<TickData> tick_queue(1024);

    auto feed_handler = MarketDataFeedHandler("btcusdt", 5, 100, ioc, soc, tick_queue);
    feed_handler.start_feeds();
}
