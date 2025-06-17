//
// Created by Yeo Shu Heng on 17/6/25.
//

enum Venue {
    BINANCE, HYPERLIQUID
};

const char* VenueToString(const Venue venue) {
    switch(venue) {
        case BINANCE: return "BN";
        case HYPERLIQUID: return "HYP";
        default: return "UNKNOWN";
    }
}