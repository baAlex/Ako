#!/bin/bash

flags="-std=c++11 -Wall -Wextra -Wconversion -pedantic -Wold-style-cast -I./library"

files="./library/common/conversions.cpp
       ./library/common/defaults.cpp
       ./library/common/utilities.cpp
       ./library/common/version.cpp
       ./library/decode/decode.cpp
       ./library/encode/encode.cpp
       ./tests/encoding-cases.cpp
       ./tools/akodec.cpp
       ./tools/akoenc.cpp
       ./tools/callbacks.cpp"

clang-tidy-12 $files -- $flags
