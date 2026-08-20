cmake_minimum_required(VERSION 3.22)
project(tcp_from_scratch)

set(CMAKE_CXX_STANDARD 20)

option(ENABLE_DEBUG_LOGGING "Enable verbose debug logging in TinyCP" OFF)

# Core TinyCP library
add_library(tinycp_core STATIC
    ConnectionTable.cpp
    ListenerTable.cpp
    SocketTable.cpp
    TcpSocket.cpp
    TcpStack.cpp
    TunDevice.cpp
    IPv4Packet.cpp
    TcpPacket.cpp
    utils/Checksum.cpp
    utils/Logger.cpp
)

target_include_directories(tinycp_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(ENABLE_DEBUG_LOGGING)
    target_compile_definitions(tinycp_core PUBLIC TCP_DEBUG_LOGGING=1)
endif()

# Primary Executables
add_executable(tcp_from_scratch main.cpp)
target_link_libraries(tcp_from_scratch PRIVATE tinycp_core)

add_executable(packet_dump PacketDump.cpp)
target_link_libraries(packet_dump PRIVATE tinycp_core)

add_executable(http_client http_client.cpp)
target_link_libraries(http_client PRIVATE tinycp_core)

add_executable(load_test load_test.cpp)
target_link_libraries(load_test PRIVATE tinycp_core)

add_executable(throughput_benchmark throughput_benchmark.cpp)
target_link_libraries(throughput_benchmark PRIVATE tinycp_core)

# Testing Suite
enable_testing()

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
)
FetchContent_MakeAvailable(googletest)

# Test helper library with testing hooks enabled
add_library(tinycp_testing STATIC
    ConnectionTable.cpp
    ListenerTable.cpp
    SocketTable.cpp
    TcpSocket.cpp
    TcpStack.cpp
    TunDevice.cpp
    IPv4Packet.cpp
    TcpPacket.cpp
    utils/Checksum.cpp
    utils/Logger.cpp
)
target_include_directories(tinycp_testing PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(tinycp_testing PUBLIC TCP_STACK_TESTING=1)

macro(add_tinycp_test TEST_NAME TEST_SOURCE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_link_libraries(${TEST_NAME} PRIVATE gtest_main tinycp_testing)
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endmacro()

add_tinycp_test(ipv4_header_tests tests/IPv4HeaderTest.cpp)
add_tinycp_test(ipv4_address_tests tests/IPv4AddressTest.cpp)
add_tinycp_test(tcp_header_tests tests/TcpPacketTest.cpp)
add_tinycp_test(ipv4_view_tests tests/IPv4PacketViewTest.cpp)
add_tinycp_test(tcp_view_tests tests/TcpPacketViewTest.cpp)
add_tinycp_test(logger_tests tests/LoggerTest.cpp)
add_tinycp_test(tcp_socket_tests tests/TcpSocketTest.cpp)
add_tinycp_test(rfc793_scenarios_tests tests/Rfc793ScenariosTest.cpp)
add_tinycp_test(tcp_teardown_tests tests/TcpTeardownTest.cpp)
add_tinycp_test(http_client_tests tests/HttpClientTest.cpp)
add_tinycp_test(load_tests tests/LoadTest.cpp)
add_tinycp_test(throughput_tests tests/ThroughputTest.cpp)