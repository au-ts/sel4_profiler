#!/bin/bash

set -e

echo "CI|INFO: testing toperf example"
cd toperf/example/
../target/debug/toperf --samples-path samples.json --build-dir build
if test -f ./perf.data; then
    echo ""
    echo "CI|INFO: Passed toperf example test"
else
    echo ""
    echo "CI|INFO: Failed toperf example test"
    exit 1
fi