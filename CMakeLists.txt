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

if(MSVC AND _broapps_top_level)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
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
# File types (MIME database) and directory watching. Resolved like every bro sibling: an
# existing `brovfs` target (a superbuild already added it) wins; otherwise a standalone
# checkout beside the top-level project (../brovfs, overridable with -DBROVFS_DIR=<path>);
# otherwise the top-level project's third_party/brovfs submodule
# (`git submodule update --init --recursive`).
if(NOT TARGET brovfs)
    set(BROVFS_DIR "${CMAKE_SOURCE_DIR}/../brovfs" CACHE PATH
        "Path to standalone brovfs repo (set to a missing path to force submodule fallback)")
    set(BROVFS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    if(EXISTS "${BROVFS_DIR}/CMakeLists.txt")
        message(STATUS "broapps: brovfs from ${BROVFS_DIR}")
        add_subdirectory("${BROVFS_DIR}" "${CMAKE_BINARY_DIR}/brovfs" EXCLUDE_FROM_ALL)
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/third_party/brovfs/CMakeLists.txt")
        message(STATUS "broapps: brovfs from ${CMAKE_SOURCE_DIR}/third_party/brovfs (submodule)")
        add_subdirectory("${CMAKE_SOURCE_DIR}/third_party/brovfs" "${CMAKE_BINARY_DIR}/brovfs" EXCLUDE_FROM_ALL)
    else()
        message(FATAL_ERROR
            "brovfs not found at ${BROVFS_DIR} or ${CMAKE_SOURCE_DIR}/third_party/brovfs. "
            "Clone https://github.com/wlejon/brovfs next to this repo, run "
            "`git submodule update --init --recursive`, or override with -DBROVFS_DIR=...")
    endif()
endif()

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
option(BROAPPS_ENABLE_API "Build Bronze JavaScript API" ON)
if(BROAPPS_ENABLE_API)
    if(NOT TARGET bronze_runtime_shared)
        set(BRONZE_BUILD_SHARED_RUNTIME ON CACHE BOOL "" FORCE)
        set(BRONZE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        if(DEFINED BRONZE_DIR AND EXISTS "${BRONZE_DIR}/CMakeLists.txt")
            # Explicit BRONZE_DIR supplied
        elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../bronze/CMakeLists.txt")
            set(BRONZE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../bronze" CACHE PATH "bronze source tree")
        elseif(EXISTS "${CMAKE_SOURCE_DIR}/../bronze/CMakeLists.txt")
            set(BRONZE_DIR "${CMAKE_SOURCE_DIR}/../bronze" CACHE PATH "bronze source tree")
        endif()

        if(NOT BRONZE_DIR OR NOT EXISTS "${BRONZE_DIR}/CMakeLists.txt")
            message(FATAL_ERROR
                "bronze not found: check it out beside this repository (../bronze, with ../brass beside it) "
                "or pass -DBRONZE_DIR=<path>.")
        endif()

        add_subdirectory("${BRONZE_DIR}" "${CMAKE_BINARY_DIR}/bronze-build" EXCLUDE_FROM_ALL)
    endif()

    add_subdirectory(src/api)
endif()

if(BROAPPS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
