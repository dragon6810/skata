#!/usr/bin/env bash

if make; then
    mkdir -p run
    cd run
    ../bin/skata $@ > program.s
    clang program.s -o program
    exit 0
fi

exit 1