#!/bin/bash

set -e

SDK_PATH=$1

CI_BUILD_DIR="ci_build"

[[ -z $SDK_PATH ]] && echo "usage: example.sh [PATH TO SDK]" && exit 1
[[ ! -d $SDK_PATH ]] && echo "The path to the SDK provided does not exist: '$SDK_PATH'" && exit 1

build_profiler_example() {
    BOARD=$1
    echo "CI|INFO: building profiler example for board: $BOARD"
    BUILD_DIR="${PWD}/${CI_BUILD_DIR}/example/${BOARD}"
    rm -rf ${BUILD_DIR}
    mkdir -p ${BUILD_DIR}
    make -C example   \
        BUILD_DIR=${BUILD_DIR} \
        MICROKIT_SDK=${SDK_PATH} \
        MICROKIT_BOARD=${BOARD}
}

build_profiler_example "maaxboard"
build_profiler_example "odroidc4"

echo ""
echo "CI|INFO: Passed profiler example test"