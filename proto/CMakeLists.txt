# ── Dependencies ───────────────────────────────────────────────────────────────

find_package(Threads   REQUIRED)

set(protobuf_MODULE_COMPATIBLE TRUE)
cmake_policy(SET CMP0077 NEW)
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC     CONFIG REQUIRED)

find_program(PROTOC_EXECUTABLE   protoc)
find_program(GRPC_CPP_PLUGIN     grpc_cpp_plugin)
find_program(CLANG_FORMAT        clang-format)

# ── Aliases ────────────────────────────────────────────────────────────────────

set(GRPC_CPP_LIBRARY  gRPC::grpc++)
set(GRPC_REFLECTION   gRPC::grpc++_reflection)
set(GRPC_PROTOBUF_LIB protobuf::libprotobuf)

set(PLUGIN_BIN "${CMAKE_BINARY_DIR}/protoc-gen-framework")

# ── Output directories ────────────────────────────────────────────────────────

set(GENERATED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/generated")
set(FRAMEWORK_GEN_DIR "${CMAKE_SOURCE_DIR}/gen")

file(MAKE_DIRECTORY "${GENERATED_DIR}")
file(MAKE_DIRECTORY "${FRAMEWORK_GEN_DIR}")

# ── Code generation ────────────────────────────────────────────────────────────

file(GLOB PROTO_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.proto")

set(GENERATED_SOURCES)

foreach(PROTO_FILE IN LISTS PROTO_FILES)
    get_filename_component(PROTO_ABS_PATH "${PROTO_FILE}" REALPATH)
    get_filename_component(PROTO_FILENAME "${PROTO_FILE}" NAME)
    get_filename_component(PROTO_BASENAME "${PROTO_FILE}" NAME_WE)
    get_filename_component(PROTO_DIR      "${PROTO_ABS_PATH}" DIRECTORY)

    string(REPLACE ".proto" ".pb.cc"             PROTO_SRC  "${PROTO_FILENAME}")
    string(REPLACE ".proto" ".pb.h"              PROTO_HDR  "${PROTO_FILENAME}")
    string(REPLACE ".proto" ".grpc.pb.cc"        GRPC_SRC   "${PROTO_FILENAME}")
    string(REPLACE ".proto" ".grpc.pb.h"         GRPC_HDR   "${PROTO_FILENAME}")
    string(REPLACE ".proto" ".framework.pb.h"    FWRK_HDR   "${PROTO_FILENAME}")

    # Generate .pb.{h,cc} and .grpc.pb.{h,cc} as before.
    add_custom_command(
        OUTPUT
            "${GENERATED_DIR}/${PROTO_SRC}"
            "${GENERATED_DIR}/${PROTO_HDR}"
            "${GENERATED_DIR}/${GRPC_SRC}"
            "${GENERATED_DIR}/${GRPC_HDR}"
        COMMAND "${PROTOC_EXECUTABLE}"
        ARGS
            --cpp_out  "${GENERATED_DIR}"
            --grpc_out "${GENERATED_DIR}"
            --proto_path "${PROTO_DIR}"
            --plugin=protoc-gen-grpc="${GRPC_CPP_PLUGIN}"
            "${PROTO_ABS_PATH}"
        DEPENDS "${PROTO_ABS_PATH}"
        COMMENT "Generating protobuf/gRPC sources for ${PROTO_FILENAME}"
    )

    # Generate .framework.pb.h into gen/.
    add_custom_command(
        OUTPUT  "${FRAMEWORK_GEN_DIR}/${FWRK_HDR}"
        COMMAND "${PROTOC_EXECUTABLE}"
        ARGS
            --plugin=protoc-gen-framework=${PLUGIN_BIN}
            --framework_out=${FRAMEWORK_GEN_DIR}
            --proto_path "${PROTO_DIR}"
            "${PROTO_ABS_PATH}"
        DEPENDS "${PROTO_ABS_PATH}" protoc_gen_framework
        COMMENT "Generating framework adapter header for ${PROTO_FILENAME}"
    )

    # Format the generated header in-place if clang-format is available.
    if(CLANG_FORMAT)
        add_custom_command(
            OUTPUT  "${FRAMEWORK_GEN_DIR}/${FWRK_HDR}"
            COMMAND "${CLANG_FORMAT}" -i --style=file "${FRAMEWORK_GEN_DIR}/${FWRK_HDR}"
            APPEND
        )
    endif()

    list(APPEND GENERATED_SOURCES
        "${GENERATED_DIR}/${PROTO_SRC}"
        "${GENERATED_DIR}/${PROTO_HDR}"
        "${GENERATED_DIR}/${GRPC_SRC}"
        "${GENERATED_DIR}/${GRPC_HDR}"
    )
    list(APPEND FRAMEWORK_HEADERS "${FRAMEWORK_GEN_DIR}/${FWRK_HDR}")
endforeach()

# ── Library ────────────────────────────────────────────────────────────────────

# Explicit target so the framework headers are always generated even though they
# are header-only (CMake only schedules custom_command outputs that are consumed
# by a real target or another custom_target).
add_custom_target(framework_headers ALL DEPENDS ${FRAMEWORK_HEADERS})
add_dependencies(framework_headers protoc_gen_framework)

# Collect only .cc files (not headers) for the library sources.
set(LIB_SOURCES)
foreach(SRC IN LISTS GENERATED_SOURCES)
    if(SRC MATCHES "\\.cc$")
        list(APPEND LIB_SOURCES "${SRC}")
    endif()
endforeach()

add_library(proto_lib ${LIB_SOURCES})
add_dependencies(proto_lib protoc_gen_framework framework_headers)

target_link_libraries(proto_lib
    PUBLIC
        ${GRPC_CPP_LIBRARY}
        ${GRPC_REFLECTION}
        ${GRPC_PROTOBUF_LIB}
)

target_include_directories(proto_lib
    PUBLIC
        "${GENERATED_DIR}"
        "${FRAMEWORK_GEN_DIR}"
)