//
// Created by Yeo Shu Heng on 23/6/25.
//
#include <array>

class TickDataBuffer {

public:
    std::array<std::unique_ptr<TickData>, 2> data_buffer{{nullptr, nullptr}};
    std::atomic<uint64_t> version{0};

    TickDataBuffer() = default;
};