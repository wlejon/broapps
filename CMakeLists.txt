cmake_minimum_required(VERSION 3.24)

project(broapps VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(_broapps_top_level OFF)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(_broapps_top_level ON)
endif()

option(BROAPPS_BUILD_TESTS "Build broapps tests" ${_broapps_top_level})

# Coverage. Scoped to a top-level build: a consumer that add_subdirectory()s
# broapps has no business having its own build instrumented from in here.
option(BROAPPS_COVERAGE "Instrument the build for gcov coverage (GCC/Clang)" OFF)
if(BROAPPS_COVERAGE AND _broapps_top_level)
    if(MSVC)
        message(FATAL_ERROR "BROAPPS_COVERAGE requires GCC or Clang (gcov instrumentation); MSVC is unsupported.")
    endif()
    add_compile_options(--coverage -O0 -g)
    add_link_options(--coverage)
    message(STATUS "broapps: coverage instrumentation ON (-O0)")
endif()

# The DLL CRT (/MD, /MDd), as bro uses. broapps_api links
# bronze_runtime_shared, a DLL whose embed boundary passes C++ objects
# (std::string, ...) both ways: with the static CRT the DLL and an executable
# each get their own heap, and a Debug build asserts
# (__acrt_first_block == header) the first time one frees the other's memory.
if(MSVC AND _broapps_top_level)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

function(broapps_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8 /MP)
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

# ---- brovfs --------------------------------------------------------------------
# File types (MIME database) and directory watching. Resolved through cmake/bro_deps.cmake:
# an existing `brovfs` target (a superbuild already added it) wins; otherwise the ../brovfs
# working tree beside the top-level project; otherwise the head of its main, fetched at configure.
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/bro_deps.cmake)
bro_dependency(brovfs GITHUB wlejon/brovfs OPTIONS BROVFS_BUILD_TESTS=OFF)

file(GLOB BROAPPS_SOURCES CONFIGURE_DEPENDS src/common/*.cpp)

if(WIN32)
    file(GLOB _win_sources CONFIGURE_DEPENDS src/win/*.cpp)
    list(APPEND BROAPPS_SOURCES ${_win_sources})
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(Threads REQUIRED)
    file(GLOB _linux_sources CONFIGURE_DEPENDS src/linux/*.cpp)
    list(APPEND BROAPPS_SOURCES ${_linux_sources})
elseif(APPLE)
    enable_language(OBJCXX)
    set(CMAKE_OBJCXX_STANDARD 20)
    set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
    find_package(Threads REQUIRED)
    file(GLOB _mac_sources CONFIGURE_DEPENDS src/mac/*.cpp src/mac/*.mm)
    list(APPEND BROAPPS_SOURCES ${_mac_sources})
else()
    message(FATAL_ERROR "broapps implements Windows, Linux and macOS backends only")
endif()

add_library(broapps STATIC ${BROAPPS_SOURCES})
add_library(broapps::broapps ALIAS broapps)

target_include_directories(broapps
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/common)

broapps_warnings(broapps)
target_link_libraries(broapps PUBLIC brovfs::brovfs)

if(WIN32)
    target_compile_definitions(broapps PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE _WIN32_WINNT=0x0A00)
    target_link_libraries(broapps PRIVATE user32 shell32 ole32 oleaut32 advapi32 propsys shlwapi)
elseif(APPLE)
    target_compile_options(broapps PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
    target_link_libraries(broapps PUBLIC Threads::Threads PRIVATE "-framework CoreFoundation" "-framework Foundation" "-framework CoreServices")
else()
    target_link_libraries(broapps PUBLIC Threads::Threads)
endif()

# --- Standalone Bronze JavaScript API ---
# On by default only when broapps is the top-level project. A library that
# consumes broapps then never needs bronze for it; bro forces
# BROAPPS_ENABLE_API ON itself when it wants the binding. (The same rule is
# what keeps the nested brovfs from demanding bronze here.)
option(BROAPPS_ENABLE_API "Build Bronze JavaScript API" ${PROJECT_IS_TOP_LEVEL})
if(BROAPPS_ENABLE_API)
    # bronze (with brass, its code generator) through cmake/bro_deps.cmake: an
    # outer project's target, else ../bronze beside the top-level project, else
    # the head of its main, fetched at configure.
    bro_dependency(bronze GITHUB wlejon/bronze TARGET bronze_runtime_shared
                   OPTIONS BRONZE_BUILD_SHARED_RUNTIME=ON BRONZE_BUILD_TESTS=OFF)

    add_subdirectory(src/api)
endif()

if(BROAPPS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
