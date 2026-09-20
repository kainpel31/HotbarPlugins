cmake_minimum_required(VERSION 3.21)

option(COPY_BUILD "Copy the build output to the Skyrim directory." TRUE)

macro(set_from_environment VARIABLE)
    if (NOT DEFINED ${VARIABLE} AND DEFINED ENV{${VARIABLE}})
        set(${VARIABLE} $ENV{${VARIABLE}})
    endif ()
endmacro()

set_from_environment(VCPKG_ROOT)
if (NOT DEFINED VCPKG_ROOT)
    message(FATAL_ERROR "Variable VCPKG_ROOT is not set.")
endif ()

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message(FATAL_ERROR "In-source builds not allowed.")
endif()

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "")
set(MMO_HOTBAR_VERSION "1.0.0" CACHE STRING "Project version override")

# --- Identitas Mod ---
set(AUTHOR_NAME "Ortriniaz")
set(PRODUCT_NAME "MMOHotbar")
set(BEAUTIFUL_NAME "MMO Hotbar")

project(${PRODUCT_NAME} VERSION ${MMO_HOTBAR_VERSION} LANGUAGES CXX)

if (MSVC)
    add_compile_definitions(_UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    add_compile_options(/MP)
endif ()

include(FetchContent)

set(BUILD_TESTS OFF CACHE BOOL "Disable CommonLibSSE-NG tests" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "Disable project tests" FORCE)
set(ENABLE_TESTS OFF CACHE BOOL "Disable project tests" FORCE)

FetchContent_Declare(
    CommonLibSSE
    GIT_REPOSITORY https://github.com/CharmedBaryon/CommonLibSSE-NG.git
    GIT_TAG main
    GIT_SHALLOW TRUE
    GIT_SUBMODULES_RECURSE TRUE
)
FetchContent_MakeAvailable(CommonLibSSE)

FetchContent_Declare(
    SKSEMenuFrameworkSDK
    GIT_REPOSITORY https://github.com/QTR-Modding/SKSE-Menu-Framework-3.git
    GIT_TAG master
    GIT_SHALLOW TRUE
)
FetchContent_Populate(SKSEMenuFrameworkSDK)

find_package(spdlog REQUIRED CONFIG)
find_package(nlohmann_json REQUIRED CONFIG)
find_package(xbyak REQUIRED CONFIG)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

add_library(${PROJECT_NAME} SHARED ${SOURCES})

target_sources(${PROJECT_NAME} PRIVATE
    "${sksemenuframeworksdk_SOURCE_DIR}/imgui/imgui.cpp"
    "${sksemenuframeworksdk_SOURCE_DIR}/imgui/imgui_draw.cpp"
    "${sksemenuframeworksdk_SOURCE_DIR}/imgui/imgui_tables.cpp"
    "${sksemenuframeworksdk_SOURCE_DIR}/imgui/imgui_widgets.cpp"
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${sksemenuframeworksdk_SOURCE_DIR}/imgui"
)
target_link_libraries(${PROJECT_NAME} PRIVATE
    CommonLibSSE::CommonLibSSE
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    xbyak::xbyak
)

if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /sdl /utf-8 /Zi /permissive- /W4 /WX /external:anglebrackets /external:W0)
    target_link_options(${PROJECT_NAME} PRIVATE /WX "$<$<CONFIG:RELEASE>:/INCREMENTAL:NO;/OPT:REF;/OPT:ICF;/DEBUG:FULL>")
endif ()

if (COPY_BUILD)
    set_from_environment(STB_WIDGETS_DEPLOY_DIR)
    if (NOT DEFINED STB_WIDGETS_DEPLOY_DIR)
        set(STB_WIDGETS_DEPLOY_DIR "C:/Skyrim/[STB] Mod Organizer/mods/MMO Hotbar/skse/plugins" CACHE PATH "Plugin deployment directory")
    endif ()
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:${PROJECT_NAME}> "${STB_WIDGETS_DEPLOY_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_PDB_FILE:${PROJECT_NAME}> "${STB_WIDGETS_DEPLOY_DIR}"
    )
endif ()
