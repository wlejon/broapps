include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

add_executable(broapps_test_helper test_helper.cpp)
broapps_warnings(broapps_test_helper)

# Portable tests
add_executable(test_event_queue test_event_queue.cpp)
target_link_libraries(test_event_queue PRIVATE broapps)
broapps_warnings(test_event_queue)
add_test(NAME test_event_queue COMMAND test_event_queue)

add_executable(test_search test_search.cpp)
target_link_libraries(test_search PRIVATE broapps)
broapps_warnings(test_search)
add_test(NAME test_search COMMAND test_search)

add_executable(test_mime_service test_mime_service.cpp)
target_link_libraries(test_mime_service PRIVATE broapps)
broapps_warnings(test_mime_service)
add_test(NAME test_mime_service COMMAND test_mime_service)

add_executable(test_catalog_watcher test_catalog_watcher.cpp)
target_link_libraries(test_catalog_watcher PRIVATE broapps)
broapps_warnings(test_catalog_watcher)
if(WIN32)
    target_link_libraries(test_catalog_watcher PRIVATE ole32 uuid)
endif()
add_test(NAME test_catalog_watcher COMMAND test_catalog_watcher)
set_tests_properties(test_catalog_watcher PROPERTIES TIMEOUT 120)

if(WIN32)
    add_executable(test_win_lnk win/test_win_lnk.cpp)
    target_link_libraries(test_win_lnk PRIVATE broapps)
    broapps_warnings(test_win_lnk)
    add_test(NAME test_win_lnk COMMAND test_win_lnk)

    add_executable(test_win_catalog win/test_win_catalog.cpp)
    target_link_libraries(test_win_catalog PRIVATE broapps)
    broapps_warnings(test_win_catalog)
    add_test(NAME test_win_catalog COMMAND test_win_catalog)

    add_executable(test_win_launch win/test_win_launch.cpp)
    target_link_libraries(test_win_launch PRIVATE broapps)
    broapps_warnings(test_win_launch)
    add_test(NAME test_win_launch COMMAND test_win_launch $<TARGET_FILE:broapps_test_helper>)

    add_executable(test_win_mime win/test_win_mime.cpp)
    target_link_libraries(test_win_mime PRIVATE broapps)
    broapps_warnings(test_win_mime)
    add_test(NAME test_win_mime COMMAND test_win_mime)

    add_executable(test_win_recent win/test_win_recent.cpp)
    target_link_libraries(test_win_recent PRIVATE broapps)
    broapps_warnings(test_win_recent)
    add_test(NAME test_win_recent COMMAND test_win_recent)

elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(test_desktop_parse linux/test_desktop_parse.cpp)
    target_link_libraries(test_desktop_parse PRIVATE broapps)
    broapps_warnings(test_desktop_parse)
    add_test(NAME test_desktop_parse COMMAND test_desktop_parse)

    add_executable(test_linux_catalog linux/test_linux_catalog.cpp)
    target_link_libraries(test_linux_catalog PRIVATE broapps)
    broapps_warnings(test_linux_catalog)
    add_test(NAME test_linux_catalog COMMAND test_linux_catalog)

    add_executable(test_linux_launch linux/test_linux_launch.cpp)
    target_link_libraries(test_linux_launch PRIVATE broapps)
    broapps_warnings(test_linux_launch)
    add_test(NAME test_linux_launch COMMAND test_linux_launch $<TARGET_FILE:broapps_test_helper>)

    add_executable(test_xbel test_xbel.cpp)
    target_link_libraries(test_xbel PRIVATE broapps)
    broapps_warnings(test_xbel)
    add_test(NAME test_xbel COMMAND test_xbel)

    add_executable(test_linux_mime linux/test_linux_mime.cpp)
    target_link_libraries(test_linux_mime PRIVATE broapps)
    broapps_warnings(test_linux_mime)
    add_test(NAME test_linux_mime COMMAND test_linux_mime)

    add_executable(test_linux_recent linux/test_linux_recent.cpp)
    target_link_libraries(test_linux_recent PRIVATE broapps)
    broapps_warnings(test_linux_recent)
    add_test(NAME test_linux_recent COMMAND test_linux_recent)

    add_executable(test_linux_icon linux/test_linux_icon.cpp)
    target_link_libraries(test_linux_icon PRIVATE broapps)
    broapps_warnings(test_linux_icon)
    add_test(NAME test_linux_icon COMMAND test_linux_icon)

