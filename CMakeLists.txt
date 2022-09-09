
cmake_minimum_required(VERSION 3.12)
project("Ako" CXX)


option(AKO_SHARED "Build shared library" ON)
option(AKO_STATIC "Build static library" ON)
option(AKO_DEC    "Build decoding tool"  ON)
option(AKO_ENC    "Build encoding tool"  ON)
option(AKO_TESTS  "Build tests"          ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS True) # For Clangd
set(CMAKE_CXX_STANDARD 11)


if (MSVC)
	add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif ()


set(AKO_SOURCES
	"./library/common/adler32.cpp"
	"./library/common/conversions.cpp"
	"./library/common/defaults.cpp"
	"./library/common/utilities.cpp"
	"./library/common/version.cpp"
	"./library/decode/decode.cpp"
	"./library/decode/format.cpp"
	"./library/encode/encode.cpp"
	"./library/encode/format.cpp")


if (AKO_SHARED)
	add_library("ako" SHARED ${AKO_SOURCES})
	set_property(TARGET "ako" PROPERTY INTERPROCEDURAL_OPTIMIZATION True)
	set_property(TARGET "ako" PROPERTY CXX_VISIBILITY_PRESET hidden)
	target_include_directories("ako" PRIVATE "./library/")

	if (NOT MSVC)
		target_compile_options("ako" PRIVATE -Werror -Wall -Wextra -Wconversion -pedantic -Wold-style-cast)
	endif ()
endif ()


if (AKO_STATIC)
	add_library("ako-static" STATIC ${AKO_SOURCES})
	target_include_directories("ako-static" PRIVATE "./library/")

	if (NOT MSVC)
		target_compile_options("ako-static" PRIVATE -Werror -Wall -Wextra -Wconversion -pedantic -Wold-style-cast)
	endif ()
endif ()


if (AKO_ENC OR AKODEC)
	add_library("thirdparty-static" STATIC
		"./tools/thirdparty/lodepng.cpp")
endif ()


if (AKO_DEC)
	add_executable("akodec"
		"./tools/akodec.cpp"
		"./tools/callbacks.cpp"
		"./tools/png.cpp")

	target_include_directories("akodec" PRIVATE "./library/")
	target_link_libraries("akodec" PRIVATE "ako-static")
	target_link_libraries("akodec" PRIVATE "thirdparty-static")

	if (NOT MSVC)
		target_compile_options("akodec" PRIVATE -Werror -Wall -Wextra -Wconversion -pedantic -Wold-style-cast)
	endif ()
endif ()


if (AKO_ENC)
	add_executable("akoenc"
		"./tools/akoenc.cpp"
		"./tools/callbacks.cpp"
		"./tools/png.cpp")

	target_include_directories("akoenc" PRIVATE "./library/")
	target_link_libraries("akoenc" PRIVATE "ako-static")
	target_link_libraries("akoenc" PRIVATE "thirdparty-static")

	if (NOT MSVC)
		target_compile_options("akoenc" PRIVATE -Werror -Wall -Wextra -Wconversion -pedantic -Wold-style-cast)
	endif ()
endif ()


if (AKO_TESTS)
	add_executable("forward-backward" "./tests/forward-backward.cpp")
	target_include_directories("forward-backward" PRIVATE "./library/")
	target_link_libraries("forward-backward" PRIVATE "ako-static")

	add_test(NAME "ForwardBackward" COMMAND "forward-backward")

	include(CTest)
endif ()
