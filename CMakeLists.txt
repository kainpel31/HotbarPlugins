cmake_minimum_required(VERSION 3.20)
project(MMOHotbar VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Mengatur kebijakan CMake untuk ekstraksi ZIP
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()

# Menggunakan cabang 'main' yang valid untuk CommonLibSSE-NG
include(FetchContent)
FetchContent_Declare(
    CommonLibSSE
    URL https://github.com/CommonLibSSE-NG/CommonLibSSE-NG/archive/refs/heads/main.zip
)
FetchContent_MakeAvailable(CommonLibSSE)

add_library(MMOHotbar SHARED
    src/main.cpp
)

target_link_libraries(MMOHotbar PRIVATE CommonLibSSE::CommonLibSSE)
