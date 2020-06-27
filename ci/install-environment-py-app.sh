#!/usr/bin/env sh


cd ci && \
pipenv install \
    --ignore-pipfile \
    --keep-outdated

last_return=${?}

if [ ${last_return} -ne 0 ]; then
    echo 'Unable to install py app packages.'
    exit 1
fi
