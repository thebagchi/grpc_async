#!/bin/bash

BASEDIR=$(pwd)
mkdir -p $BASEDIR/protobuf/grpc
git clone --recurse-submodules -b v1.40.0 https://github.com/grpc/grpc
pushd grpc

# Building grpc
mkdir -p cmake/build
pushd cmake/build

cmake -DgRPC_INSTALL=ON                             \
      -DgRPC_BUILD_TESTS=OFF                        \
      -DCMAKE_INSTALL_PREFIX=$BASEDIR/protobuf/grpc \
      ../..
make clean; make; make install
popd

# Building abseil
mkdir -p third_party/abseil-cpp/cmake/build
pushd third_party/abseil-cpp/cmake/build

cmake -DCMAKE_INSTALL_PREFIX=$BASEDIR/protobuf/grpc \
      -DCMAKE_POSITION_INDEPENDENT_CODE=TRUE        \
      ../..
make clean; make; make install
popd

rm -rf $BASEDIR/grpc
