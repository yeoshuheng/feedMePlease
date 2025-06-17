//
// Created by Yeo Shu Heng on 17/6/25.
//
#include <spdlog/spdlog.h>
#include <iostream>
#include "boost/lockfree/queue.hpp"

#include "feeds/MarketDataFeeds.cpp"
#include "utils/ThreadAffinity.cpp"
#include "utils/StringModifications.cpp"

typedef TickData* tick_data_ptr;
typedef std::unordered_map<std::string, std::unordered_map <std::string, std::unique_ptr<boost::lockfree::queue<TickData>>>> pmap;
typedef std::unordered_map<std::string, std::unordered_map<std::string, std::atomic<tick_data_ptr>>> snapshot;

class MarketDataFeedHandler {

    std::string symbol;

    const int snapshot_frequency_ms;

    net::io_context& ioc;
    ssl::context& soc;

    boost::lockfree::queue<TickData> &tick_queue;

    pmap price_map;
    snapshot price_snapshot;

    std::atomic<bool> is_running{false};

    void process_tick(const TickData &tick) {
        auto type_it = price_map.find(tick.type);
        if (type_it == price_map.end()) {
            spdlog::warn("Unknown type: {}", tick.type);
            return;
        }
        auto& symbol_map = type_it->second;
        auto symbol_it = symbol_map.find(tick.symbol);
        if (symbol_it == symbol_map.end() || !symbol_it->second) {
            spdlog::warn("Unknown symbol or null queue ptr: {}", tick.symbol);
            return;
        }
        auto& queue_ptr = symbol_it->second;
        queue_ptr->push(tick);
        auto snap_type_it = price_snapshot.find(tick.type);
        if (snap_type_it == price_snapshot.end()) return;
        auto& snap_symbol_map = snap_type_it->second;
        auto snap_symbol_it = snap_symbol_map.find(tick.symbol);
        if (snap_symbol_it == snap_symbol_map.end()) return;

        auto& atomic_ptr = snap_symbol_it->second;

        tick_data_ptr old_ptr = atomic_ptr.load(std::memory_order_acquire);
        tick_data_ptr new_ptr = new TickData(tick);
        while (!atomic_ptr.compare_exchange_weak(old_ptr, new_ptr,
                                                 std::memory_order_release,
                                                 std::memory_order_acquire))
            {delete new_ptr; new_ptr = new TickData(tick);}
        delete old_ptr;
    };

    void poll_ticker_queue() {
        TickData tick;
        while (is_running.load()) {
            while (tick_queue.pop(tick)) {
                spdlog::debug("got tick: {}", tick_to_string(tick));
                process_tick(tick);
            }
        }
        spdlog::info("shutting down feeds");
    }


    void display_snapshot() {
        while (is_running.load()) {
            spdlog::debug("getting snapshots");
            std::cout << "SNAPSHOT" << "\n";
            for (const auto& [type, symbol_map] : price_snapshot) {
                for (const auto& [symbol, atomic_tick_ptr] : symbol_map) {
                    if (const tick_data_ptr tick_ptr = atomic_tick_ptr.load(std::memory_order_acquire)) {
                        const std::string str = tick_to_string(*tick_ptr);
                        std::cout << type << " | " << symbol << " | " << str << "\n";
                    }
                }
            }
            std::cout << std::flush;
            std::this_thread::sleep_for(std::chrono::milliseconds(snapshot_frequency_ms));
        }
    }

    void pre_load_all_maps() {
        spdlog::info("starting feed for {} with snapshot frequency {}", symbol, snapshot_frequency_ms);
        for (const std::vector<std::string> types = {"SPOT", "PERP"}; std::string type : types) {
            price_map[type][to_upper(symbol)] = std::make_unique<boost::lockfree::queue<TickData>>(1024);
            price_snapshot[type][to_upper(symbol)].store(nullptr, std::memory_order_relaxed);
        }
    }

public:
    MarketDataFeedHandler(const std::string &symbol, const int snapshot_frequency_ms, net::io_context& ioc, ssl::context& soc, boost::lockfree::queue<TickData> &tick_queue)
    : symbol(symbol), snapshot_frequency_ms(snapshot_frequency_ms), ioc(ioc), soc(soc), tick_queue(tick_queue) {
        pre_load_all_maps();
    }

    ~MarketDataFeedHandler() {
        for (auto &symbol_map: price_snapshot | std::views::values) {
            for (auto &atomic_tick_ptr: symbol_map | std::views::values) {
                if (const tick_data_ptr ptr = atomic_tick_ptr.load(std::memory_order_acquire); ptr != nullptr) {
                    delete ptr;
                    atomic_tick_ptr.store(nullptr, std::memory_order_release);
                }
            }
        }
    }

    void start_feeds() {
        is_running = true;

        MarketDataFeeds::start_feeds(symbol, ioc, soc, tick_queue);

        std::thread io_thread([this]() {ioc.run();});

        std::thread consumer_thread([&]() {poll_ticker_queue();});

        std::thread snapshot_thread([&]() {display_snapshot();});

        set_affinity(consumer_thread, 0);
        set_affinity(io_thread, 1);
        set_affinity(snapshot_thread, 2);

        io_thread.join();
        consumer_thread.join();
    }

    void kill_feeds() {
        is_running = false;
        ioc.stop();
    }
};