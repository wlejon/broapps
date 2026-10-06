add_library(broapps_api STATIC
    api.cpp
    host_class.cpp
    native_apps.cpp
    native_process.cpp
)

target_include_directories(broapps_api PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
)

target_link_libraries(broapps_api PUBLIC
    broapps
    bronze_runtime_shared
)

target_compile_features(broapps_api PUBLIC cxx_std_20)

broapps_warnings(broapps_api)
