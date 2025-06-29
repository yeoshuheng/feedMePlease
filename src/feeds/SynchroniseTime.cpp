//
// Created by Yeo Shu Heng on 29/6/25.
//
#include <curl/curl.h>
#include <chrono>
#include <string>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

using json = nlohmann::json;

size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

int64_t get_binance_server_time_ms() {
    CURL* curl;
    CURLcode res;
    std::string readBuffer;

    curl = curl_easy_init();
    if(!curl) {
        spdlog::error("Failed to init CURL");
        return -1;
    }

    curl_easy_setopt(curl, CURLOPT_URL, "https://api.binance.com/api/v3/time");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

    res = curl_easy_perform(curl);
    if(res != CURLE_OK) {
        spdlog::error("curl_easy_perform() failed: {}", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        return -1;
    }

    curl_easy_cleanup(curl);

    try {
        auto j = json::parse(readBuffer);
        if (j.contains("serverTime")) {
            return j["serverTime"].get<int64_t>();
        } else {
            spdlog::error("No serverTime field in response");
            return -1;
        }
    } catch (std::exception& e) {
        spdlog::error("JSON parse error: {}", e.what());
        return -1;
    }
}

int64_t get_system_time_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

int64_t compute_time_offset() {
    int64_t system_time = get_system_time_ms();
    int64_t binance_time = get_binance_server_time_ms();

    if (binance_time == -1) {
        spdlog::warn("Failed to get Binance server time. Using offset = 0");
        return 0;
    }

    int64_t offset = system_time - binance_time;
    spdlog::info("System time: {} ms, Binance time: {} ms, offset: {} ms", system_time, binance_time, offset);
    return offset;
}
