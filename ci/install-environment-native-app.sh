#!/usr/bin/env sh

if [ -r /etc/os-release ]; then
    . /etc/os-release
elif [ -r /usr/lib/os-release ]; then
    . /usr/lib/os-release
else
    echo 'The os-release file does not exist or is not readable. Unable to'\
         'determine OS.'
    exit 1
fi

conan remote add \
    bincrafters \
    https://api.bintray.com/conan/bincrafters/public-conan && \
conan remote add \
    jinncrafters \
    https://api.bintray.com/conan/jinncrafters/conan

last_return=${?}

if [ ${last_return} -ne 0 ]; then
    echo 'Unable to install native app packages.'
    exit 1
fi

if [ "${ID}" = 'centos' ]; then
    if [ "${VERSION_ID}" = '7' ]; then
        conan install \
            -b missing \
            -b boost \
            -b fmt \
            -b spdlog \
            -b botan \
            -b libsodium \
            -s build_type=Debug \
            -s compiler.libcxx=libstdc++11 \
            .

        last_return=${?}
    elif [ "${VERSION_ID}" -eq '8' ]; then
        conan install \
            -b missing \
            -s build_type=Debug \
            -s compiler.libcxx=libstdc++11 \
            .

        last_return=${?}
    else
        echo 'Unsupported version of the OS distribution.'
        exit 1
    fi

    if [ ${last_return} -ne 0 ]; then
        echo 'Unable to install native app packages.'
        exit 1
    fi
else
    echo 'Unsupported OS distribution.'
    exit 1
fi
