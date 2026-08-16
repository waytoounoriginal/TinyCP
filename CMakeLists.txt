cmake_minimum_required(VERSION 3.22)
project(tcp_from_scratch)

set(CMAKE_CXX_STANDARD 20)

add_library(utils STATIC
        utils/Logger.h
        utils/Logger.cpp
        utils/Checksum.cpp
        TcpPacket.cpp)

target_include_directories(utils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

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
        TcpSocket.cpp
        TcpStack.h
        TcpStack.cpp
        utils/Checksum.cpp
        TcpPacket.cpp
        utils/Utils.h)

target_link_libraries(tcp_from_scratch PRIVATE utils)

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
        TcpSocket.cpp
        TcpStack.h
        TcpStack.cpp
        TcpPacket.cpp)

target_link_libraries(packet_dump PRIVATE utils)

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
        IPv4.h
        TransmissionControlBlock.h
        BlockingBuffer.h
        TcpPacket.cpp
)

target_include_directories(ipv4_header_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ipv4_header_tests PRIVATE gtest_main utils)

add_executable(tcp_header_tests
        tests/TcpPacketTest.cpp
        TcpPacket.h
        TcpPacket.cpp
        TcpPacket.cpp)

target_include_directories(tcp_header_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tcp_header_tests PRIVATE gtest_main utils)

add_executable(ipv4_view_tests
        tests/IPv4PacketViewTest.cpp
        IPv4Packet.h
        IPv4Packet.cpp
        IPv4PacketView.h
        TcpPacket.cpp)

target_include_directories(ipv4_view_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ipv4_view_tests PRIVATE gtest_main utils)

add_executable(tcp_view_tests
        tests/TcpPacketViewTest.cpp
        TcpPacket.h
        TcpPacket.cpp
        TcpPacketView.h
        TcpPacket.cpp)

target_include_directories(tcp_view_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tcp_view_tests PRIVATE gtest_main utils)

add_executable(logger_tests
        tests/LoggerTest.cpp
        TcpPacket.cpp
)

target_include_directories(logger_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(logger_tests PRIVATE gtest_main utils)

add_executable(tcp_socket_tests
        tests/TcpSocketTest.cpp
        TcpSocket.h
        TcpSocket.cpp
        TcpStack.h
        TcpStack.cpp
        IPv4Packet.h
        IPv4Packet.cpp
        IPv4PacketView.h
        TcpPacket.h
        TcpPacket.cpp
        TcpPacketView.h
        TunDevice.cpp
        TunDevice.h
        TcpPacket.cpp
        tests/TcpSocketTest.cpp)

target_include_directories(tcp_socket_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tcp_socket_tests PRIVATE gtest_main utils)

add_test(NAME ipv4_header_tests COMMAND ipv4_header_tests)
add_test(NAME tcp_header_tests COMMAND tcp_header_tests)
add_test(NAME ipv4_view_tests COMMAND ipv4_view_tests)
add_test(NAME tcp_view_tests COMMAND tcp_view_tests)
add_test(NAME logger_tests COMMAND logger_tests)
add_test(NAME tcp_socket_tests COMMAND tcp_socket_tests)