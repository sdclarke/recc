set(PROTO_GEN_PATH ${CMAKE_CURRENT_BINARY_DIR}/gens)
set(PROTO_PATH_THIRD_PARTY ${CMAKE_CURRENT_SOURCE_DIR}/protos/third_party/googleapis)
set(PROTO_PATH_INTERNAL ${CMAKE_CURRENT_SOURCE_DIR}/protos)
set(protos google/api/http google/api/annotations
           google/rpc/code google/rpc/status
           google/longrunning/operations
           build/bazel/semver/semver)

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
    list(APPEND proto_files "${PROTO_PATH_THIRD_PARTY}/${proto}.proto")
endforeach(proto)

file(MAKE_DIRECTORY ${PROTO_GEN_PATH})
add_custom_command(
    OUTPUT ${proto_srcs} ${proto_headers} ${grpc_srcs} ${grpc_headers} ${grpc_mock_headers}
    COMMAND ${Protobuf_PROTOC_EXECUTABLE}
    ARGS "--grpc_out=generate_mock_code=true:${PROTO_GEN_PATH}"
         --cpp_out "${PROTO_GEN_PATH}"
         -I "${PROTO_PATH_THIRD_PARTY}"
         -I "${PROTOBUF_INCLUDE_DIR}"
         --plugin=protoc-gen-grpc="${GRPC_CPP_PLUGIN}"
         ${proto_files}
    DEPENDS ${proto_files})

add_library(reccproto ${grpc_srcs} ${proto_srcs})
target_include_directories(reccproto PUBLIC ${PROTO_GEN_PATH})
target_link_libraries(reccproto ${GRPC_TARGET} ${Protobuf_LIBRARIES})

if(${CMAKE_SYSTEM_NAME} MATCHES "AIX")
    # AIX's sys/sysmacros.h defines a macro called major(). Bazel's semver
    # protobuf defines a property called major, which produces a method called
    # major() in the generated code.

    # To resolve this, pass a flag to the compiler to prevent sys/sysmacros.h
    # from defining anything.
    target_compile_options(reccproto PUBLIC -D_H_SYSMACROS)
endif()
