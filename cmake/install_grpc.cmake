# cmake/InstallGRPC.cmake
# Downloads, builds, and installs gRPC v1.81.1 into ${CMAKE_SOURCE_DIR}/third_party/grpc
# at configure time (equivalent to install.sh).
# Skipped automatically if gRPC is already installed there.

function(install_grpc)
    set(GRPC_VERSION     v1.81.1)
    set(GRPC_INSTALL_DIR ${CMAKE_SOURCE_DIR}/third_party/bin)
    set(GRPC_SRC_DIR     ${CMAKE_SOURCE_DIR}/third_party/src/grpc)

    # ------------------------------------------------------------------
    # Skip if already installed
    # ------------------------------------------------------------------
    if(EXISTS "${GRPC_INSTALL_DIR}/lib/libgrpc.a" OR
       EXISTS "${GRPC_INSTALL_DIR}/lib/libgrpc.so" OR
       EXISTS "${GRPC_INSTALL_DIR}/lib64/libgrpc.a" OR
       EXISTS "${GRPC_INSTALL_DIR}/lib64/libgrpc.so")
        message(STATUS "[InstallGRPC] gRPC already installed at ${GRPC_INSTALL_DIR} — skipping.")
        return()
    endif()

    # ------------------------------------------------------------------
    # Clone
    # ------------------------------------------------------------------
    if(NOT EXISTS "${GRPC_SRC_DIR}/.git")
        message(STATUS "[InstallGRPC] Cloning gRPC ${GRPC_VERSION} (this may take a while)...")
        execute_process(
            COMMAND git clone --recurse-submodules -b ${GRPC_VERSION}
                    --depth 1 --shallow-submodules
                    https://github.com/grpc/grpc ${GRPC_SRC_DIR}
            RESULT_VARIABLE CLONE_RESULT
        )
        if(NOT CLONE_RESULT EQUAL 0)
            message(FATAL_ERROR "[InstallGRPC] git clone failed (exit code ${CLONE_RESULT})")
        endif()
    else()
        message(STATUS "[InstallGRPC] Source already cloned at ${GRPC_SRC_DIR}")
    endif()

    # ------------------------------------------------------------------
    # Configure gRPC
    # ------------------------------------------------------------------
    set(GRPC_BUILD_DIR ${GRPC_SRC_DIR}/cmake/build)
    file(MAKE_DIRECTORY ${GRPC_BUILD_DIR})

    message(STATUS "[InstallGRPC] Configuring gRPC...")
    execute_process(
        COMMAND ${CMAKE_COMMAND}
            -DgRPC_INSTALL=ON
            -DgRPC_BUILD_TESTS=OFF
            -DCMAKE_CXX_STANDARD=17
            -DCMAKE_INSTALL_PREFIX=${GRPC_INSTALL_DIR}
            -DCMAKE_BUILD_TYPE=Release
            -DCMAKE_POSITION_INDEPENDENT_CODE=ON
            ${GRPC_SRC_DIR}
        WORKING_DIRECTORY ${GRPC_BUILD_DIR}
        RESULT_VARIABLE CMAKE_CONFIGURE_RESULT
    )
    if(NOT CMAKE_CONFIGURE_RESULT EQUAL 0)
        message(FATAL_ERROR "[InstallGRPC] CMake configure failed (exit code ${CMAKE_CONFIGURE_RESULT})")
    endif()

    # ------------------------------------------------------------------
    # Build & install gRPC (abseil-cpp is built as part of the gRPC build)
    # ------------------------------------------------------------------
    include(ProcessorCount)
    ProcessorCount(CPU_COUNT)
    if(CPU_COUNT EQUAL 0)
        set(CPU_COUNT 4)
    endif()

    message(STATUS "[InstallGRPC] Building gRPC with ${CPU_COUNT} jobs (this may take a while)...")
    execute_process(
        COMMAND ${CMAKE_MAKE_PROGRAM} -j${CPU_COUNT} install
        WORKING_DIRECTORY ${GRPC_BUILD_DIR}
        RESULT_VARIABLE BUILD_RESULT
    )
    if(NOT BUILD_RESULT EQUAL 0)
        message(FATAL_ERROR "[InstallGRPC] Build/install failed (exit code ${BUILD_RESULT})")
    endif()

    message(STATUS "[InstallGRPC] gRPC installed at ${GRPC_INSTALL_DIR}")
endfunction()
