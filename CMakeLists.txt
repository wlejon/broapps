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
# File types (MIME database) and directory watching. An existing `brovfs` target (a superbuild
# already added it) wins; otherwise the repo beside this one (../brovfs), overridable with
# -DBROVFS_DIR=<path>.
if(NOT TARGET brovfs)
    get_filename_component(_brovfs_default "${CMAKE_CURRENT_SOURCE_DIR}/../brovfs" ABSOLUTE)
    set(BROVFS_DIR "${_brovfs_default}" CACHE PATH "Path to the brovfs repo")
    if(NOT EXISTS "${BROVFS_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "broapps needs brovfs: clone it beside broapps (../brovfs) or pass -DBROVFS_DIR=<path>")
    endif()
    message(STATUS "broapps: brovfs from ${BROVFS_DIR}")
    set(BROVFS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    add_subdirectory("${BROVFS_DIR}" "${CMAKE_BINARY_DIR}/brovfs" EXCLUDE_FROM_ALL)
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

if(BROAPPS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
