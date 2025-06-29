//
// Created by Yeo Shu Heng on 15/6/25.
//


#include "MarketDataFeed.cpp"
#include "MarketDataFeedCallbacks.cpp"
#include "boost/lockfree/queue.hpp"
#include "SynchroniseTime.cpp"

class MarketDataFeeds {

public:

    static void start_feeds(
        const std::string& symbol,
        net::io_context& ioc,
        ssl::context& soc,
        boost::lockfree::queue<TickData> &spot_queue,
        boost::lockfree::queue<TickData> &perp_queue) {

        auto binance_funding_map = std::make_shared<funding_map>();
        binance_funding_map->emplace(to_upper(symbol), std::make_shared<std::atomic<double>>(-1.0));

        const int64_t binance_offset_ms = compute_time_offset();

        const auto binance_futures_data_feed = std::make_shared<MarketDataFeed>(
           "binance_futures_feed",
           ioc, soc,
           "fstream.binance.com",
           "443",
           "/ws",
           binance_callback_futures(perp_queue,
               binance_funding_map, binance_offset_ms));

        binance_futures_data_feed->connect();

        const auto binance_spot_data_feed = std::make_shared<MarketDataFeed>(
            "binance_spot_feed",
            ioc, soc,
            "stream.binance.com",
            "9443",
            "/ws",
            binance_callback_spot(spot_queue, binance_offset_ms));

        binance_spot_data_feed->connect();


        binance_spot_data_feed->send(
            std::format(R"({{"method":"SUBSCRIBE","params":["{}@aggTrade"],"id":1}})", symbol));

        binance_futures_data_feed->send(
            std::format(R"({{"method":"SUBSCRIBE","params":["{}@aggTrade","{}@markPrice"],"id":1}})",
                symbol, symbol));
    }
};