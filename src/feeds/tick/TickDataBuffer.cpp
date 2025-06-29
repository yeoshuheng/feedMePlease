//
// Created by Yeo Shu Heng on 23/6/25.
//
#include <array>

class TickDataBuffer {

public:
    std::atomic<uint64_t> version{0};
    TickData data_buffer[2];

    TickDataBuffer() {
        data_buffer[0] = {};
        data_buffer[1] = {};
    }
};