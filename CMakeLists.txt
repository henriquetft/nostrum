# ============================================================================ #
# Copyright (c) 2026 Henrique Teófilo                                          #
# All rights reserved.                                                         #
#                                                                              # 
# Build configuration for Nostrum Relay                                        #
#                                                                              #
# This file is part of Nostrum project.                                        #
# For the terms of usage and distribution, please see COPYING file.            #
# ============================================================================ #

cmake_minimum_required(VERSION 3.16)
project(nostrum VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Tests
include(CTest)

option(ENABLE_ASAN "Enable address/leak sanitizer" ON)


execute_process(
    COMMAND git describe --tags --dirty --always
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

if(GIT_VERSION)
    set(APP_VERSION "${PROJECT_VERSION}+${GIT_VERSION}")
else()
    set(APP_VERSION "${PROJECT_VERSION}")
endif()

if(NOT GIT_COMMIT)
    set(GIT_COMMIT "unknown")
endif()

configure_file(
    ${CMAKE_SOURCE_DIR}/src/nostrum-version.h.in
    ${CMAKE_BINARY_DIR}/generated/nostrum-version.h
    @ONLY
)

# pkg-config
find_package(PkgConfig REQUIRED)

# Dependencies
pkg_check_modules(SQLITE       REQUIRED  IMPORTED_TARGET  sqlite3)
pkg_check_modules(GLIB         REQUIRED  IMPORTED_TARGET  glib-2.0)
pkg_check_modules(JSONGLIB     REQUIRED  IMPORTED_TARGET  json-glib-1.0)
pkg_check_modules(LIBSECP256K1 REQUIRED  IMPORTED_TARGET  libsecp256k1)
pkg_check_modules(GIO          REQUIRED  IMPORTED_TARGET  gio-2.0)
pkg_check_modules(LIBSOUP      REQUIRED  IMPORTED_TARGET  libsoup-3.0)

# Warnings
add_library(common_warnings INTERFACE)
target_compile_options(common_warnings INTERFACE
    $<$<C_COMPILER_ID:GNU,Clang>:-Wall -Wextra>
)

# Core
add_library(nostrum-core
    src/nostrum-event.c
    src/nostrum-msg-ok.c
    src/nostrum-filter.c
    src/nostrum-subscription.c
    src/nostrum-json-utils.c
    src/nostrum-utils.c
    src/nostrum-msg-req.c
    src/nostrum-msg-close.c
    src/nostrum-msg-eose.c
    src/nostrum-msg-closed.c
    src/nostrum-msg-notice.c
)

target_include_directories(nostrum-core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(nostrum-core
    PUBLIC common_warnings
    PUBLIC PkgConfig::GLIB
    PUBLIC PkgConfig::JSONGLIB
    PUBLIC PkgConfig::LIBSECP256K1
)

target_include_directories(nostrum-core PUBLIC ${CMAKE_BINARY_DIR}/generated)

# Storage (SQLite)
add_library(nostrum-storage-sqlite
    src/nostrum-storage-sqlite.c
)

target_link_libraries(nostrum-storage-sqlite
    PUBLIC nostrum-core
    PRIVATE PkgConfig::SQLITE
)

target_include_directories(nostrum-storage-sqlite
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Relay
add_library(nostrum-relay
    src/nostrum-relay.c
    src/nostrum-config.c
)

target_link_libraries(nostrum-relay
    PUBLIC nostrum-core
    PUBLIC PkgConfig::LIBSOUP
)

target_include_directories(nostrum-relay
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Nostrum relay executable
add_executable(nostrum
    src/nostrum.c
)

target_link_libraries(nostrum
    PRIVATE nostrum-relay
    PRIVATE nostrum-storage-sqlite
)

if(ENABLE_ASAN)
    target_compile_options(nostrum PRIVATE -fsanitize=leak)
    target_link_options(nostrum PRIVATE -fsanitize=leak)
endif()


# Tests
function(add_nostrum_test name)
    add_executable(${name} tests/${name}.c)
    target_link_libraries(${name} PRIVATE nostrum-core common_warnings)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_nostrum_test(event-test)
add_nostrum_test(filter-test)
add_nostrum_test(msg-req-test)
add_nostrum_test(msg-ok-test)
add_nostrum_test(msg-eose-test)
add_nostrum_test(msg-close-test)
add_nostrum_test(msg-closed-test)
add_nostrum_test(msg-notice-test)

# Storage test
add_executable(storage-test tests/storage-test.c)
target_link_libraries(storage-test
    PRIVATE nostrum-storage-sqlite
    PRIVATE common_warnings
)
add_test(NAME storage-test COMMAND storage-test)

# Server/relay test
add_executable(server-test tests/server-test.c)
target_link_libraries(server-test
    PRIVATE nostrum-relay
    PRIVATE nostrum-storage-sqlite
    PRIVATE common_warnings
)
add_test(NAME server-test COMMAND server-test)