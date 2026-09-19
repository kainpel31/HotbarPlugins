cmake_minimum_required(VERSION 3.20)
project(MMOHotbar VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Menggunakan CommonLibSSE (Template standar SKSE)
find_package(CommonLibSSE REQUIRED)

add_library(MMOHotbar SHARED
    src/main.cpp
)

target_link_libraries(MMOHotbar PRIVATE CommonLibSSE::CommonLibSSE)