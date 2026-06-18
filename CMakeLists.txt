cmake_minimum_required(VERSION 3.16)
project(ecids_core VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# --- Dependencies ---
find_package(PkgConfig QUIET)

# nlohmann_json
find_package(nlohmann_json 3.2.0 QUIET)
if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann_json not found via find_package, trying PkgConfig")
    pkg_check_modules(JSON IMPORTED_TARGET nlohmann_json)
endif()

# ZeroMQ
pkg_check_modules(ZMQ IMPORTED_TARGET libzmq)

# libcurl
pkg_check_modules(CURL IMPORTED_TARGET libcurl)

# OpenSSL
pkg_check_modules(OPENSSL IMPORTED_TARGET openssl)

# Google Test
find_package(GTest QUIET)

# --- Sources (exclude main.cpp from library) ---
file(GLOB_RECURSE CORE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
list(REMOVE_ITEM CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

# --- Static Library ---
add_library(ecids_core_lib STATIC ${CORE_SOURCES})
target_include_directories(ecids_core_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ecids_core_lib PUBLIC
    PkgConfig::ZMQ
    PkgConfig::CURL
    PkgConfig::OPENSSL
)

if(nlohmann_json_FOUND)
    target_link_libraries(ecids_core_lib PUBLIC nlohmann_json::nlohmann_json)
elseif(JSON_FOUND)
    target_link_libraries(ecids_core_lib PUBLIC PkgConfig::JSON)
endif()

# --- Main Executable ---
add_executable(ecids_core_node src/main.cpp)
target_link_libraries(ecids_core_node PRIVATE ecids_core_lib)

# --- Tests ---
enable_testing()
file(GLOB TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_*.cpp")
if(TEST_SOURCES)
    if(GTest_FOUND)
        add_executable(ecids_core_tests ${TEST_SOURCES})
        target_link_libraries(ecids_core_tests PRIVATE ecids_core_lib GTest::GTest GTest::Main)
        include(GoogleTest)
        gtest_discover_tests(ecids_core_tests)
    else()
        message(STATUS "Google Test not found — skipping test target")
    endif()
endif()

# --- Install ---
install(TARGETS ecids_core_node RUNTIME DESTINATION bin)
