set(PROTO_GEN_PATH ${CMAKE_CURRENT_BINARY_DIR}/gens)
set(PROTO_PATH ${CMAKE_CURRENT_SOURCE_DIR}/third_party/googleapis)
set(protos google/api/http google/api/annotations
           google/rpc/code google/rpc/status
           google/longrunning/operations google/bytestream/bytestream
           google/devtools/remoteexecution/v1test/remote_execution
           google/devtools/remoteworkers/v1test2/bots
           google/devtools/remoteworkers/v1test2/worker
           google/watcher/v1/watch)

set(proto_srcs "")
set(proto_headers "")
set(grpc_srcs "")
set(grpc_headers "")
set(grpc_mock_headers "")
set(proto_files "")
foreach(proto ${protos})
    list(APPEND proto_srcs "${PROTO_GEN_PATH}/${proto}.pb.cc")
    list(APPEND proto_headers "${PROTO_GEN_PATH}/${proto}.pb.h")
    list(APPEND grpc_srcs "${PROTO_GEN_PATH}/${proto}.grpc.pb.cc")
    list(APPEND grpc_headers "${PROTO_GEN_PATH}/${proto}.grpc.pb.h")
    list(APPEND grpc_mock_headers "${PROTO_GEN_PATH}/${proto}_mock.grpc.pb.h")
    list(APPEND proto_files "${PROTO_PATH}/${proto}.proto")
endforeach(proto)

file(MAKE_DIRECTORY ${PROTO_GEN_PATH})
add_custom_command(
    OUTPUT ${proto_srcs} ${proto_headers} ${grpc_srcs} ${grpc_headers} ${grpc_mock_headers}
    COMMAND $<TARGET_FILE:protobuf::protoc>
    ARGS "--grpc_out=generate_mock_code=true:${PROTO_GEN_PATH}"
         --cpp_out "${PROTO_GEN_PATH}"
         -I "${PROTO_PATH}"
         -I "${PROTOBUF_INCLUDE_DIR}"
         --plugin=protoc-gen-grpc="$<TARGET_FILE:gRPC::grpc_cpp_plugin>"
         ${proto_files}
    DEPENDS ${proto_files})
 

add_library(reccproto ${grpc_srcs} ${proto_srcs})
target_include_directories(reccproto PUBLIC ${PROTO_GEN_PATH})
target_link_libraries(reccproto gRPC::grpc++_unsecure protobuf::libprotobuf)
