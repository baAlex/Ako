#!/bin/bash

flags="-I./library -I./tools/thirdparty/CLI11/include -I./tools/thirdparty/lodepng
       -std=c++11 -Werror -Wall -Wextra -Wconversion -pedantic -Wold-style-cast -Wstrict-aliasing=2"

files="./library/common/adler32.cpp
       ./library/common/conversions.cpp
       ./library/common/defaults.cpp
       ./library/common/essentials.cpp
       ./library/common/utilities.cpp
       ./library/common/version.cpp

       ./library/decode/compression.cpp
       ./library/decode/decode.cpp
       ./library/decode/format.cpp
       ./library/decode/lifting.cpp
       ./library/decode/wavelet-cdf53.cpp
       ./library/decode/wavelet-haar.cpp

       ./library/encode/compression.cpp
       ./library/encode/encode.cpp
       ./library/encode/format.cpp
       ./library/encode/lifting.cpp
       ./library/encode/wavelet-cdf53.cpp
       ./library/encode/wavelet-haar.cpp

       ./tests/forward-backward.cpp

       ./tools/akodec.cpp
       ./tools/akoenc.cpp
       ./tools/callbacks.cpp
       ./tools/developer.cpp
       ./tools/png.cpp"

clang-tidy-12 $files -- $flags