elseif(APPLE)
    add_executable(test_mac_bundle mac/test_mac_bundle.cpp)
    target_link_libraries(test_mac_bundle PRIVATE broapps)
    broapps_warnings(test_mac_bundle)
    add_test(NAME test_mac_bundle COMMAND test_mac_bundle)

    add_executable(test_mac_catalog mac/test_mac_catalog.cpp)
    target_link_libraries(test_mac_catalog PRIVATE broapps)
    broapps_warnings(test_mac_catalog)
    add_test(NAME test_mac_catalog COMMAND test_mac_catalog)

    add_executable(test_mac_launch mac/test_mac_launch.cpp)
    target_link_libraries(test_mac_launch PRIVATE broapps)
    broapps_warnings(test_mac_launch)
    add_test(NAME test_mac_launch COMMAND test_mac_launch $<TARGET_FILE:broapps_test_helper>)

    add_executable(test_mac_mime mac/test_mac_mime.cpp)
    target_link_libraries(test_mac_mime PRIVATE broapps)
    broapps_warnings(test_mac_mime)
    add_test(NAME test_mac_mime COMMAND test_mac_mime)

    add_executable(test_mac_recent mac/test_mac_recent.cpp)
    target_link_libraries(test_mac_recent PRIVATE broapps)
    broapps_warnings(test_mac_recent)
    add_test(NAME test_mac_recent COMMAND test_mac_recent)

    add_executable(test_mac_icon mac/test_mac_icon.cpp)
    target_link_libraries(test_mac_icon PRIVATE broapps)
    broapps_warnings(test_mac_icon)
    add_test(NAME test_mac_icon COMMAND test_mac_icon)
endif()

# The api test scripts the freedesktop catalog: a .desktop file in a scratch
# search path, XDG_CONFIG_HOME for mimeapps.list, /bin/echo as the launched
# program. It is Linux-only for that reason; broapps_api itself still builds
# everywhere, so a Windows or macOS build still proves it compiles.
if(BROAPPS_ENABLE_API AND NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(STATUS "broapps: broapps_test_api is Linux-only (it drives the freedesktop catalog)")
elseif(BROAPPS_ENABLE_API)
    add_executable(broapps_test_api
        test_apps_api.cpp
    )
    target_include_directories(broapps_test_api PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/api
        ${PROJECT_SOURCE_DIR}/src
    )
    target_link_libraries(broapps_test_api PRIVATE
        broapps_api
        broapps
        bronze_runtime_shared
    )
    broapps_warnings(broapps_test_api)
    if(WIN32)
        target_compile_definitions(broapps_test_api PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE _WIN32_WINNT=0x0A00)
        add_custom_command(TARGET broapps_test_api POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:bronze_runtime_shared>
                $<TARGET_FILE_DIR:broapps_test_api>
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_LINKER_FILE:bronze_runtime_shared>
                $<TARGET_FILE_DIR:broapps_test_api>
        )
    endif()

    if(NOT BRASS_HOST_BACKEND)
        message(STATUS "broapps: broapps_test_api not registered (brass has no code generator for this target)")
    else()
        add_test(NAME broapps_test_api COMMAND broapps_test_api)
        set_tests_properties(broapps_test_api PROPERTIES TIMEOUT 60 SKIP_RETURN_CODE 77)

        add_test(NAME broapps_test_api_gc_stress COMMAND broapps_test_api)
        set_tests_properties(broapps_test_api_gc_stress PROPERTIES
            TIMEOUT 60
            SKIP_RETURN_CODE 77
            ENVIRONMENT "BRONZE_GC_STRESS=1;BRONZE_GC_POISON=1"
        )
        if(WIN32)
            set_tests_properties(broapps_test_api PROPERTIES
                ENVIRONMENT "PATH=$<TARGET_FILE_DIR:bronze_runtime_shared>;$<TARGET_FILE_DIR:broapps_test_api>;$ENV{PATH}")
            set_property(TEST broapps_test_api_gc_stress APPEND PROPERTY
                ENVIRONMENT "PATH=$<TARGET_FILE_DIR:bronze_runtime_shared>;$<TARGET_FILE_DIR:broapps_test_api>;$ENV{PATH}")
        endif()
    endif()
endif()

# TEST_SKIP (test_common.h) exits 77 when a tool or platform feature is absent: ctest reports
# that as a skip, not a failure.
get_property(_broapps_tests DIRECTORY PROPERTY TESTS)
set_tests_properties(${_broapps_tests} PROPERTIES SKIP_RETURN_CODE 77)

# Every test executable links no_dialogs.cpp (see the file).
if(WIN32)
    get_property(_test_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
    foreach(_t IN LISTS _test_targets)
        get_target_property(_type ${_t} TYPE)
        if(_type STREQUAL "EXECUTABLE")
            target_sources(${_t} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/no_dialogs.cpp)
        endif()
    endforeach()
endif()
