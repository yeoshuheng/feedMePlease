//
// Created by Yeo Shu Heng on 17/6/25.
//
enum InstrumentType {
    SPOT, PERP
};

const char* InstrumentTypeToString(const InstrumentType type) {
    switch(type) {
        case SPOT: return "SPOT";
        case PERP: return "PERP";
        default: return "UNKNOWN";
    }
}