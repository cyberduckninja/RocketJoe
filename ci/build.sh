#!/usr/bin/env sh

mkdir -p build && \
cd build && \
cmake \
    -GNinja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DDEV_MODE=ON \
    .. && \
cmake --build .

last_return=${?}

if [ ${last_return} -ne 0 ]; then
    echo 'Unable to build app.'
    exit 1
fi
