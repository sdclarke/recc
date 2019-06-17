include_directories(.)

set(SRCS
    actionbuilder.cpp
    authsession.cpp
    casclient.cpp
    commandlineutils.cpp
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

set_target_properties(remoteexecution PROPERTIES COMPILER_FLAGS "-Wall -Werror")
set_target_properties(recc PROPERTIES COMPILER_FLAGS "-Wall -Werror")
set_target_properties(casupload PROPERTIES COMPILER_FLAGS "-Wall -Werror")
set_target_properties(deps PROPERTIES COMPILER_FLAGS "-Wall -Werror")
