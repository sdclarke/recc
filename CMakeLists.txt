cmake_minimum_required(VERSION 3.6)

project(recc CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# gcc on AIX can't deal with -isystem that contains C++ .h files
if(${CMAKE_SYSTEM_NAME} MATCHES "AIX" AND ${CMAKE_CXX_COMPILER_ID} MATCHES "GNU")
    set(CMAKE_NO_SYSTEM_FROM_IMPORTED ON)
endif()

option(BUILD_STATIC "Build statically" OFF)

# Platform-specific settings
if(CMAKE_SYSTEM_NAME MATCHES "SunOS")
    add_definitions(-DRECC_PLATFORM_COMPILER="CC")
    if(BUILD_STATIC)
        set(_EXTRA_LDD_FLAGS "-static-libgcc -static-libstdc++ -Wl,-z,relax=comdat")
        set(OS_LIBS "-lresolv -lsocket -lnsl")
    endif()
elseif(CMAKE_SYSTEM_NAME MATCHES "AIX")
    add_definitions(-DRECC_PLATFORM_COMPILER="xlc")
    if(BUILD_STATIC)
        set(_EXTRA_LDD_FLAGS "-static-libgcc -static-libstdc++ -Wl,-bbigtoc")
    endif()
elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
    add_definitions(-DRECC_PLATFORM_COMPILER="gcc")
    if(BUILD_STATIC)
        set(OS_LIBS "-lresolv -ldl")
    endif()
endif()

# Determine recc's version if none was provided
if(NOT RECC_VERSION)
    find_program(GIT git)

    if(GIT STREQUAL "GIT-NOTFOUND")
        set(GIT_SHA "unknown")
        message("Could not find git, setting version to 'unknown'")
    else()
        execute_process(COMMAND ${GIT} rev-parse --verify HEAD --short
                        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                        OUTPUT_VARIABLE GIT_SHA
                        OUTPUT_STRIP_TRAILING_WHITESPACE)
        message(STATUS "Using git commit as version: ${GIT_SHA}")
    endif()

    set(RECC_VERSION ${GIT_SHA})
endif()

add_definitions(-DRECC_VERSION="${RECC_VERSION}")

include(cmake/deps.cmake)

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
