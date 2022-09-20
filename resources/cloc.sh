#!/bin/bash

flags=""

files="./library/ako.hpp
       ./library/ako-private.hpp

       ./library/common/adler32.cpp
       ./library/common/defaults.cpp
       ./library/common/essentials.cpp
       ./library/common/utilities.cpp

       ./library/decode/compression.cpp
       ./library/decode/decode.cpp
       ./library/decode/format.cpp
       ./library/decode/haar.cpp
       ./library/decode/lifting.cpp

       ./library/encode/compression.cpp
       ./library/encode/encode.cpp
       ./library/encode/format.cpp
       ./library/encode/haar.cpp
       ./library/encode/lifting.cpp"

# './library/common/conversions.cpp', doesn't count
# './library/common/version.cpp', same

cloc $flags $files
