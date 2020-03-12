set(BINARY ${CMAKE_PROJECT_NAME})

include_directories(.)

FILE(GLOB SRCS *.cpp)

if(CMAKE_BUILD_TYPE STREQUAL "DEBUG")
    set(DEBUG_FLAGS -Werror -Wextra -pedantic-errors -Wall -Wconversion -Wno-vla)
endif()

add_library(remoteexecution STATIC ${SRCS})
target_link_libraries(remoteexecution reccproto ${_EXTRA_LDD_FLAGS} ${Protobuf_LIBRARIES} ${GRPC_TARGET} ${OPENSSL_TARGET})

if(BUILD_STATIC)
    target_link_libraries(remoteexecution ${ZLIB_LIBRARIES} ${OS_LIBS})
endif()

if (CMAKE_SYSTEM_NAME MATCHES "SunOS")
    target_link_libraries(remoteexecution socket nsl)
endif ()

# Link remoteexecution to the buildboxcommon library
target_link_libraries(remoteexecution Buildbox::buildboxcommon)

add_executable(${BINARY} bin/${BINARY}.m.cpp)
target_link_libraries(${BINARY} remoteexecution)

add_executable(casupload bin/casupload.m.cpp)
target_link_libraries(casupload remoteexecution)

add_executable(deps deps.cpp bin/deps.m.cpp)
target_link_libraries(deps remoteexecution)

install(TARGETS ${BINARY} RUNTIME DESTINATION bin)

if(${CMAKE_SYSTEM_NAME} MATCHES "AIX" AND ${CMAKE_CXX_COMPILER_ID} MATCHES "GNU")
    message("Skipping all warnings due to GNU compiler + AIX system")
else()
    target_compile_options(remoteexecution PRIVATE -Wall -Werror=shadow ${DEBUG_FLAGS})
    target_compile_options(${BINARY} PRIVATE -Wall -Werror=shadow ${DEBUG_FLAGS})
    target_compile_options(casupload PRIVATE -Wall -Werror=shadow ${DEBUG_FLAGS})
    target_compile_options(deps PRIVATE -Wall -Werror=shadow ${DEBUG_FLAGS})
endif()
