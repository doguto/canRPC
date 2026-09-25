cmake_minimum_required(VERSION 3.16)
project(can_rpc_host_test CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(can_rpc_host_test test_main.cpp)
target_include_directories(can_rpc_host_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_compile_definitions(can_rpc_host_test PRIVATE CAN_RPC_HOST_TEST)

if(MSVC)
    target_compile_options(can_rpc_host_test PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(can_rpc_host_test PRIVATE -Wall -Wextra)
endif()

enable_testing()
add_test(NAME can_rpc_host_test COMMAND can_rpc_host_test)
