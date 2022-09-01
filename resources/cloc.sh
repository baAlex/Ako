#!/bin/bash

flags=""

files="./library/ako.hpp
       ./library/ako-private.hpp
       ./library/common/defaults.cpp
       ./library/common/utilities.cpp
       ./library/decode/decode.cpp
       ./library/encode/encode.cpp
       ./tests/encoding-cases.cpp"

# './library/common/conversions.cpp', doesn't count
# './library/common/version.cpp'

cloc $flags $files