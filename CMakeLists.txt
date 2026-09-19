cmake_minimum_required(VERSION 3.20)

project(MMOHotbar VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Wajib untuk kebijakan FetchContent
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

# Paksa matikan seluruh fitur test di CommonLibSSE-NG agar tidak mencari rapidcsv
set(ENABLE_TESTS OFF CACHE BOOL "Disable tests" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "Disable testing" FORCE)

include(FetchContent)

FetchContent_Declare(
    CommonLibSSE
    GIT_REPOSITORY https://github.com/CharmedBaryon/CommonLibSSE-NG.git
    GIT_TAG main
    GIT_SHALLOW TRUE
    GIT_SUBMODULES_RECURSE TRUE
)

FetchContent_MakeAvailable(CommonLibSSE)

add_library(MMOHotbar SHARED
    src/main.cpp
)

target_link_libraries(MMOHotbar
    PRIVATE
        CommonLibSSE::CommonLibSSE
)
