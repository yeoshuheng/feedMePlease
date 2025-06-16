//
// Created by Yeo Shu Heng on 16/6/25.
//
#include <cstdint>
#include <cstring>

struct TickData {

    char venue[16];
    char symbol[32];
    char type[8];

    double price;

    int64_t event_time_ms;
    int64_t received_time_ns;

    double funding_rate;
    int64_t next_funding_time_ms;

    TickData() :
        price(0.0),
        event_time_ms(0),
        received_time_ns(0),
        funding_rate(NAN),
        next_funding_time_ms(-1)
    {
        std::memset(venue, 0, sizeof(venue));
        std::memset(symbol, 0, sizeof(symbol));
        std::memset(type, 0, sizeof(type));
    }
};

std::string tick_to_string(const TickData& tick) {
    std::ostringstream oss;

    oss << "Venue: " << tick.venue
        << ", Symbol: " << tick.symbol
        << ", Type: " << tick.type
        << ", Price: " << std::fixed << std::setprecision(8) << tick.price
        << ", EventTime(ms): " << tick.event_time_ms
        << ", ReceivedTime(ns): " << tick.received_time_ns;

    if (!std::isnan(tick.funding_rate)) {
        oss << ", FundingRate: " << tick.funding_rate;
    } else {
        oss << ", FundingRate: N/A";
    }

    if (tick.next_funding_time_ms != -1) {
        oss << ", NextFundingTime(ms): " << tick.next_funding_time_ms;
    } else {
        oss << ", NextFundingTime(ms): N/A";
    }

    return oss.str();
}