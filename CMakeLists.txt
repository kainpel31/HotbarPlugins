cmake_minimum_required(VERSION 3.21)

# ---- Cache build vars ----
option(COPY_BUILD "Copy the build output to the Skyrim directory." TRUE)
macro(set_from_environment VARIABLE)
	if (NOT DEFINED ${VARIABLE} AND DEFINED ENV{${VARIABLE}})
		set(${VARIABLE} $ENV{${VARIABLE}})
	endif ()
endmacro()

set_from_environment(STB_WIDGETS_DEPLOY_DIR)
if (NOT DEFINED STB_WIDGETS_DEPLOY_DIR)
	set(
		STB_WIDGETS_DEPLOY_DIR
		"C:/Skyrim/[STB] Mod Organizer/mods/MMO Hotbar/skse/plugins"
		CACHE PATH
		"Directory to copy the built plugin and PDB into after a successful build."
	)
else ()
	set(
		STB_WIDGETS_DEPLOY_DIR
		"${STB_WIDGETS_DEPLOY_DIR}"
		CACHE PATH
		"Directory to copy the built plugin and PDB into after a successful build."
	)
endif ()

# ---- Vcpkg check ----
set_from_environment(VCPKG_ROOT)
if (NOT DEFINED VCPKG_ROOT)
	message(FATAL_ERROR "Variable VCPKG_ROOT is not set.")
endif ()

# ---- Include guards ----
if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
	message(FATAL_ERROR "In-source builds not allowed. Please make a build directory and run CMake from there.")
endif()

set(Boost_USE_STATIC_RUNTIME OFF CACHE BOOL "")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "")
set(MMO_HOTBAR_VERSION "1.0.0" CACHE STRING "Project version override")

# ---- Project ----
project(
	MMOHotbar
	VERSION ${MMO_HOTBAR_VERSION}
	LANGUAGES CXX
)

# ---- Globals ----
if (MSVC)
	add_compile_definitions(_UNICODE)
	if (NOT ${CMAKE_GENERATOR} STREQUAL "Ninja")
		add_compile_options(/MP)
	endif ()
endif ()

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_DEBUG OFF)
set(Boost_USE_STATIC_LIBS ON)

# ---- Dependencies ----
find_package(CommonLibSSE CONFIG REQUIRED)
find_package(spdlog REQUIRED CONFIG)
find_package(nlohmann_json REQUIRED CONFIG)
find_package(xbyak REQUIRED CONFIG)

# ---- Add source files ----
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

source_group(
	TREE
		${CMAKE_CURRENT_SOURCE_DIR}
	FILES
		${SOURCES}
)

# ---- Create DLL ----
add_library(
	${PROJECT_NAME}
	SHARED
	${SOURCES}
)

target_compile_features(
	${PROJECT_NAME}
	PRIVATE
		cxx_std_23
)

target_include_directories(
	${PROJECT_NAME}
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(
	${PROJECT_NAME}
	PRIVATE
		CommonLibSSE::CommonLibSSE
		nlohmann_json::nlohmann_json
		spdlog::spdlog
		xbyak::xbyak
)

if (MSVC)
	target_compile_options(
		${PROJECT_NAME}
		PRIVATE
			/sdl
			/utf-8
			/Zi
			/permissive-
			/Zc:alignedNew
			/Zc:auto
			/Zc:char8_t
			/Zc:__cplusplus
			/Zc:externC
			/Zc:externConstexpr
			/Zc:forScope
			/Zc:hiddenFriend
			/Zc:implicitNoexcept
			/Zc:lambda
			/Zc:noexceptTypes
			/Zc:preprocessor
			/Zc:referenceBinding
			/Zc:rvalueCast
			/Zc:sizedDealloc
			/Zc:strictStrings
			/Zc:ternary
			/Zc:threadSafeInit
			/Zc:tlsGuards
			/Zc:trigraphs
			/Zc:wchar_t
			/external:anglebrackets
			/external:W0
			/W4
			/WX
			"$<$<CONFIG:RELEASE>:/Zc:inline;/JMC-;/Ob3>"
	)

	target_link_options(
		${PROJECT_NAME}
		PRIVATE
			/WX
			"$<$<CONFIG:DEBUG>:/INCREMENTAL;/OPT:NOREF;/OPT:NOICF>"
			"$<$<CONFIG:RELEASE>:/INCREMENTAL:NO;/OPT:REF;/OPT:ICF;/DEBUG:FULL>"
	)
endif ()

# ---- Post Build Deployment ----
if (COPY_BUILD)
	message(STATUS "Post-build copy destination: ${STB_WIDGETS_DEPLOY_DIR}")
	add_custom_command(
		TARGET ${PROJECT_NAME} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:${PROJECT_NAME}> "${STB_WIDGETS_DEPLOY_DIR}"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_PDB_FILE:${PROJECT_NAME}> "${STB_WIDGETS_DEPLOY_DIR}"
	)
endif ()
