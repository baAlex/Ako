#!/bin/bash

flags="-std=c++11 -Wall -Wextra -Wconversion -pedantic -Wold-style-cast -I./library"

files="./library/common/defaults.cpp
        ./library/common/utilities.cpp
        ./library/common/version.cpp
        ./library/decode/decode.cpp
        ./library/encode/encode.cpp
        ./tests/encoding-cases.cpp"

clang-tidy-12 $files -- $flags
