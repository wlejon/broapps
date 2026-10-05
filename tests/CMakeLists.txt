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
endif()
