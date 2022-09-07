#!/bin/bash

flags=""

files="./library/ako.hpp
       ./library/ako-private.hpp
       ./library/common/defaults.cpp
       ./library/common/utilities.cpp
       ./library/decode/decode.cpp
       ./library/decode/format.cpp
       ./library/encode/encode.cpp
       ./library/encode/format.cpp"

# './library/common/conversions.cpp', doesn't count
# './library/common/version.cpp', same

cloc $flags $files
