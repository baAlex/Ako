
cmake_minimum_required(VERSION 3.12)
project("Ako" C CXX)


option(AKO_SHARED "Build shared library"   ON)
option(AKO_STATIC "Build static library"   ON)
option(AKO_DEC    "Build decoding tool"    ON)
option(AKO_ENC    "Build encoding tool"    ON)
option(AKO_TESTS  "Build tests"            ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS True) # For Clangd


if (MSVC)
	add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else ()
	add_compile_options(-Werror -Wall -Wextra -pedantic
	                    -Wno-unused-function -Wno-unused-parameter -Wno-unused-variable -Wno-empty-translation-unit)
endif ()


set(AKO_SOURCES
	"./library/compression-rle.c"
	"./library/compression.c"
	"./library/decode.c"
	"./library/developer.c"
	"./library/encode.c"
	"./library/format.c"
	"./library/head.c"
	"./library/kagari.c"
	"./library/lifting.c"
	"./library/misc.c"
	"./library/quantization.c"
	"./library/version.c"
	"./library/wavelet-cdf53.c"
	"./library/wavelet-dd137.c"
	"./library/wavelet-haar.c")


add_library("lodepng-static" STATIC "./tools/thirdparty/lodepng.cpp")


if (AKO_SHARED)
	add_library("ako" SHARED ${AKO_SOURCES})
	set_property(TARGET "ako" PROPERTY INTERPROCEDURAL_OPTIMIZATION True)
	set_property(TARGET "ako" PROPERTY C_VISIBILITY_PRESET hidden)
	set_property(TARGET "ako" PROPERTY C_STANDARD 11)

	if (NOT MSVC)
		target_link_libraries("ako" PRIVATE "m")
	endif ()
endif ()


if (AKO_STATIC)
	add_library("ako-static" STATIC ${AKO_SOURCES})
	set_property(TARGET "ako-static" PROPERTY INTERPROCEDURAL_OPTIMIZATION True)
	set_property(TARGET "ako-static" PROPERTY C_VISIBILITY_PRESET hidden)
	set_property(TARGET "ako-static" PROPERTY C_STANDARD 11)

	if (NOT MSVC)
		target_link_libraries("ako-static" PRIVATE "m")
	endif ()
endif ()


if (AKO_DEC)
	add_executable("akodec" "./tools/akodec.cpp")
	set_property(TARGET "akodec" PROPERTY CXX_STANDARD 14)

	target_include_directories("akodec" PRIVATE "./library/")
	target_link_libraries("akodec" PRIVATE "ako-static")

	target_link_libraries("akodec" PRIVATE "lodepng-static")
endif ()


if (AKO_ENC)
	add_executable("akoenc" "./tools/akoenc.cpp")
	set_property(TARGET "akoenc" PROPERTY CXX_STANDARD 14)

	target_include_directories("akoenc" PRIVATE "./library/")
	target_link_libraries("akoenc" PRIVATE "ako-static")

	target_link_libraries("akoenc" PRIVATE "lodepng-static")
endif ()


if (AKO_TESTS)
	add_executable("elias-test" "./tests/elias-test.c")
	target_include_directories("elias-test" PRIVATE "./library/")
	target_link_libraries("elias-test" PRIVATE "ako-static")

	add_executable("dd137-test" "./tests/dd137-test.c")
	target_include_directories("dd137-test" PRIVATE "./library/")
	target_link_libraries("dd137-test" PRIVATE "ako-static")

	add_executable("cdf53-test" "./tests/cdf53-test.c")
	target_include_directories("cdf53-test" PRIVATE "./library/")
	target_link_libraries("cdf53-test" PRIVATE "ako-static")

	add_executable("rle-test" "./tests/rle-test.c")
	target_include_directories("rle-test" PRIVATE "./library/")
	target_link_libraries("rle-test" PRIVATE "ako-static")
endif ()
