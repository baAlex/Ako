
cmake_minimum_required(VERSION 3.12)
project("Ako" C CXX)


# option(AKO_EXPORT_SYMBOLS "Export symbols" ON)
option(AKO_SHARED "Build shared library"   ON)
option(AKO_STATIC "Build static library"   ON)
option(AKO_ENC "Build encoding tool"       ON)
option(AKO_DEC "Build decoding tool"       ON)


if (MSVC)
	add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else ()
	add_compile_options(-Werror -Wall -Wextra -pedantic -Wno-unused-function -Wno-unused-parameter)
endif ()


set(AKO_SOURCES
	"./library/decode.c"
	"./library/developer.c"
	"./library/encode.c"
	"./library/format.c"
	"./library/head.c"
	"./library/misc.c"
	"./library/version.c")


if (AKO_SHARED)
	add_library("ako" SHARED ${AKO_SOURCES})
	target_include_directories("ako" PUBLIC "./library/")

	if (NOT MSVC)
		target_link_libraries("ako" PRIVATE "m")
	endif ()
endif ()

if (AKO_STATIC)
	add_library("ako-static" STATIC ${AKO_SOURCES})
	target_include_directories("ako-static" PUBLIC "./library/")

	if (NOT MSVC)
		target_link_libraries("ako-static" PRIVATE "m")
	endif ()
endif ()


if (AKO_ENC)
	add_executable("akoenc" "./tools/akoenc.cpp"
	                        "./tools/thirdparty/lodepng.cpp")
	target_link_libraries("akoenc" PRIVATE "ako-static")
endif ()


if (AKO_DEC)
	add_executable("akodec" "./tools/akodec.cpp"
	                        "./tools/thirdparty/lodepng.cpp")
	target_link_libraries("akodec" PRIVATE "ako-static")
endif ()
