# 1.
set(CMOCK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/cmock)
set(UNITY_DIR ${CMOCK_DIR}/vendor/unity)

# 2. Test binary
add_executable(test_suite EXCLUDE_FROM_ALL 
    test_http.c
    ${UNITY_DIR}/src/unity.c           
    ${CMOCK_DIR}/src/cmock.c           
    mocks/Mocktcp_client.c             
)

# 3. Include directories
target_include_directories(test_suite PRIVATE 
    # Project headers
    ${CMAKE_SOURCE_DIR}/utils/include
    ${CMAKE_SOURCE_DIR}/modules/include
    
    # Unity & CMock headers
    ${UNITY_DIR}/src
    ${CMOCK_DIR}/src
    
    # Generated mocks folder
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
)

target_link_libraries(test_suite PRIVATE 
    maestromodules
    maestroutils 
    mbedtls 
    mbedx509 
    mbedcrypto
    tfpsacrypto
)

# 5. Register ctest
add_test(NAME HttpTests COMMAND test_suite)


