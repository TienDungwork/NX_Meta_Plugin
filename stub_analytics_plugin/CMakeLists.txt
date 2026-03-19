## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

# NOTE: This plugin directory is intended to be buildable standalone. It needs Metadata SDK
# (nx_sdk headers/sources + nx_kit) to be provided via `metadataSdkDir`.

cmake_minimum_required(VERSION 3.14)
project(stub_analytics_plugin)

set(metadataSdkDir "" CACHE PATH "Path to unpacked VMS Metadata SDK (server_plugin_sdk).")
if(metadataSdkDir STREQUAL "")
    message(FATAL_ERROR "Define metadataSdkDir cache variable to point to the unzipped SDK.")
endif()
if(NOT EXISTS ${metadataSdkDir}/src/nx/sdk OR NOT EXISTS ${metadataSdkDir}/nx_kit/src/nx/kit)
    message(FATAL_ERROR "metadataSdkDir does not look like a valid SDK root: ${metadataSdkDir}")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(UNIX)
    # In Linux, for the plugin .so library, set `rpath` to "$ORIGIN" and do not set `runpath`, thus
    # enabling the lookup of the dependencies in the plugin dir first.
    string(APPEND CMAKE_SHARED_LINKER_FLAGS " -Wl,--disable-new-dtags")
endif()
set(CMAKE_SKIP_BUILD_RPATH ON)
set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
set(CMAKE_INSTALL_RPATH "$ORIGIN")

if(WIN32)
    set(API_IMPORT_MACRO "__declspec(dllimport)")
    set(API_EXPORT_MACRO "__declspec(dllexport)")
else()
    set(API_IMPORT_MACRO "")
    set(API_EXPORT_MACRO "__attribute__((visibility(\"default\")))")
endif()

#--------------------------------------------------------------------------------------------------
# Define nx_kit lib, static.

set(nxKitLibraryType "STATIC" CACHE STRING "" FORCE)
set(nxKitWithTests "NO" CACHE STRING "" FORCE)
add_subdirectory(${metadataSdkDir}/nx_kit ${CMAKE_CURRENT_BINARY_DIR}/nx_kit)

#--------------------------------------------------------------------------------------------------
# Define nx_sdk lib, static, depends on nx_kit.

set(SDK_SRC_DIR ${metadataSdkDir}/src)
file(GLOB_RECURSE SDK_SRC CONFIGURE_DEPENDS ${SDK_SRC_DIR}/*)

add_library(nx_sdk STATIC ${SDK_SRC})
target_include_directories(nx_sdk PUBLIC ${SDK_SRC_DIR})
target_link_libraries(nx_sdk PRIVATE nx_kit)
target_compile_definitions(nx_sdk PRIVATE NX_PLUGIN_API=${API_EXPORT_MACRO}) #< for nxLibContext()

#--------------------------------------------------------------------------------------------------
# Define stub_analytics_plugin lib, dynamic, depends on nx_kit and nx_sdk.

set(STUB_ANALYTICS_PLUGIN_SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/src)
set(STUB_ANALYTICS_PLUGIN_SRC
    # Entry point.
    ${STUB_ANALYTICS_PLUGIN_SRC_DIR}/nx/vms_server_plugins/analytics/stub/main.cpp
    ${STUB_ANALYTICS_PLUGIN_SRC_DIR}/nx/vms_server_plugins/analytics/stub/utils.cpp

    # Common small servers/stores (HTTP polygon + MQTT subscriber).
    ${STUB_ANALYTICS_PLUGIN_SRC_DIR}/nx/vms_server_plugins/analytics/stub/common/http_server.cpp
    ${STUB_ANALYTICS_PLUGIN_SRC_DIR}/nx/vms_server_plugins/analytics/stub/common/polygon_http_api.cpp
    ${STUB_ANALYTICS_PLUGIN_SRC_DIR}/nx/vms_server_plugins/analytics/stub/common/roi_store.cpp
    ${STUB_ANALYTICS_PLUGIN_SRC_DIR}/nx/vms_server_plugins/analytics/stub/common/mqtt_subscriber.cpp
    ${STUB_ANALYTICS_PLUGIN_SRC_DIR}/nx/vms_server_plugins/analytics/stub/common/mqtt_config_store.cpp

    # ROI module.
    ${STUB_ANALYTICS_PLUGIN_SRC_DIR}/nx/vms_server_plugins/analytics/stub/roi/integration.cpp
    ${STUB_ANALYTICS_PLUGIN_SRC_DIR}/nx/vms_server_plugins/analytics/stub/roi/engine.cpp
    ${STUB_ANALYTICS_PLUGIN_SRC_DIR}/nx/vms_server_plugins/analytics/stub/roi/device_agent.cpp
    ${STUB_ANALYTICS_PLUGIN_SRC_DIR}/nx/vms_server_plugins/analytics/stub/roi/stub_analytics_plugin_roi_ini.cpp

    # Object detection module.
    ${STUB_ANALYTICS_PLUGIN_SRC_DIR}/nx/vms_server_plugins/analytics/stub/object_detection/integration.cpp
    ${STUB_ANALYTICS_PLUGIN_SRC_DIR}/nx/vms_server_plugins/analytics/stub/object_detection/engine.cpp
    ${STUB_ANALYTICS_PLUGIN_SRC_DIR}/nx/vms_server_plugins/analytics/stub/object_detection/device_agent.cpp
    ${STUB_ANALYTICS_PLUGIN_SRC_DIR}/nx/vms_server_plugins/analytics/stub/object_detection/object_attributes.cpp
    ${STUB_ANALYTICS_PLUGIN_SRC_DIR}/nx/vms_server_plugins/analytics/stub/object_detection/stub_analytics_plugin_object_detection_ini.cpp

    # MQTT settings helper module.
    ${STUB_ANALYTICS_PLUGIN_SRC_DIR}/nx/vms_server_plugins/analytics/stub/mqtt/integration.cpp
    ${STUB_ANALYTICS_PLUGIN_SRC_DIR}/nx/vms_server_plugins/analytics/stub/mqtt/engine.cpp
    ${STUB_ANALYTICS_PLUGIN_SRC_DIR}/nx/vms_server_plugins/analytics/stub/mqtt/device_agent.cpp
    ${STUB_ANALYTICS_PLUGIN_SRC_DIR}/nx/vms_server_plugins/analytics/stub/mqtt/stub_analytics_plugin_mqtt_ini.cpp

    # Provide constants that are referenced by shared stub utilities (avoid undefined symbols).
    ${STUB_ANALYTICS_PLUGIN_SRC_DIR}/nx/vms_server_plugins/analytics/stub/object_streamer/constants.cpp
)

add_library(stub_analytics_plugin SHARED ${STUB_ANALYTICS_PLUGIN_SRC})
target_include_directories(stub_analytics_plugin PRIVATE ${STUB_ANALYTICS_PLUGIN_SRC_DIR})
target_link_libraries(stub_analytics_plugin PRIVATE nx_kit nx_sdk)

target_compile_definitions(stub_analytics_plugin
    PRIVATE NX_PLUGIN_API=${API_EXPORT_MACRO}
)

if(NOT WIN32)
    target_link_libraries(stub_analytics_plugin PRIVATE pthread)
endif()

#
# This project variant intentionally includes only ROI + Object Detection modules.
