cmake_minimum_required(VERSION 3.10)



# Project setup

project(event-async)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
if (MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -pedantic -Werror)
endif()

include_directories("/usr/local/include")
link_directories("/usr/local/lib")
list(INSERT CMAKE_SYSTEM_PREFIX_PATH 0 /opt/homebrew)



# Library definitions

find_package(OpenSSL REQUIRED)

find_path     (LIBEVENT_INCLUDE_DIR NAMES event.h)
find_library  (LIBEVENT_LIBRARY     NAMES event)
find_library  (LIBEVENT_CORE        NAMES event_core)
find_library  (LIBEVENT_THREAD      NAMES event_pthreads)
find_library  (LIBEVENT_SSL         NAMES event_openssl)
set (LIBEVENT_INCLUDE_DIRS ${LIBEVENT_INCLUDE_DIR})
set (LIBEVENT_LIBRARIES
        ${LIBEVENT_LIBRARY}
        ${LIBEVENT_CORE}
        ${LIBEVENT_THREAD}
        ${LIBEVENT_SSL})

add_library(event-async src/Base.cc src/Buffer.cc src/Config.cc src/DNSBase.cc src/Event.cc src/Task.cc)
target_include_directories(event-async PUBLIC ${LIBEVENT_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR})
target_link_libraries(event-async phosg ${LIBEVENT_LIBRARIES} ${OPENSSL_LIBRARIES})

add_library(http-async
    src/Protocols/HTTP/Connection.cc
    src/Protocols/HTTP/Request.cc
    src/Protocols/HTTP/Server.cc
)
target_link_libraries(http-async event-async)

add_library(memcache-async
    src/Protocols/Memcache/Client.cc
    src/Protocols/Memcache/Types.cc
)
target_link_libraries(memcache-async event-async)

add_library(mysql-async
    src/Protocols/MySQL/BinlogProcessor.cc
    src/Protocols/MySQL/Client.cc
    src/Protocols/MySQL/ProtocolBuffer.cc
    src/Protocols/MySQL/Types.cc
)
target_link_libraries(mysql-async event-async)



# Executable definitions

add_executable(ControlFlowTests src/Examples/ControlFlowTests.cc)
target_link_libraries(ControlFlowTests event-async)

add_executable(EchoServer src/Examples/EchoServer.cc)
target_link_libraries(EchoServer event-async)

add_executable(UDPEchoServer src/Examples/UDPEchoServer.cc)
target_link_libraries(UDPEchoServer event-async)

add_executable(RangeReverseDNS src/Examples/RangeReverseDNS.cc)
target_link_libraries(RangeReverseDNS event-async)

add_executable(HTTPClient src/Examples/HTTPClient.cc)
target_link_libraries(HTTPClient http-async)

add_executable(HTTPServer src/Examples/HTTPServer.cc)
target_link_libraries(HTTPServer http-async)

add_executable(HTTPWebsocketServer src/Examples/HTTPWebsocketServer.cc)
target_link_libraries(HTTPWebsocketServer http-async)

add_executable(MemcacheFunctionalTest src/Examples/MemcacheFunctionalTest.cc)
target_link_libraries(MemcacheFunctionalTest memcache-async)

add_executable(MySQLBinlogReader src/Examples/MySQLBinlogReader.cc)
target_link_libraries(MySQLBinlogReader mysql-async)

add_executable(MySQLBinlogStats src/Examples/MySQLBinlogStats.cc)
target_link_libraries(MySQLBinlogStats mysql-async)



# Test configuration

enable_testing()

add_test(NAME ControlFlowTests COMMAND ControlFlowTests)



# Installation configuration

file(GLOB Headers ${CMAKE_SOURCE_DIR}/src/*.hh)
file(GLOB HTTPHeaders ${CMAKE_SOURCE_DIR}/src/Protocols/HTTP/*.hh)
file(GLOB MemcacheHeaders ${CMAKE_SOURCE_DIR}/src/Protocols/Memcache/*.hh)
file(GLOB MySQLHeaders ${CMAKE_SOURCE_DIR}/src/Protocols/MySQL/*.hh)

install(TARGETS event-async EXPORT event-async DESTINATION lib)
install(TARGETS http-async EXPORT event-async DESTINATION lib)
install(TARGETS memcache-async EXPORT event-async DESTINATION lib)
install(TARGETS mysql-async EXPORT event-async DESTINATION lib)
install(FILES ${Headers} DESTINATION include/event-async)
install(FILES ${HTTPHeaders} DESTINATION include/event-async/Protocols/HTTP)
install(FILES ${MemcacheHeaders} DESTINATION include/event-async/Protocols/Memcache)
install(FILES ${MySQLHeaders} DESTINATION include/event-async/Protocols/MySQL)

install(FILES event-async-config.cmake DESTINATION lib)
install(EXPORT event-async DESTINATION lib)
