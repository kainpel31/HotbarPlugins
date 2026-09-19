cmake_minimum_required(VERSION 3.20)
project(MMOHotbar VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Menggunakan FetchContent dengan opsi shallow clone untuk menghindari prompt kredensial
include(FetchContent)
FetchContent_Declare(
    CommonLibSSE
    GIT_REPOSITORY https://github.com/CommonLibSSE-NG/CommonLibSSE-NG.git
    GIT_TAG master
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(CommonLibSSE)

add_library(MMOHotbar SHARED
    src/main.cpp
)

target_link_libraries(MMOHotbar PRIVATE CommonLibSSE::CommonLibSSE)
