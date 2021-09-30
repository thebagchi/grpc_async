cmake_minimum_required(VERSION 3.10)
project(proto)

find_package(Threads REQUIRED)
find_program(_PROTOBUF_PROTOC protoc)
find_program(_GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)

set(protobuf_MODULE_COMPATIBLE TRUE)
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)

set(GRPC_CPPLIBRARY gRPC::grpc++)
set(GRPC_REFLECTION gRPC::grpc++_reflection)
set(GRPC_LIBPROTOBUF protobuf::libprotobuf)

set(PROTOBUF_PLUGINS_DIR "${CMAKE_SOURCE_DIR}/protobuf")
set(PROTOC_INCLUDE_DIR "${PROTOBUF_PLUGINS_DIR}/grpc/include")

set(GENERATED_DIR "${CMAKE_SOURCE_DIR}/proto")

file(GLOB PROTO_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.proto")

message(${PROTO_FILES})

set(GENERATED_SOURCES)

foreach(PROTO_FILE ${PROTO_FILES})
    get_filename_component(__FILE__ ${PROTO_FILE} REALPATH)
    get_filename_component(__FILE_NAME__ ${PROTO_FILE} NAME)
    get_filename_component(__FILE_PATH__ ${__FILE__} PATH)

    message("Processing ${__FILE__} ...")

    string(REPLACE ".proto" ".pb.cc" PROTO_SRC ${__FILE_NAME__})
    string(REPLACE ".proto" ".pb.h" PROTO_HDR ${__FILE_NAME__})
    string(REPLACE ".proto" ".grpc.pb.cc" GRPC_SRC ${__FILE_NAME__})
    string(REPLACE ".proto" ".grpc.pb.h" GRPC_HDR ${__FILE_NAME__})

    add_custom_command(
            OUTPUT "${GENERATED_DIR}/${PROTO_SRC}" "${GENERATED_DIR}/${PROTO_HDR}" "${GENERATED_DIR}/${GRPC_SRC}" "${GENERATED_DIR}/${GRPC_HDR}"
            COMMAND ${_PROTOBUF_PROTOC}
            ARGS --grpc_out "${GENERATED_DIR}"
            --proto_path="${__FILE_PATH__}"
            --cpp_out "${GENERATED_DIR}"
            -I "${PROTOC_INCLUDE_DIR}"
            --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
            "${__FILE__}"
            DEPENDS "${__FILE__}"
    )

    list(APPEND GENERATED_SOURCES "${GENERATED_DIR}/${PROTO_SRC}")
    list(APPEND GENERATED_SOURCES "${GENERATED_DIR}/${PROTO_HDR}")
    list(APPEND GENERATED_SOURCES "${GENERATED_DIR}/${GRPC_SRC}")
    list(APPEND GENERATED_SOURCES "${GENERATED_DIR}/${GRPC_HDR}")
endforeach()

add_library(
        proto_lib
        ${GENERATED_SOURCES}
)

target_link_libraries(
        proto_lib
        ${GRPC_CPPLIBRARY}
        ${GRPC_REFLECTION}
        ${GRPC_LIBPROTOBUF}
)