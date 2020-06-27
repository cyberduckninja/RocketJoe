#!/usr/bin/env sh

pip3 install \
    --user \
    -r ci/requirements-pipenv.txt

last_return=${?}

if [ ${last_return} -ne 0 ]; then
    echo 'Unable to pipenv install package.'
    exit 1
fi
