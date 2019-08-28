include_directories(.)

set(SRCS
    actionbuilder.cpp
    authsession.cpp
    casclient.cpp
    deps.cpp
    env.cpp
    fileutils.cpp
    formpost.cpp
    grpcretry.cpp
    grpcchannels.cpp
    grpccontext.cpp
    jsonfilemanager.cpp
    merklize.cpp
    metricsconfig.cpp
    parsedcommand.cpp
    reccfile.cpp
    remoteexecutionclient.cpp
    remoteexecutionsignals.cpp
    requestmetadata.cpp
    subprocess.cpp
    reccmetrics/metriccollectorfactory.cpp
    reccmetrics/filewriter.cpp
    reccmetrics/udpwriter.cpp
    reccmetrics/durationmetrictimer.cpp
    reccmetrics/durationmetricvalue.cpp
    reccmetrics/totaldurationmetrictimer.cpp
    reccmetrics/totaldurationmetricvalue.cpp
)

if(CMAKE_BUILD_TYPE STREQUAL "DEBUG")
    set(DEBUG_FLAGS -Werror -Wextra -pedantic-errors -Wall -Wconversion -Wno-vla)
endif()

add_library(remoteexecution STATIC ${SRCS})
target_link_libraries(remoteexecution reccproto ${_EXTRA_LDD_FLAGS} ${CURL_LIBRARIES} ${Protobuf_LIBRARIES} ${GRPC_TARGET} ${OPENSSL_TARGET})

if(BUILD_STATIC)
    target_link_libraries(remoteexecution ${ZLIB_LIBRARIES} ${OS_LIBS})
endif()

if (CMAKE_SYSTEM_NAME MATCHES "SunOS")
    target_link_libraries(remoteexecution socket nsl)
endif ()

add_executable(recc bin/recc.cpp)
target_link_libraries(recc remoteexecution)

add_executable(casupload bin/casupload.cpp)
target_link_libraries(casupload remoteexecution)

add_executable(deps deps.cpp bin/deps.cpp)
target_link_libraries(deps remoteexecution)

install(TARGETS recc RUNTIME DESTINATION bin)

if(${CMAKE_SYSTEM_NAME} MATCHES "AIX" AND ${CMAKE_CXX_COMPILER_ID} MATCHES "GNU")
    message("Skipping all warnings due to GNU compiler + AIX system")
else()
    target_compile_options(remoteexecution PRIVATE -Wall -Werror=shadow ${DEBUG_FLAGS})
    target_compile_options(recc PRIVATE -Wall -Werror=shadow ${DEBUG_FLAGS})
    target_compile_options(casupload PRIVATE -Wall -Werror=shadow ${DEBUG_FLAGS})
    target_compile_options(deps PRIVATE -Wall -Werror=shadow ${DEBUG_FLAGS})
endif()
