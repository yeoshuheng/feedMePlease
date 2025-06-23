//
// Created by Yeo Shu Heng on 17/6/25.
//
#include <spdlog/spdlog.h>
#include "boost/lockfree/queue.hpp"
#include <iostream>

#include "feeds/MarketDataFeeds.cpp"
#include "utils/ThreadAffinity.cpp"
#include "feeds/tick/TickDataBuffer.cpp"


typedef std::unordered_map<std::string, std::unordered_map<std::string, TickDataBuffer>> snapshot;

class MarketDataFeedHandler {

    std::string symbol;

    const int snapshot_frequency_ms;
    const int max_lag_ms;

    net::io_context& ioc;
    ssl::context& soc;

    boost::lockfree::queue<TickData> &tick_queue;

    std::shared_ptr<snapshot> price_snapshot;

    std::atomic<bool> is_running{false};

    void pre_load_all_maps() {
        price_snapshot = std::make_shared<snapshot>();
        spdlog::info("starting feed for {} with snapshot frequency {}", symbol, snapshot_frequency_ms);
        for (const std::vector<std::string> types = {"SPOT", "PERP"}; std::string type : types) {
            (*price_snapshot)[type].try_emplace(to_upper(symbol));
        }
    }

    /**
     * Tick processing related code.
     */
    void process_tick(const TickData &tick) {
        auto snap_type_it = price_snapshot->find(tick.type);
        if (snap_type_it == price_snapshot->end()) {return;}
        auto& snap_symbol_map = snap_type_it->second;
        auto snap_symbol_it = snap_symbol_map.find(tick.symbol);
        if (snap_symbol_it == snap_symbol_map.end()) {return;}

        TickDataBuffer& data_buffer = snap_symbol_it->second;

        const uint64_t old_version = data_buffer.version.load(std::memory_order_acquire);
        const uint64_t new_version = old_version + 1;
        const int write_idx = new_version % 2;

        data_buffer.data_buffer[write_idx] = std::make_unique<TickData>(tick);
        data_buffer.version.store(new_version, std::memory_order_release);
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

    /**
    * Snapshot processing related code.
    */
    void process_snapshot() {
        while (is_running.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(snapshot_frequency_ms));

            const int64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

            std::cout << "SNAPSHOT" << "\n";
            for (auto& [type, symbol_map] : *price_snapshot) {
                for (auto& [symbol, buffer] : symbol_map) {
                    TickData curr_snapshot;
                    read_snapshot(buffer, curr_snapshot);

                    if (curr_snapshot.event_time_ms == 0) {
                        continue;
                    }
                    if (now_ms - curr_snapshot.received_time_ns > max_lag_ms) {
                        spdlog::warn("Dropping stale tick for {} {}: event_time_ms={}, now_ms={}",
                                     type, symbol, curr_snapshot.event_time_ms, now_ms);
                        continue;
                    }
                    std::cout << tick_to_string(curr_snapshot) << "\n";
                }
            }
            std::flush(std::cout);
        }
    }

    void read_snapshot(const TickDataBuffer& buffer, TickData& out_tick) {
        const TickData* ptr = nullptr;
        uint64_t v0, v1;
        do {
            v0 = buffer.version.load(std::memory_order_acquire);
            const int read_idx = v0 % 2;
            ptr = buffer.data_buffer[read_idx].get();
            if (!ptr) {return;}

            out_tick = *ptr;

            v1 = buffer.version.load(std::memory_order_acquire);
        } while (v0 != v1);
    }

public:
    MarketDataFeedHandler(const std::string &symbol, const int max_lag_ms, const int snapshot_frequency_ms, net::io_context& ioc, ssl::context& soc, boost::lockfree::queue<TickData> &tick_queue)
    : symbol(symbol), max_lag_ms(max_lag_ms), snapshot_frequency_ms(snapshot_frequency_ms), ioc(ioc), soc(soc), tick_queue(tick_queue) {
        pre_load_all_maps();
    }

    ~MarketDataFeedHandler() {
        kill_feeds();
    }

    void start_feeds() {
        is_running = true;

        MarketDataFeeds::start_feeds(symbol, ioc, soc, tick_queue);

        std::thread io_thread([this]() {ioc.run();});

        std::thread consumer_thread([&]() {poll_ticker_queue();});

        std::thread snapshot_thread([&]() {process_snapshot();});

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