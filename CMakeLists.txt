cmake_minimum_required(VERSION 3.22)
project(tcp_from_scratch)

set(CMAKE_CXX_STANDARD 20)

add_executable(tcp_from_scratch main.cpp
        TunDevice.cpp
        TunDevice.h
        IPv4Packet.h
        IPv4Packet.cpp
        IPv4PacketView.h
        TcpPacket.h
        TcpPacket.cpp
        TcpPacketView.h
        TcpSocket.h
        TcpSocket.cpp)

add_executable(packet_dump
        PacketDump.cpp
        TunDevice.cpp
        TunDevice.h
        IPv4Packet.h
        IPv4Packet.cpp
        IPv4PacketView.h
        TcpPacket.h
        TcpPacket.cpp
        TcpPacketView.h
        TcpSocket.h
        TcpSocket.cpp)

enable_testing()

include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
)
FetchContent_MakeAvailable(googletest)

add_executable(ipv4_header_tests
        tests/IPv4HeaderTest.cpp
        IPv4Packet.h
        IPv4Packet.cpp
        TcpSocket.cpp
        TcpSocket.h)

target_include_directories(ipv4_header_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ipv4_header_tests PRIVATE GTest::gtest_main)

add_executable(tcp_header_tests
        tests/TcpPacketTest.cpp
        TcpPacket.h
        TcpPacket.cpp)

target_include_directories(tcp_header_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tcp_header_tests PRIVATE GTest::gtest_main)

add_executable(ipv4_view_tests
        tests/IPv4PacketViewTest.cpp
        IPv4Packet.h
        IPv4Packet.cpp
        IPv4PacketView.h)

target_include_directories(ipv4_view_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ipv4_view_tests PRIVATE GTest::gtest_main)

add_executable(tcp_view_tests
        tests/TcpPacketViewTest.cpp
        TcpPacket.h
        TcpPacket.cpp
        TcpPacketView.h)

target_include_directories(tcp_view_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tcp_view_tests PRIVATE GTest::gtest_main)

add_test(NAME ipv4_header_tests COMMAND ipv4_header_tests)
add_test(NAME tcp_header_tests COMMAND tcp_header_tests)
add_test(NAME ipv4_view_tests COMMAND ipv4_view_tests)
add_test(NAME tcp_view_tests COMMAND tcp_view_tests)