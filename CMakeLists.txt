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
# Python Venv & Dependencies Setup
# ============================================================
if(BUILD_MODULES OR BUILD_TESTS)

    # Find any system python first (needed to create venv)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)

    set(VENV_DIR "${CMAKE_BINARY_DIR}/python_venv")
    set(VENV_PYTHON "${VENV_DIR}/bin/python")

    # --------------------------------------------------------
    # Create venv if missing
    # --------------------------------------------------------
    if(NOT EXISTS "${VENV_PYTHON}")
        message(STATUS "Creating Python virtual environment...")
        execute_process(
            COMMAND ${Python3_EXECUTABLE} -m venv ${VENV_DIR}
            RESULT_VARIABLE venv_result
        )
        if(NOT venv_result EQUAL 0)
            message(FATAL_ERROR "Failed to create Python virtual environment")
        endif()
    endif()

    # --------------------------------------------------------
    # Ensure pip exists (portable solution, no apt needed)
    # --------------------------------------------------------
    execute_process(
        COMMAND ${VENV_PYTHON} -m pip --version
        RESULT_VARIABLE PIP_MISSING
        OUTPUT_QUIET ERROR_QUIET
    )

    if(PIP_MISSING)
        message(STATUS "pip missing -> bootstrapping via ensurepip")
        execute_process(
            COMMAND ${VENV_PYTHON} -m ensurepip --upgrade
            RESULT_VARIABLE ensurepip_result
        )
        if(NOT ensurepip_result EQUAL 0)
            message(FATAL_ERROR "Failed to bootstrap pip using ensurepip")
        endif()
    endif()

    # --------------------------------------------------------
    # Install dependencies (safe if already installed)
    # --------------------------------------------------------
    message(STATUS "Installing Python dependencies...")
    execute_process(COMMAND ${VENV_PYTHON} -m pip install --upgrade pip)
    execute_process(COMMAND ${VENV_PYTHON} -m pip install jsonschema jinja2 pyyaml)

    # --------------------------------------------------------
    # Force entire project + subprojects to use venv python
    # --------------------------------------------------------
    set(Python3_EXECUTABLE ${VENV_PYTHON} CACHE FILEPATH "Python in venv" FORCE)
    set(PYTHON_EXECUTABLE  ${VENV_PYTHON} CACHE FILEPATH "Python in venv" FORCE)

    message(STATUS "Python set to: ${Python3_EXECUTABLE}")
endif()

# ============================================================
# External: mbed TLS & cJSON Paths
# ============================================================
set(MBEDTLS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/mbedtls")
set(CJSON_DIR   "${CMAKE_CURRENT_SOURCE_DIR}/external/cjson")

if(BUILD_MODULES)
    # Configure mbedTLS
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
    
    # Om vi INTE bygger moduler, plocka bort filen som kräver mbedTLS
    if(NOT BUILD_MODULES)
        list(FILTER UTL_SRCS EXCLUDE REGEX "mbedtls_utils.c")
    endif()

    add_library(maestroutils STATIC ${UTL_SRCS})
    
    target_include_directories(maestroutils PUBLIC 
        utils/include
        modules/include
        ${CJSON_DIR}
        # Inkludera bara mbedtls-mappen om vi faktiskt bygger modulerna
        $<$<BOOL:${BUILD_MODULES}>:${MBEDTLS_DIR}/include>
    )
    
    # Länka bara mot mbedTLS om vi faktiskt bygger modulerna
    if(BUILD_MODULES)
        target_link_libraries(maestroutils PUBLIC mbedcrypto mbedtls mbedx509)
    endif()
endif()

# ============================================================
# Target: maestromodules
# ============================================================
if(BUILD_MODULES)
    file(GLOB_RECURSE MOD_SRCS CONFIGURE_DEPENDS "modules/src/*.c")
    
    add_library(maestromodules STATIC ${MOD_SRCS} ${CJSON_DIR}/cJSON.c)
    
    target_include_directories(maestromodules PUBLIC 
        modules/include
        utils/include
        ${CJSON_DIR}
        ${MBEDTLS_DIR}/include
    )

    target_compile_definitions(maestromodules PUBLIC MAESTROUTILS_WITH_CJSON)

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


