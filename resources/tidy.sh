#!/bin/bash

flags="-std=c++11 -Wall -Wextra -Wconversion -pedantic -Wold-style-cast -I./library"

files="./library/common/adler32.cpp
       ./library/common/conversions.cpp
       ./library/common/defaults.cpp
       ./library/common/utilities.cpp
       ./library/common/version.cpp
       ./library/decode/decode.cpp
       ./library/decode/format.cpp
       ./library/encode/encode.cpp
       ./library/encode/format.cpp
       ./tests/forward-backward.cpp
       ./tools/akodec.cpp
       ./tools/akoenc.cpp
       ./tools/callbacks.cpp
       ./tools/pgm.cpp"

clang-tidy-12 $files -- $flags
