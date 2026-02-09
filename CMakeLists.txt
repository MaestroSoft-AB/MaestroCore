cmake_minimum_required(VERSION 3.15)
project(MaestroCore LANGUAGES C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============================================================
# Build Options
# ============================================================
option(BUILD_MODULES "Build modules" ON)
option(BUILD_UTILS   "Build utils" ON)
option(BUILD_TESTS   "Build unit tests" OFF)

# ============================================================
# Python requirement (modules + tests)
# ============================================================
if(BUILD_MODULES OR BUILD_TESTS)

    find_package(Python3 COMPONENTS Interpreter)

    if(NOT Python3_FOUND)
        message(FATAL_ERROR
"------------------------------------------------------------------
PYTHON IS REQUIRED

Install:

  Ubuntu / Debian / WSL:
    sudo apt install python3 python3-venv python3-pip

  Arch:
    sudo pacman -S python python-pip

After installation verify:
    python3 --version
------------------------------------------------------------------")
    endif()

    message(STATUS "Python found: ${Python3_EXECUTABLE}")

endif()

# ============================================================
# Ruby requirement (tests only)
# ============================================================
if(BUILD_TESTS)

    find_program(RUBY_EXECUTABLE ruby)

    if(NOT RUBY_EXECUTABLE)
        message(FATAL_ERROR
"------------------------------------------------------------------
RUBY IS REQUIRED FOR TESTS

Install:

  Ubuntu / Debian / WSL:
    sudo apt install ruby

  Arch:
    sudo pacman -S ruby

After installation verify:
    ruby --version
------------------------------------------------------------------")
    endif()

    message(STATUS "Ruby found: ${RUBY_EXECUTABLE}")

endif()

# ============================================================
# mbed TLS (only if modules)
# ============================================================
if(BUILD_MODULES)

    set(MBEDTLS_USER_CONFIG_FILE
        "${CMAKE_CURRENT_SOURCE_DIR}/modules/include/maestromodules/tls_config.h"
        CACHE INTERNAL ""
    )

    set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
    set(INSTALL_MBEDTLS_HEADERS OFF CACHE BOOL "" FORCE)

    add_subdirectory(external/mbedtls EXCLUDE_FROM_ALL)

endif()

# ============================================================
# Utils library (independent)
# ============================================================
if(BUILD_UTILS)

    file(GLOB_RECURSE UTL_SRCS CONFIGURE_DEPENDS "utils/src/*.c")

    add_library(maestroutils STATIC ${UTL_SRCS})

    target_include_directories(maestroutils PUBLIC
        utils/include
    )

endif()

# ============================================================
# Modules library
# ============================================================
if(BUILD_MODULES)

    file(GLOB_RECURSE MOD_SRCS CONFIGURE_DEPENDS "modules/src/*.c")

    add_library(maestromodules STATIC ${MOD_SRCS} external/cjson/cJSON.c)

    target_include_directories(maestromodules PUBLIC
        modules/include
        external/cjson
    )

    target_compile_definitions(maestromodules PUBLIC MAESTROUTILS_WITH_CJSON)

    target_link_libraries(maestromodules PUBLIC
        mbedtls
        mbedx509
        mbedcrypto
        tfpsacrypto
    )

endif()

# ============================================================
# Tests
# ============================================================
if(BUILD_TESTS)

    enable_testing()

    set(MOCK_OUT_DIR "${CMAKE_BINARY_DIR}/test/mocks")
    file(MAKE_DIRECTORY ${MOCK_OUT_DIR})

    add_custom_command(
        OUTPUT "${MOCK_OUT_DIR}/Mocktcp_client.c"
        COMMAND ${RUBY_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/test/cmock/lib/cmock.rb"
            -o"${CMAKE_CURRENT_SOURCE_DIR}/test/cmock_config.yaml"
            "${CMAKE_CURRENT_SOURCE_DIR}/modules/include/maestromodules/tcp_client.h"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/modules/include/maestromodules/tcp_client.h"
    )

    add_executable(test_suite
        test/test_http.c
        "${MOCK_OUT_DIR}/Mocktcp_client.c"
        test/cmock/vendor/unity/src/unity.c
        test/cmock/src/cmock.c
    )

    target_link_libraries(test_suite PRIVATE maestromodules)

    add_test(NAME MaestroTests COMMAND test_suite)

endif()
