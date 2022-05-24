#!/bin/bash

cflags="-std=c11 -Wall -Wextra -Wconversion -pedantic -I./library"

cfiles="./library/compression-rle.c
        ./library/compression.c
        ./library/decode.c
        ./library/developer.c
        ./library/encode.c
        ./library/format.c
        ./library/head.c
        ./library/kagari.c
        ./library/lifting.c
        ./library/misc.c
        ./library/quantization.c
        ./library/version.c"

clang-tidy-12 $cfiles -- $cflags

clang-tidy-12 -checks=-bugprone-narrowing-conversions "./library/wavelet-cdf53.c" -- $cflags
clang-tidy-12 -checks=-bugprone-narrowing-conversions "./library/wavelet-dd137.c" -- $cflags
clang-tidy-12 -checks=-bugprone-narrowing-conversions "./library/wavelet-haar.c" -- $cflags
