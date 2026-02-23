# 1. Setup paths
set(CMOCK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cmock")
set(MOCK_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/mocks")
file(MAKE_DIRECTORY ${MOCK_OUT_DIR})

# 2. Skalbar loop
set(HEADERS_TO_MOCK "transport.h")
set(MOCK_SOURCES "")

foreach(HEADER ${HEADERS_TO_MOCK})
    set(MOCK_C "${MOCK_OUT_DIR}/Mock${HEADER}")
    string(REPLACE ".h" ".c" MOCK_C ${MOCK_C})
    list(APPEND MOCK_SOURCES ${MOCK_C})

    add_custom_command(
        OUTPUT "${MOCK_C}"
        # Vi kör ruby inifrån cmock/lib - då hittar den ALLTID sina plugins
        # Vi använder absoluta sökvägar i själva kommandot (genererade av CMake)
        COMMAND ${Ruby_EXECUTABLE} -I. ./cmock.rb 
                "-o${CMAKE_CURRENT_SOURCE_DIR}/cmock_config.yml" 
                "${CMAKE_SOURCE_DIR}/modules/include/maestromodules/${HEADER}"
        DEPENDS "${CMAKE_SOURCE_DIR}/modules/include/maestromodules/${HEADER}" 
                "${CMAKE_CURRENT_SOURCE_DIR}/cmock_config.yml"
        WORKING_DIRECTORY "${CMOCK_DIR}/lib"
        COMMENT "Generating mock for ${HEADER}..."
        VERBATIM
    )
endforeach()

# 3. Target
add_executable(test_suite 
    test_http.c 
    ${MOCK_SOURCES} 
    "${CMOCK_DIR}/src/cmock.c" 
    "${CMOCK_DIR}/vendor/unity/src/unity.c"
)

# Register the executable as a CTest test
add_test(
  NAME test_suite
  COMMAND $<TARGET_FILE:test_suite>
)

# Optional: ensure it runs from the right working dir (if your tests rely on relative paths)
set_tests_properties(test_suite PROPERTIES
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)


target_include_directories(test_suite PRIVATE 
    ${CMAKE_SOURCE_DIR}/utils/include 
    ${CMAKE_SOURCE_DIR}/modules/include 
    ${CMAKE_SOURCE_DIR}/modules/include/maestromodules
    "${CMOCK_DIR}/src" 
    "${CMOCK_DIR}/vendor/unity/src" 
    "${MOCK_OUT_DIR}"
)

target_link_libraries(test_suite PRIVATE maestromodules maestroutils mbedtls mbedx509 mbedcrypto tfpsacrypto)