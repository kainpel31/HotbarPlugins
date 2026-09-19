cmake_minimum_required(VERSION 3.20)
project(MMOHotbar VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Mengunduh CommonLibSSE-NG langsung via arsip ZIP (Aman untuk GitHub Web/Actions)
include(FetchContent)
FetchContent_Declare(
    CommonLibSSE
    URL https://github.com/CommonLibSSE-NG/CommonLibSSE-NG/archive/refs/heads/master.zip
)
FetchContent_MakeAvailable(CommonLibSSE)

add_library(MMOHotbar SHARED
    src/main.cpp
)

target_link_libraries(MMOHotbar PRIVATE CommonLibSSE::CommonLibSSE)
