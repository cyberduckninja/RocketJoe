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

if [ "${ID}" = 'centos' ]; then
    if [ "${VERSION_ID}" = '7' ]; then
        export LANG=en_US.UTF-8
        export LC_ALL=en_US.UTF-8
        export PATH="${PATH}:${HOME}/.local/bin"

        scl enable devtoolset-8 "${@}"

        last_return=${?}
    elif [ "${VERSION_ID}" -eq '8' ]; then
        export PATH="${PATH}:${HOME}/.local/bin"

        ${@}

        last_return=${?}
    else
        echo 'Unsupported version of the OS distribution.'
        exit 1
    fi

    if [ ${last_return} -ne 0 ]; then
        echo 'Unable to run command.'
        exit 1
    fi
else
    echo 'Unsupported OS distribution.'
    exit 1
fi
