cmake_minimum_required(VERSION 3.15)
project(MaestroCore LANGUAGES C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============================================================
# Build Options
# ============================================================
option(BUILD_MODULES "Build modules library" ON)
option(BUILD_UTILS   "Build utils library" ON)
option(BUILD_TESTS   "Build unit tests" OFF)

# ============================================================
# Dependencies: Python & Ruby (Behåll din befintliga logik)
# ============================================================
# ============================================================
# Python Venv & Dependencies Setup
# ============================================================
if(BUILD_MODULES OR BUILD_TESTS)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    
    set(VENV_DIR "${CMAKE_BINARY_DIR}/python_venv")
    
    # Skapa venv och installera paket om de saknas
    if(NOT EXISTS "${VENV_DIR}")
        message(STATUS "Creating Python venv and installing mbedTLS dependencies...")
        execute_process(COMMAND ${Python3_EXECUTABLE} -m venv ${VENV_DIR})
        execute_process(COMMAND ${VENV_DIR}/bin/pip install jsonschema jinja2)
    endif()

    # TVINGA CMake och MbedTLS att använda venv:ns python
    set(Python3_EXECUTABLE "${VENV_DIR}/bin/python3" CACHE FILEPATH "" FORCE)
    set(PYTHON_EXECUTABLE "${VENV_DIR}/bin/python3" CACHE FILEPATH "" FORCE)
endif()

# ============================================================
# External: mbed TLS & cJSON Paths
# ============================================================
set(MBEDTLS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/mbedtls")
set(CJSON_DIR   "${CMAKE_CURRENT_SOURCE_DIR}/external/cjson")

if(BUILD_MODULES OR BUILD_UTILS)
    # Konfigurera mbedTLS
    set(MBEDTLS_USER_CONFIG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/modules/include/maestromodules/tls_config.h" CACHE INTERNAL "")
    set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
    set(INSTALL_MBEDTLS_HEADERS OFF CACHE BOOL "" FORCE)
    
    add_subdirectory(${MBEDTLS_DIR} external/mbedtls EXCLUDE_FROM_ALL)
endif()

# ============================================================
# Target: maestroutils
# ============================================================
if(BUILD_UTILS)
    file(GLOB_RECURSE UTL_SRCS CONFIGURE_DEPENDS "utils/src/*.c")
    add_library(maestroutils STATIC ${UTL_SRCS})
    
    target_include_directories(maestroutils PUBLIC 
        utils/include
        modules/include
        ${CJSON_DIR}
        ${MBEDTLS_DIR}/include
    )
    
    # Om mbedtls_utils.c anropar mbedtls-funktioner måste vi länka här också
    target_link_libraries(maestroutils PUBLIC mbedcrypto mbedtls mbedx509)
endif()

# ============================================================
# Target: maestromodules
# ============================================================
if(BUILD_MODULES)
    file(GLOB_RECURSE MOD_SRCS CONFIGURE_DEPENDS "modules/src/*.c")
    
    # Bygg modulerna och inkludera cJSON källkoden
    add_library(maestromodules STATIC ${MOD_SRCS} ${CJSON_DIR}/cJSON.c)
    
    target_include_directories(maestromodules PUBLIC 
        modules/include
        utils/include
        ${CJSON_DIR}
        ${MBEDTLS_DIR}/include
    )

    target_compile_definitions(maestromodules PUBLIC MAESTROUTILS_WITH_CJSON)

    # Modulerna behöver utils och mbedtls
    target_link_libraries(maestromodules PUBLIC 
        maestroutils 
        mbedtls 
        mbedx509 
        mbedcrypto 
        tfpsacrypto
    )
endif()

# ============================================================
# Target: maestrocore (Umbrella)
# ============================================================
add_custom_target(maestrocore ALL)
if(BUILD_UTILS)
    add_dependencies(maestrocore maestroutils)
endif()
if(BUILD_MODULES)
    add_dependencies(maestrocore maestromodules)
endif()

# ============================================================
# Target: test_suite
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
        COMMENT "Generating mocks..."
    )

    add_executable(test_suite
        test/test_http.c
        "${MOCK_OUT_DIR}/Mocktcp_client.c"
        test/cmock/vendor/unity/src/unity.c
        test/cmock/src/cmock.c
    )

    # ALLT som rör test_suite MÅSTE ligga här inne
    target_link_libraries(test_suite PRIVATE 
        maestromodules 
        m 
        pthread
    )
    
    target_include_directories(test_suite PRIVATE 
        test/cmock/vendor/unity/src 
        test/cmock/src 
        ${MOCK_OUT_DIR}
    )

    add_test(NAME MaestroTests COMMAND test_suite)
endif()
