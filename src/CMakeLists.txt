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
    parsedcommand.cpp
    recccounterguard.cpp
    remoteexecutionclient.cpp
    remoteexecutionsignals.cpp
    subprocess.cpp
)

add_library(remoteexecution STATIC ${SRCS})
target_link_libraries(remoteexecution reccproto ${CURL_LIBRARIES} ${Protobuf_LIBRARIES} ${GRPC_TARGET} OpenSSL::Crypto)

if (CMAKE_SYSTEM_NAME MATCHES "SunOS")
    target_link_libraries(remoteexecution socket nsl)
endif ()

add_executable(recc bin/recc.cpp)
target_link_libraries(recc remoteexecution)

add_executable(reccworker bin/reccworker.cpp)
target_link_libraries(reccworker remoteexecution)

add_executable(casupload bin/casupload.cpp)
target_link_libraries(casupload remoteexecution)

add_executable(deps deps.cpp bin/deps.cpp)
target_link_libraries(deps remoteexecution)

install(TARGETS recc RUNTIME DESTINATION bin)
install(TARGETS reccworker RUNTIME DESTINATION bin)

set_target_properties(remoteexecution PROPERTIES COMPILER_FLAGS "-Wall -Werror")
set_target_properties(recc PROPERTIES COMPILER_FLAGS "-Wall -Werror")
set_target_properties(reccworker PROPERTIES COMPILER_FLAGS "-Wall -Werror")
set_target_properties(casupload PROPERTIES COMPILER_FLAGS "-Wall -Werror")
set_target_properties(deps PROPERTIES COMPILER_FLAGS "-Wall -Werror")
