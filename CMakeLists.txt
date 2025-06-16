cmake_minimum_required(VERSION 3.20)

project(feedMePlease VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

execute_process(
        COMMAND brew --prefix boost
        OUTPUT_VARIABLE BOOST_ROOT
        OUTPUT_STRIP_TRAILING_WHITESPACE
)
set(BOOST_INCLUDEDIR "${BOOST_ROOT}/include")
list(APPEND CMAKE_PREFIX_PATH "${BOOST_ROOT}")
find_package(Boost REQUIRED COMPONENTS system)

include(FetchContent)
FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.13.0
)
FetchContent_MakeAvailable(spdlog)


FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(json)


execute_process(
        COMMAND brew --prefix openssl@3
        OUTPUT_VARIABLE OPENSSL_ROOT
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

list(APPEND CMAKE_PREFIX_PATH "${OPENSSL_ROOT}")
find_package(OpenSSL REQUIRED)

add_executable(feedMePlease
        src/main.cpp
)

target_link_libraries(feedMePlease PRIVATE
        Boost::system
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        OpenSSL::SSL
        OpenSSL::Crypto
)