//
// Created by Yeo Shu Heng on 17/6/25.
//
#include <string>
#include <algorithm>

std::string to_upper(const std::string& input) {
    std::string result = input;
    std::ranges::transform(result, result.begin(),
                           [](const unsigned char c) { return std::toupper(c); });
    return result;
}