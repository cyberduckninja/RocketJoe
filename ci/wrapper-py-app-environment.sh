#!/usr/bin/env sh

cd ci && \
pipenv run bash -c "cd .. && ${@}"

last_return=${?}

if [ ${last_return} -ne 0 ]; then
    echo 'Unable to run command.'
    exit 1
fi
