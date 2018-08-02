cmake_minimum_required(VERSION 3.6)
project(recc CXX)

set(CMAKE_CXX_STANDARD 11)
set(BUILD_SHARED_LIBS off)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# gcc on AIX can't deal with -isystem that contains C++ .h files
if(${CMAKE_SYSTEM_NAME} MATCHES "AIX" AND ${CMAKE_CXX_COMPILER_ID} MATCHES "GNU")
    set(CMAKE_NO_SYSTEM_FROM_IMPORTED ON)
endif()

if(CMAKE_SYSTEM_NAME MATCHES "SunOS")
    add_definitions(-DRECC_PLATFORM_COMPILER="CC")
elseif(CMAKE_SYSTEM_NAME MATCHES "AIX")
    add_definitions(-DRECC_PLATFORM_COMPILER="xlc")
elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
    add_definitions(-DRECC_PLATFORM_COMPILER="gcc")
endif()

set(protobuf_MODULE_COMPATIBLE TRUE)
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)
find_package(OpenSSL REQUIRED)

include(cmake/protos.cmake)
add_subdirectory(src)

include(cmake/gtest.cmake)
include_directories(third_party/grpc/include)
enable_testing()
add_subdirectory(test)
