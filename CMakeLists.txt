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
# Debug tooling options
# ============================================================
option(VALGRIND_FRIENDLY "Build Debug with Valgrind-friendly flags (no omit-frame-pointer, less inlining, etc.)" OFF)

if(VALGRIND_FRIENDLY)
  # Only use in debug-mode
  if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(WARNING "VALGRIND_FRIENDLY is ON but CMAKE_BUILD_TYPE is not Debug. Consider -DCMAKE_BUILD_TYPE=Debug.")
  endif()

  add_compile_options(
    -g3
    -O0
    -fno-omit-frame-pointer
    -fno-inline
    -fno-optimize-sibling-calls
  )

  add_link_options(
    -Wl,--export-dynamic
  )

  add_compile_definitions(VALGRIND_FRIENDLY_BUILD=1)

  message(STATUS "Valgrind-friendly flags enabled")
endif()

# ============================================================
# Python Venv & Dependencies Setup
# ============================================================
if(BUILD_MODULES OR BUILD_TESTS)

  # Find a system Python first (needed to create venv)
  find_package(Python3 COMPONENTS Interpreter QUIET)
  if(NOT Python3_FOUND)
    message(FATAL_ERROR
      "Python 3 was not found but is required to configure this project "
      "(BUILD_MODULES or BUILD_TESTS is ON).\n\n"
      "Install Python 3:\n"
      "  Ubuntu/Debian: sudo apt install python3 python3-venv python3-pip\n"
      "  Fedora/RHEL:   sudo dnf install python3 python3-pip\n"
      "  Arch:          sudo pacman -S python python-pip\n"
      "  macOS (brew):  brew install python\n"
    )
  endif()

  set(VENV_DIR "${CMAKE_BINARY_DIR}/python_venv")
  set(VENV_PYTHON "${VENV_DIR}/bin/python")

  # ----------------------------------------------------------
  # Create venv if missing
  # ----------------------------------------------------------
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

  # ----------------------------------------------------------
  # Ensure pip exists (portable solution, no apt needed)
  # ----------------------------------------------------------
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

  # ----------------------------------------------------------
  # Install dependencies (safe if already installed)
  # ----------------------------------------------------------
  message(STATUS "Installing Python dependencies...")
  execute_process(COMMAND ${VENV_PYTHON} -m pip install --upgrade pip)
  execute_process(COMMAND ${VENV_PYTHON} -m pip install jsonschema jinja2 pyyaml)

  # ----------------------------------------------------------
  # Force entire project + subprojects to use venv python
  # ----------------------------------------------------------
  set(Python3_EXECUTABLE ${VENV_PYTHON} CACHE FILEPATH "Python in venv" FORCE)
  set(PYTHON_EXECUTABLE  ${VENV_PYTHON} CACHE FILEPATH "Python in venv" FORCE)

  message(STATUS "Python set to: ${Python3_EXECUTABLE}")

  # ==========================================================
  # Ruby (only required for unit tests / CMock)
  # ==========================================================
  if(BUILD_TESTS)
    find_package(Ruby QUIET)
    if(NOT Ruby_FOUND)
      message(FATAL_ERROR
        "Ruby was not found but is required to build unit tests (BUILD_TESTS=ON).\n"
        "CMock uses Ruby to generate mocks.\n\n"
        "Install Ruby:\n"
        "  Ubuntu/Debian: sudo apt install ruby\n"
        "  Fedora/RHEL:   sudo dnf install ruby\n"
        "  Arch:          sudo pacman -S ruby\n"
        "  macOS (brew):  brew install ruby\n"
      )
    endif()
  endif()

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

  # If we are NOT building modules, remove the file that requires mbedTLS
  if(NOT BUILD_MODULES)
    list(FILTER UTL_SRCS EXCLUDE REGEX "mbedtls_utils.c")
  endif()

  add_library(maestroutils STATIC ${UTL_SRCS})

  target_include_directories(maestroutils PUBLIC
    utils/include
    modules/include
    ${CJSON_DIR}
    # Only include mbedtls headers when modules are enabled
    $<$<BOOL:${BUILD_MODULES}>:${MBEDTLS_DIR}/include>
  )

  # Only link mbedTLS when modules are enabled
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

# ============================================================
# Target: Unit Tests
# ============================================================
if(BUILD_TESTS)
  link_directories(${CMAKE_BINARY_DIR}/external/mbedtls/library)
  enable_testing()
  add_subdirectory(test)
endif()


# ============================================================
# Target: manual HTTP smoke test
# ============================================================

if(BUILD_MODULES)
  add_executable(http_manual_test manual_http_test_with_states.c)

  target_include_directories(http_manual_test PRIVATE
      modules/include
      utils/include
  )

  # Samma lösning som testbygget, men target-scoped för manual-testet
  target_link_directories(http_manual_test PRIVATE
      ${CMAKE_BINARY_DIR}/external/mbedtls/library
  )

  target_link_libraries(http_manual_test PRIVATE
      maestromodules
      maestroutils
      mbedtls
      mbedx509
      mbedcrypto
      tfpsacrypto
  )
endif()