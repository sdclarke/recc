cmake_minimum_required(VERSION 3.6)

project(recc CXX)

set(CMAKE_CXX_STANDARD 11)
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

find_package(OpenSSL REQUIRED)

find_package(Protobuf REQUIRED)
include_directories(${Protobuf_INCLUDE_DIRS})

find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(gRPC REQUIRED IMPORTED_TARGET grpc++)
endif()
if(gRPC_FOUND)
    set(GRPC_TARGET PkgConfig::gRPC)
    find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin)
else()
    find_package(gRPC REQUIRED)
    set(GRPC_TARGET gRPC::grpc++)
    set(GRPC_CPP_PLUGIN $<TARGET_FILE:gRPC::grpc_cpp_plugin>)
endif()

# define CMAKE_INSTALL_DIR
add_definitions(-DCMAKE_INSTALL_DIR="${CMAKE_INSTALL_PREFIX}")

# define RECC_CONFIG_PREFIX_DIR
add_definitions(-DRECC_CONFIG_PREFIX_DIR="${RECC_CONFIG_PREFIX_DIR}")

include(cmake/protos.cmake)
add_subdirectory(src)

include(CTest)
if(BUILD_TESTING)
    include(cmake/gtest.cmake)
    include_directories(third_party/grpc/include)
    add_subdirectory(test)
endif()
