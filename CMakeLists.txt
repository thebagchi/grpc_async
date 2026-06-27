cmake_minimum_required(VERSION 3.16)
project(grpc_async)

# ── Build settings ─────────────────────────────────────────────────────────────

set(CMAKE_CXX_STANDARD 17)
add_compile_definitions(USING_TEMPLATE)
add_compile_options(-g)

# ── Dependencies ───────────────────────────────────────────────────────────────

include(${CMAKE_SOURCE_DIR}/cmake/install_grpc.cmake)
install_grpc()

set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${CMAKE_SOURCE_DIR}/third_party/bin)

add_subdirectory(tools)
add_subdirectory(proto)

include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/gen)

# ── Code formatting ─────────────────────────────────────────────────────────────
# Runs clang-format in-place over the framework and example sources only when a
# source file is newer than the stamp. Every executable depends on the `format`
# target so formatting always precedes compilation.

find_program(CLANG_FORMAT NAMES clang-format)

file(GLOB_RECURSE FORMAT_SOURCES
    ${CMAKE_SOURCE_DIR}/framework/*.hpp
    ${CMAKE_SOURCE_DIR}/example/*.cc
)

set(FORMAT_STAMP "${CMAKE_BINARY_DIR}/format.stamp")

if(CLANG_FORMAT)
    add_custom_command(
        OUTPUT  "${FORMAT_STAMP}"
        COMMAND ${CLANG_FORMAT} -i --style=file ${FORMAT_SOURCES}
        COMMAND ${CMAKE_COMMAND} -E touch "${FORMAT_STAMP}"
        DEPENDS ${FORMAT_SOURCES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Formatting sources with clang-format"
        VERBATIM
    )
else()
    add_custom_command(
        OUTPUT  "${FORMAT_STAMP}"
        COMMAND ${CMAKE_COMMAND} -E touch "${FORMAT_STAMP}"
        COMMENT "clang-format not found — skipping formatting"
        VERBATIM
    )
endif()

add_custom_target(format ALL DEPENDS "${FORMAT_STAMP}")

# ── Targets ────────────────────────────────────────────────────────────────────

add_executable(example_server
    example/server.cc
)
target_link_libraries(example_server PRIVATE proto_lib)
add_dependencies(example_server format)

add_executable(example_client
    example/client.cc
)
target_link_libraries(example_client PRIVATE proto_lib)
add_dependencies(example_client format)

