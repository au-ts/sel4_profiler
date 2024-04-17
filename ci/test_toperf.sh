#!/bin/bash

set -e

echo "CI|INFO: testing toperf example"
cd toperf/example/
../target/debug/toperf --samples-path samples.json --build-dir build
if test -f ./perf.data; then
    echo ""
    echo "CI|INFO: perf.data file created"
else
    echo ""
    echo "CI|INFO: perf.data file not created"
    exit 1
fi

echo "CI|INFO: toperf test passed!"
