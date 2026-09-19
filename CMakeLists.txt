cmake_minimum_required(VERSION 3.20)

project(MMOHotbar VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(ENABLE_TESTS OFF CACHE BOOL "Disable tests" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "Disable CommonLibSSE tests" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "Disable CTest" FORCE)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

if(NOT RAPIDCSV_INCLUDE_DIRS)
    find_path(RAPIDCSV_INCLUDE_DIRS rapidcsv.h
        PATHS
            "$ENV{VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/include"
            "$ENV{VCPKG_ROOT}/installed/x64-windows/include"
            "${CMAKE_SOURCE_DIR}/extern/rapidcsv/include"
        NO_DEFAULT_PATH
    )
endif()

if(NOT RAPIDCSV_INCLUDE_DIRS)
    message(FATAL_ERROR "rapidcsv headers were not found.")
endif()

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
    src/HotbarManager.h
    src/InputHandler.h
    src/UIMenu.h
)

target_link_libraries(MMOHotbar
    PRIVATE
        CommonLibSSE::CommonLibSSE
)
