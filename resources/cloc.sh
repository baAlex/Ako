#!/bin/bash

flags=""

files="./library/ako.hpp
       ./library/ako-private.hpp

       ./library/common/defaults.cpp
       ./library/common/essentials.cpp
       ./library/common/utilities.cpp

       ./library/decode/compression-kagari.hpp
       ./library/decode/compression-none.hpp
       ./library/decode/decode.cpp
       ./library/decode/format.cpp
       ./library/decode/heads.cpp
       ./library/decode/lifting.cpp
       ./library/decode/wavelet-cdf53.cpp
       ./library/decode/wavelet-haar.cpp

       ./library/encode/compression-kagari.hpp
       ./library/encode/compression-none.hpp
       ./library/encode/compression.cpp
       ./library/encode/encode.cpp
       ./library/encode/format.cpp
       ./library/encode/heads.cpp
       ./library/encode/lifting.cpp
       ./library/encode/wavelet-cdf53.cpp
       ./library/encode/wavelet-haar.cpp"

# './library/common/conversions.cpp', doesn't count
# './library/common/version.cpp', same

cloc $flags $files
