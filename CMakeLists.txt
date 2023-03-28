cmake_minimum_required(
  VERSION 3.8 
)

project(NetworkBackend)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
include_directories(ping_pong)
include_directories(include)
include_directories(example)
include_directories(network)
include_directories(external/libfabric_1_16_1/include)
link_directories(external/libfabric_1_16_1/lib)
include_directories(external/ucx-1.14.0/include)
link_directories(external/ucx-1.14.0/lib)

add_definitions(-g -Wall -Wno-pedantic -Wno-unused-variable -DDEFAULT_DEBUG_LEVEL=5)

add_executable(ping_pong examples/main_ping_pong.cpp src/log.c src/eventloop.cpp src/networkPosix.cpp src/network.cpp src/networkLibfabric.cpp src/networkUCX.cpp)
target_link_libraries(ping_pong PRIVATE Threads::Threads -l:libfabric.so -l:libucp.so -l:libuct.so)
set_property(TARGET ping_pong PROPERTY CXX_STANDARD 17)

add_executable(unit_tests_tcp test/unit_tests_posix.cpp src/log.c src/eventloop.cpp src/networkPosix.cpp src/network.cpp src/networkLibfabric.cpp src/networkUCX.cpp)
target_link_libraries(unit_tests_tcp PRIVATE Threads::Threads -l:libfabric.so -l:libucp.so -l:libuct.so)
set_property(TARGET ping_pong PROPERTY CXX_STANDARD 17)