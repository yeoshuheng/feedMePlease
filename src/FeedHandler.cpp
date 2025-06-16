//
// Created by Yeo Shu Heng on 15/6/25.
//


#include "io/MarketDataFeed.cpp"
#include "io/MarketDataFeedCallbacks.cpp";
#include "boost/lockfree/queue.hpp"

class FeedHandler {
public:
    static void start_feeds(net::io_context& ioc, ssl::context& soc, boost::lockfree::queue<TickData> &tick_queue) {

        auto const binance_futures_funding_map = std::make_shared<std::unordered_map<std::string, std::pair<double, int64_t>>>();
        auto const binance_futures_funding_map_mutex = std::make_shared<std::mutex>();

        const auto binance_futures_data_feed = std::make_shared<MarketDataFeed>(
           "binance_futures_feed",
           ioc, soc,
           "fstream.binance.com",
           "443",
           "/ws",
           binance_callback_futures(tick_queue,
               binance_futures_funding_map,
               binance_futures_funding_map_mutex));

        binance_futures_data_feed->connect();

        const auto binance_spot_data_feed = std::make_shared<MarketDataFeed>(
            "binance_spot_feed",
            ioc, soc,
            "stream.binance.com",
            "9443",
            "/ws",
            binance_callback_spot(tick_queue));

        binance_spot_data_feed->connect();

        const auto hyperliquid_futures_data_feed = std::make_shared<MarketDataFeed>(
            "hyperliquid_futures_feed",
            ioc, soc,
            "api.hyperliquid.xyz",
            "443",
            "/ws",
            hyperliquid_callback_futures(tick_queue)
        );

        hyperliquid_futures_data_feed->connect();

        const auto hyperliquid_spot_data_feed = std::make_shared<MarketDataFeed>(
            "hyperliquid_spot_feed",
            ioc, soc,
            "api.hyperliquid.xyz",
            "443",
            "/ws",
            hyperliquid_callback_spot(tick_queue)
        );

        hyperliquid_spot_data_feed->connect();

        binance_spot_data_feed->send(R"({
            "method": "SUBSCRIBE",
            "params": [
                "btcusdt@aggTrade"
            ],
            "id": 1
        })");

        binance_futures_data_feed->send(R"({
            "method": "SUBSCRIBE",
            "params": [
                "btcusdt@aggTrade",
                "btcusdt@markPrice"
            ],
            "id": 1
        })");

        hyperliquid_futures_data_feed->send(R"({
            "action": "subscribe",
            "channels": [
                "future.price:BTC-PERP"
            ]
        })");

        hyperliquid_spot_data_feed->send(R"({
            "action": "subscribe",
            "channels": [
                "spot.price:BTC-USD"
            ]
        })");


    }
};