//
// Created by Yeo Shu Heng on 16/6/25.
//

#include <string>
#include <spdlog/spdlog.h>
#include "boost/lockfree/queue.hpp"
#include "nlohmann/json.hpp"
#include "tick/TickData.cpp"
#include "tick/Venue.cpp"
#include "tick/InstrumentType.cpp"
#include "../utils/StringModifications.cpp"

typedef std::unordered_map<std::string, std::shared_ptr<std::atomic<double>>> funding_map;
using json = nlohmann::json;

std::function<void(const std::string&)> binance_callback_spot(boost::lockfree::queue<TickData> &tick_queue) {
    return [&tick_queue](const std::string& resp) {
        const auto received_ts_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();

        auto j = json::parse(resp, nullptr, false);

        if (j.is_discarded()) {
            spdlog::warn("Failed to parse JSON: {}", resp);
            return;
        }

        if (j.contains("result") && j["result"].is_null()) {
            spdlog::info("Subscription confirmed for binance spot stream.");
            return;
        }

        TickData tick;
        std::memset(&tick, 0, sizeof(TickData));

        std::strncpy(tick.venue, VenueToString(BINANCE), sizeof(tick.venue) - 1);
        std::strncpy(tick.symbol, j.value("s", "").c_str(), sizeof(tick.symbol) - 1);
        std::strncpy(tick.type, InstrumentTypeToString(SPOT), sizeof(tick.type) - 1);

        tick.price = std::stod(j.value("p", "0"));
        tick.event_time_ms = j.value("E", 0);
        tick.received_time_ns = received_ts_ns;
        tick.funding_rate = NAN;
        tick.next_funding_time_ms = -1;

        if (!tick_queue.push(tick)) {
            spdlog::warn("tick queue full, dropped message with timestamp {}", received_ts_ns);
        }
    };
}

std::function<void(const std::string&)> binance_callback_futures(
    boost::lockfree::queue<TickData> &tick_queue,
    std::shared_ptr<funding_map> funding_map) {

    return [&tick_queue, funding_map](const std::string& resp) {
        const auto received_ts_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();

        auto j = json::parse(resp, nullptr, false);
        if (j.is_discarded()) {
            spdlog::warn("Failed to parse JSON: {}", resp);
            return;
        }

        if (j.contains("result") && j["result"].is_null()) {
            spdlog::info("Subscription confirmed for binance futures stream.");
            return;
        }

        const std::string event_type = j.value("e", "");
        const std::string symbol = j.value("s", "");

        if (event_type == "markPriceUpdate") {

            double rate = std::stod(j.value("r", "nan"));
            int64_t next_time = j.value("T", -1);

            (*funding_map)[to_upper(symbol)]->store(rate);

            spdlog::info("funding rate update for future: symbol={}, rate={}, next_time={}", symbol, rate, next_time);

        } else if (event_type == "aggTrade") {

            TickData tick;
            std::memset(&tick, 0, sizeof(TickData));
            std::strncpy(tick.venue, VenueToString(BINANCE), sizeof(tick.venue) - 1);
            std::strncpy(tick.symbol, symbol.c_str(), sizeof(tick.symbol) - 1);
            std::strncpy(tick.type, InstrumentTypeToString(PERP), sizeof(tick.type) - 1);

            tick.price = std::stod(j.value("p", "0"));
            tick.event_time_ms = j.value("E", 0);
            tick.received_time_ns = received_ts_ns;

            tick.funding_rate = (*funding_map)[to_upper(symbol)]->load();

            if (!tick_queue.push(tick)) {
                spdlog::warn("tick queue full, dropped message with timestamp {}", received_ts_ns);
            }
        };
    };
}