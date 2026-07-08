#!/usr/bin/env bash

if make; then
    mkdir -p run
    cd run
    ../bin/skata $@
    exit $?
fi

exit 1