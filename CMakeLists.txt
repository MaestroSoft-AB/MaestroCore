cmake_minimum_required(VERSION 3.10)
project(MaestroCore C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- 1. Python Venv ---
find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(VENV_DIR "${CMAKE_BINARY_DIR}/python_venv")
if(NOT EXISTS "${VENV_DIR}")
    execute_process(COMMAND ${Python3_EXECUTABLE} -m venv ${VENV_DIR})
    execute_process(COMMAND ${VENV_DIR}/bin/pip install jsonschema jinja2)
endif()
set(Python3_EXECUTABLE "${VENV_DIR}/bin/python3" CACHE FILEPATH "" FORCE)

# --- 2. Mbed TLS 4.0.0 ---
set(MBEDTLS_USER_CONFIG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/modules/include/maestromodules/tls_config.h" CACHE INTERNAL "")
set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
set(INSTALL_MBEDTLS_HEADERS OFF CACHE BOOL "" FORCE)

add_subdirectory(external/mbedtls EXCLUDE_FROM_ALL)

# --- 3. THE FORCE-FEED FIX ---
# We tell the linker exactly where the .a files are located in the build tree.
# This ensures that when it sees '-lmbedcrypto', it finds it in YOUR build dir first.
link_directories(
    "${CMAKE_BINARY_DIR}/external/mbedtls/library"
    "${CMAKE_BINARY_DIR}/external/mbedtls/tf-psa-crypto/core"
)

# --- 4. MaestroCore Library ---
file(GLOB_RECURSE MOD_SRCS "modules/src/*.c")
file(GLOB_RECURSE UTL_SRCS "utils/src/*.c")
list(APPEND UTL_SRCS "external/cjson/cJSON.c")

add_library(maestrocore STATIC ${MOD_SRCS} ${UTL_SRCS})

target_include_directories(maestrocore PUBLIC 
    "modules/include"
    "utils/include"
    "external/cjson"
    "external/mbedtls/include"
)

# Link using the targets
target_link_libraries(maestrocore PUBLIC mbedtls mbedx509 mbedcrypto tfpsacrypto)
target_compile_definitions(maestrocore PUBLIC MAESTROUTILS_WITH_CJSON)

# --- 5. Unit Testing ---
enable_testing()
set(MOCK_OUT_DIR "${CMAKE_BINARY_DIR}/test/mocks")
file(MAKE_DIRECTORY ${MOCK_OUT_DIR})

add_custom_command(
    OUTPUT "${MOCK_OUT_DIR}/Mocktcp_client.c"
    COMMAND ruby "${CMAKE_CURRENT_SOURCE_DIR}/test/cmock/lib/cmock.rb" -o"${CMAKE_CURRENT_SOURCE_DIR}/test/cmock_config.yaml" "${CMAKE_CURRENT_SOURCE_DIR}/modules/include/maestromodules/tcp_client.h"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/modules/include/maestromodules/tcp_client.h"
)

add_executable(test_suite 
    "test/test_http.c"
    "${MOCK_OUT_DIR}/Mocktcp_client.c"
    "test/cmock/vendor/unity/src/unity.c"
    "test/cmock/src/cmock.c"
)

target_include_directories(test_suite PRIVATE 
    "test/cmock/vendor/unity/src" 
    "test/cmock/src" 
    "${MOCK_OUT_DIR}"
    "modules/include/maestromodules"
)

# Link the test suite. 
# link_directories above will ensure 'mbedcrypto' is resolved to your build folder.
target_link_libraries(test_suite PRIVATE maestrocore mbedtls mbedx509 mbedcrypto tfpsacrypto m pthread)
