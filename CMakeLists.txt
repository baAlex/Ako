
cmake_minimum_required(VERSION 3.16)
project("Ako" CXX)


option(AKO_SHARED "Build shared library" ON)
option(AKO_STATIC "Build static library" ON)
option(AKO_DEC    "Build decoding tool"  ON)
option(AKO_ENC    "Build encoding tool"  ON)
option(AKO_TESTS  "Build tests"          ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS True) # For Clangd
set(CMAKE_CXX_STANDARD 11)


set(CFLAGS -Werror -Wall -Wextra -Wconversion -pedantic -Wold-style-cast -Wstrict-aliasing=2)


if (MSVC)
	add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif ()


set(AKO_SOURCES
	"./library/common/adler32.cpp"
	"./library/common/conversions.cpp"
	"./library/common/defaults.cpp"
	"./library/common/essentials.cpp"
	"./library/common/utilities.cpp"
	"./library/common/version.cpp"

	"./library/decode/compression.cpp"
	"./library/decode/decode.cpp"
	"./library/decode/format.cpp"
	"./library/decode/haar.cpp"
	"./library/decode/lifting.cpp"

	"./library/encode/compression.cpp"
	"./library/encode/encode.cpp"
	"./library/encode/format.cpp"
	"./library/encode/haar.cpp"
	"./library/encode/lifting.cpp")


if (AKO_SHARED)
	add_library("ako" SHARED ${AKO_SOURCES})
	target_include_directories("ako" PRIVATE "./library/")

	set_property(TARGET "ako" PROPERTY INTERPROCEDURAL_OPTIMIZATION True)
	set_property(TARGET "ako" PROPERTY CXX_VISIBILITY_PRESET hidden)

	if (NOT MSVC)
		target_compile_options("ako" PRIVATE ${CFLAGS})
	endif ()
endif ()


if (AKO_STATIC)
	add_library("ako-static" STATIC ${AKO_SOURCES})
	target_include_directories("ako-static" PRIVATE "./library/")

	set_property(TARGET "ako-static" PROPERTY INTERPROCEDURAL_OPTIMIZATION True)

	if (NOT MSVC)
		target_compile_options("ako-static" PRIVATE ${CFLAGS})
	endif ()
endif ()


if (AKO_ENC OR AKODEC)
	add_library("tools-common" STATIC
		"./tools/thirdparty/CLI11/src/Precompile.cpp"
		"./tools/thirdparty/lodepng/lodepng.cpp"
		"./tools/callbacks.cpp"
		"./tools/developer.cpp"
		"./tools/png.cpp")
	target_include_directories("tools-common" PRIVATE "./library/")
	target_include_directories("tools-common" PRIVATE "./tools/thirdparty/CLI11/include/")
	target_include_directories("tools-common" PRIVATE "./tools/thirdparty/lodepng/")
endif ()


if (AKO_DEC)
	add_executable("akodec" "./tools/akodec.cpp")
	target_link_libraries("akodec" PRIVATE "ako-static")
	target_link_libraries("akodec" PRIVATE "tools-common")

	target_include_directories("akodec" PRIVATE "./library/")
	target_include_directories("akodec" PRIVATE "./tools/thirdparty/CLI11/include/")
	target_include_directories("akodec" PRIVATE "./tools/thirdparty/lodepng/")

	set_property(TARGET "akodec" PROPERTY INTERPROCEDURAL_OPTIMIZATION True)

	if (NOT MSVC)
		# target_compile_options("akodec" PRIVATE ${CFLAGS})
	endif ()
endif ()


if (AKO_ENC)
	add_executable("akoenc" "./tools/akoenc.cpp")
	target_link_libraries("akoenc" PRIVATE "ako-static")
	target_link_libraries("akoenc" PRIVATE "tools-common")

	target_include_directories("akoenc" PRIVATE "./library/")
	target_include_directories("akoenc" PRIVATE "./tools/thirdparty/CLI11/include/")
	target_include_directories("akoenc" PRIVATE "./tools/thirdparty/lodepng/")

	set_property(TARGET "akoenc" PROPERTY INTERPROCEDURAL_OPTIMIZATION True)

	if (NOT MSVC)
		# target_compile_options("akoenc" PRIVATE ${CFLAGS})
	endif ()
endif ()


if (AKO_TESTS)
	add_executable("forward-backward" "./tests/forward-backward.cpp")
	target_include_directories("forward-backward" PRIVATE "./library/")
	target_link_libraries("forward-backward" PRIVATE "ako-static")
	set_property(TARGET "forward-backward" PROPERTY INTERPROCEDURAL_OPTIMIZATION True)

	add_test(NAME "ForwardBackward" COMMAND "forward-backward")

	include(CTest)
endif ()
