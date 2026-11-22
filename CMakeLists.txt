cmake_minimum_required(VERSION 3.16)
project(kova_native VERSION 4.2.0 DESCRIPTION "Kova Native C++20 Multi-Storage Engine" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ALIST_BUILD_TESTS "Build security, logic, and benchmark test executables" ON)
option(KOVA_BUILD_WEBUI "Regenerate the embedded Workspace App when Node.js is available" ON)

find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(SQLite3 REQUIRED)

set(KOVA_WEBUI_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/include/alist/web_ui.hpp")
if(KOVA_BUILD_WEBUI)
    find_program(KOVA_NODE_EXECUTABLE NAMES node nodejs)
    if(KOVA_NODE_EXECUTABLE)
        add_custom_command(
            OUTPUT "${KOVA_WEBUI_HEADER}"
            COMMAND "${KOVA_NODE_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/build_webui.mjs"
            DEPENDS
                "${CMAKE_CURRENT_SOURCE_DIR}/web/index.html"
                "${CMAKE_CURRENT_SOURCE_DIR}/web/styles.css"
                "${CMAKE_CURRENT_SOURCE_DIR}/web/app.js"
                "${CMAKE_CURRENT_SOURCE_DIR}/scripts/build_webui.mjs"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            COMMENT "Building the zero-CDN Kova Workspace App"
            VERBATIM
        )
        add_custom_target(kova_webui DEPENDS "${KOVA_WEBUI_HEADER}")
    else()
        message(STATUS "Node.js not found; using the checked-in embedded Web UI header")
    endif()
endif()

set(ALIST_CORE_SOURCES
    src/json.cpp
    src/xml.cpp
    src/crypto.cpp
    src/security.cpp
    src/config.cpp
    src/db.cpp
    src/http_client.cpp
    src/drivers_local.cpp
    src/drivers_s3_webdav.cpp
    src/drivers_cloud_sftp.cpp
    src/drivers_virtual_crypt.cpp
    src/vfs.cpp
    src/archive.cpp
    src/tasks.cpp
    src/http_server.cpp
    src/webdav_server.cpp
    src/api_routes.cpp
)

add_library(alist_core_lib STATIC ${ALIST_CORE_SOURCES})
target_include_directories(alist_core_lib PUBLIC include third_party/sys_shim)
target_compile_options(alist_core_lib PRIVATE -Wall -Wextra -Wno-unused-parameter)
target_link_libraries(alist_core_lib PUBLIC
    OpenSSL::SSL
    OpenSSL::Crypto
    CURL::libcurl
    ZLIB::ZLIB
    SQLite::SQLite3
    Threads::Threads
)
if(TARGET kova_webui)
    add_dependencies(alist_core_lib kova_webui)
endif()

# Keep the historical executable and CLI name for existing deployments and scripts.
add_executable(alist src/main.cpp)
target_link_libraries(alist PRIVATE alist_core_lib)

if(ALIST_BUILD_TESTS)
    enable_testing()
    add_executable(alist_tests tests/test_main.cpp)
    target_link_libraries(alist_tests PRIVATE alist_core_lib)
    add_test(NAME alist_security_and_logic_suite COMMAND alist_tests)

    add_executable(alist_bench tests/benchmark.cpp)
    target_link_libraries(alist_bench PRIVATE alist_core_lib)

    if(KOVA_NODE_EXECUTABLE)
        add_test(NAME kova_webui_integrity
            COMMAND "${KOVA_NODE_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/build_webui.mjs" --check)
    endif()
endif()
