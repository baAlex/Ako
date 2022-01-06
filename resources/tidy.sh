#!/bin/bash

cflags="-std=c11 -Wall -Wextra -Wconversion -pedantic -I./library"

cfiles="./library/decode.c
        ./library/encode.c
        ./library/format.c
        ./library/head.c
        ./library/misc.c
        ./library/version.c"

clang-tidy-12 $cfiles -- $cflags
