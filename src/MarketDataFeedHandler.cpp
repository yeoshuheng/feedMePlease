//
// Created by Yeo Shu Heng on 17/6/25.
//
#include <spdlog/spdlog.h>
#include "boost/lockfree/queue.hpp"
#include <iostream>
#include "feeds/MarketDataFeeds.cpp"
#include "utils/Concurrency.cpp"
#include "feeds/tick/TickDataBuffer.cpp"

class MarketDataFeedHandler {

    std::string symbol;

    const int snapshot_frequency_ms;

    net::io_context& ioc;
    ssl::context& soc;

    boost::lockfree::queue<TickData> &spot_tick_queue;
    boost::lockfree::queue<TickData> &perp_tick_queue;

    TickDataBuffer spot_buffer;
    TickDataBuffer perp_buffer;

    std::atomic<bool> is_running{false};

    std::atomic<int64_t> last_snapshot_time_ns{0};
    std::atomic<int64_t> total_jitter_ns{0};
    std::atomic<size_t> jitter_count{0};

    /**
     * Tick processing related code.
     */
    void process_tick(const TickData &tick) {
        TickDataBuffer* data_buffer_ptr = nullptr;
        if (std::strncmp(tick.type, "SPOT", 4) == 0) {
            data_buffer_ptr = &spot_buffer;
        } else if (std::strncmp(tick.type, "PERP", 4) == 0) {
            data_buffer_ptr = &perp_buffer;
        } else {
            return;
        }

        if (!data_buffer_ptr) return;
        TickDataBuffer& data_buffer = *data_buffer_ptr;

        const uint64_t old_version = data_buffer.version.load(std::memory_order_acquire);
        const uint64_t new_version = old_version + 1;
        const int write_idx = new_version % 2;
        data_buffer.data_buffer[write_idx] = std::move(tick);
        data_buffer.version.store(new_version, std::memory_order_release);
    };

    void poll_ticker_queue(boost::lockfree::queue<TickData>& queue) {
        TickData tick;

        // batch poll to reduce contention.
        constexpr int batch_size = 1;
        std::vector<TickData> batch;
        batch.reserve(batch_size);

        while (is_running.load()) {
            while (queue.pop(tick)) {
                batch.push_back(std::move(tick));
                if (batch.size() == batch_size) break;
            }

            for (const auto& t : batch) {
                process_tick(t);
            }
            batch.clear();
        }
        spdlog::info("shutting down feed polling");
    }

    /**
    * Snapshot processing related code.
    */
    void process_snapshot() {
    const int64_t expected_interval_ns = snapshot_frequency_ms * 1'000'000LL;

    while (is_running.load()) {
        spin_wait(std::chrono::milliseconds(snapshot_frequency_ms));

        const int64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        if (const int64_t last_ns = last_snapshot_time_ns.load(std::memory_order_acquire); last_ns != 0) {
            const int64_t now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            const int64_t actual_interval_ns = now_ns - last_ns;
            const int64_t jitter = actual_interval_ns - expected_interval_ns;

            total_jitter_ns.fetch_add(std::abs(jitter), std::memory_order_relaxed);
            jitter_count.fetch_add(1, std::memory_order_relaxed);

            double avg_jitter = static_cast<double>(total_jitter_ns.load()) / jitter_count.load();

            spdlog::info("Snapshot jitter (ns): current={} avg={}", jitter, avg_jitter);

            last_snapshot_time_ns.store(now_ns, std::memory_order_release);
        }

        TickData spot_snapshot;
        read_snapshot(spot_buffer, spot_snapshot);

        TickData perp_snapshot;
        read_snapshot(perp_buffer, perp_snapshot);

        std::cout << "LATEST SNAPSHOT" << "\n";
        std::cout << tick_to_string(spot_snapshot) << "\n";
        std::cout << tick_to_string(perp_snapshot) << "\n";
        std::flush(std::cout);
    }
}


    void read_snapshot(const TickDataBuffer& buffer, TickData& out_tick) {
        const TickData* ptr = nullptr;
        uint64_t v0, v1;
        do {
            v0 = buffer.version.load(std::memory_order_acquire);
            const int read_idx = v0 % 2;
            out_tick = buffer.data_buffer[read_idx];
            v1 = buffer.version.load(std::memory_order_acquire);
        } while (v0 != v1);
    }

public:
    MarketDataFeedHandler(const std::string &symbol,
        const int snapshot_frequency_ms,
        net::io_context& ioc,
        ssl::context& soc,
        boost::lockfree::queue<TickData> &spot_tick_queue,
         boost::lockfree::queue<TickData> &perp_tick_queue)
    : symbol(symbol), snapshot_frequency_ms(snapshot_frequency_ms), ioc(ioc), soc(soc), spot_tick_queue(spot_tick_queue), perp_tick_queue(perp_tick_queue) {}

    ~MarketDataFeedHandler() {
        kill_feeds();
    }

    void start_feeds() {
        is_running = true;

        MarketDataFeeds::start_feeds(symbol, ioc, soc, spot_tick_queue, perp_tick_queue);

        std::thread io_thread([this]() { ioc.run();});
        std::thread spot_consumer_thread([this]() { poll_ticker_queue(spot_tick_queue); });
        std::thread perp_consumer_thread([this]() { poll_ticker_queue(perp_tick_queue); });

        std::thread snapshot_thread([this]() {process_snapshot();});

        set_affinity(spot_consumer_thread, 0);
        set_affinity(perp_consumer_thread, 1);;
        set_affinity(snapshot_thread, 2);

        spot_consumer_thread.join();
        perp_consumer_thread.join();
        snapshot_thread.join();
        io_thread.join();
    }

    void kill_feeds() {
        is_running = false;
        ioc.stop();
    }
};