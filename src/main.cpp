//
// Created by Yeo Shu Heng on 15/6/25.
//

#include "FeedHandler.cpp"
#include <spdlog/spdlog.h>
#include "spdlog/sinks/stdout_color_sinks-inl.h"

#include "boost/lockfree/queue.hpp"

#include <string>
#include <iostream>

struct TickData;

int main() {

    const auto console = spdlog::stderr_color_mt("console");
    console->set_level(spdlog::level::debug);
    spdlog::set_pattern("[%H:%M:%S] [%^%l%$] %v");
    spdlog::info("logger initialised.");

    net::io_context ioc;

    ssl::context soc(ssl::context::tlsv12_client);
    soc.load_verify_file("/opt/homebrew/etc/openssl@3/cert.pem");
    soc.set_default_verify_paths();


    boost::lockfree::queue<TickData> tick_queue(1024);

    FeedHandler::start_feeds(ioc, soc, tick_queue);

    const std::atomic<bool> running = true;

    std::thread consumer_thread([&]() {
        TickData tick;
        while (running.load()) {
            while (tick_queue.pop(tick)) {
                std::cout << tick_to_string(tick) << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    std::thread io_thread([&ioc]() {ioc.run();});

    io_thread.join();
}
