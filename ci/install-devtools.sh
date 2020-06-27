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
        yum install -y dnf && \
        dnf upgrade --refresh -y && \
        dnf install -y \
            centos-release-scl \
            epel-release && \
        dnf install -y \
            which \
            make \
            devtoolset-8-gcc-c++ \
            cmake3 \
            ninja-build \
            python3-devel \
            python3-pip && \
        mkdir -p "${HOME}/.local/bin" && \
        ln -s "`which cmake3`" "${HOME}/.local/bin/cmake"

        last_return=${?}
    elif [ "${VERSION_ID}" -eq '8' ]; then
        dnf upgrade --refresh -y && \
        dnf install -y 'dnf-command(config-manager)' && \
        dnf config-manager --set-enabled PowerTools && \
        dnf install -y epel-release && \
        dnf install -y \
            which \
            make \
            gcc-c++ \
            cmake3 \
            ninja-build \
            python3-pip && \
        mkdir -p "${HOME}/.local/bin"

        last_return=${?}
    else
        echo 'Unsupported version of the OS distribution.'
        exit 1
    fi

    if [ ${last_return} -ne 0 ]; then
        echo 'Unable to install devtools packages.'
        exit 1
    fi
else
    echo 'Unsupported OS distribution.'
    exit 1
fi
