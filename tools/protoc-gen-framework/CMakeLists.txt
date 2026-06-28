find_program(GO_EXECUTABLE go REQUIRED)

set(PLUGIN_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(PLUGIN_BIN     "${CMAKE_BINARY_DIR}/protoc-gen-framework")

file(GLOB_RECURSE PLUGIN_GO_SOURCES "${PLUGIN_SRC_DIR}/*.go")
file(GLOB_RECURSE PLUGIN_TEMPLATES  "${PLUGIN_SRC_DIR}/*.tmpl")

add_custom_command(
    OUTPUT  "${PLUGIN_BIN}"
    COMMAND "${GO_EXECUTABLE}" build -o "${PLUGIN_BIN}" .
    WORKING_DIRECTORY "${PLUGIN_SRC_DIR}"
    DEPENDS ${PLUGIN_GO_SOURCES} ${PLUGIN_TEMPLATES}
    COMMENT "Building protoc-gen-framework plugin"
    VERBATIM
)

add_custom_target(protoc_gen_framework DEPENDS "${PLUGIN_BIN}")
