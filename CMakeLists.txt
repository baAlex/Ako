
cmake_minimum_required(VERSION 3.16)
project("Ako" CXX)


option(AKO_SHARED "Build shared library" ON)
option(AKO_STATIC "Build static library" ON)
option(AKO_DEC    "Build decoding tool"  ON)
option(AKO_ENC    "Build encoding tool"  ON)
option(AKO_VIEW   "Build viewer tool"    ON)
option(AKO_TESTS  "Build tests"          ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS True) # For Clangd
set(CMAKE_CXX_STANDARD 11)

if (MSVC)
	add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif ()


set(AKO_CFLAGS -Werror -Wall -Wextra -Wconversion -pedantic -Wold-style-cast -Wstrict-aliasing=2)

if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
	set(CMAKE_CXX_FLAGS_DEBUG    "${CMAKE_CXX_FLAGS_DEBUG}    -fsanitize=undefined -fno-omit-frame-pointer")
	set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fsanitize=undefined -fno-omit-frame-pointer")
endif ()


set(AKO_SOURCES
	"./library/common/conversions.cpp"
	"./library/common/defaults.cpp"
	"./library/common/essentials.cpp"
	"./library/common/utilities.cpp"
	"./library/common/version.cpp"

	"./library/decode/compression.cpp"
	"./library/decode/decode.cpp"
	"./library/decode/format.cpp"
	"./library/decode/lifting.cpp"
	"./library/decode/wavelet-cdf53.cpp"
	"./library/decode/wavelet-haar.cpp"

	"./library/encode/compression.cpp"
	"./library/encode/encode.cpp"
	"./library/encode/format.cpp"
	"./library/encode/lifting.cpp"
	"./library/encode/wavelet-cdf53.cpp"
	"./library/encode/wavelet-haar.cpp")


if (AKO_SHARED)
	add_library("ako" SHARED ${AKO_SOURCES})
	target_include_directories("ako" PRIVATE "./library/")

	if (CMAKE_BUILD_TYPE STREQUAL "Release")
		set_property(TARGET "ako" PROPERTY CXX_VISIBILITY_PRESET hidden)
		set_property(TARGET "ako" PROPERTY INTERPROCEDURAL_OPTIMIZATION True)
	endif ()

	if (NOT MSVC)
		target_compile_options("ako" PRIVATE ${AKO_CFLAGS})
	endif ()
endif ()


if (AKO_STATIC)
	add_library("ako-static" STATIC ${AKO_SOURCES})
	target_include_directories("ako-static" PRIVATE "./library/")

	if (CMAKE_BUILD_TYPE STREQUAL "Release")
		set_property(TARGET "ako-static" PROPERTY INTERPROCEDURAL_OPTIMIZATION True)
	endif ()

	if (NOT MSVC)
		target_compile_options("ako-static" PRIVATE ${AKO_CFLAGS})
	endif ()
endif ()


if (AKO_ENC OR AKODEC OR AKOVIEW)
	add_library("tools-common" STATIC
		"./tools/common/adler32.cpp"
		"./tools/common/callbacks.cpp"
		"./tools/common/developer.cpp"
		"./tools/common/png.cpp"
		"./tools/thirdparty/CLI11/src/Precompile.cpp"
		"./tools/thirdparty/lodepng/lodepng.cpp")

	target_include_directories("tools-common" PRIVATE "./library/")
	target_include_directories("tools-common" PRIVATE "./tools/thirdparty/CLI11/include/")
	target_include_directories("tools-common" PRIVATE "./tools/thirdparty/lodepng/")

	if (CMAKE_BUILD_TYPE STREQUAL "Release")
		set_property(TARGET "tools-common" PROPERTY INTERPROCEDURAL_OPTIMIZATION True)
	endif ()
endif ()


if (AKO_DEC)
	add_executable("akodec" "./tools/akodec.cpp")
	target_link_libraries("akodec" PRIVATE "ako-static")
	target_link_libraries("akodec" PRIVATE "tools-common")

	target_include_directories("akodec" PRIVATE "./library/")
	target_include_directories("akodec" PRIVATE "./tools/thirdparty/CLI11/include/")
	target_include_directories("akodec" PRIVATE "./tools/thirdparty/lodepng/")

	if (AKO_RELEASE)
		set_property(TARGET "akodec" PROPERTY INTERPROCEDURAL_OPTIMIZATION True)
	endif ()
endif ()


if (AKO_ENC)
	add_executable("akoenc" "./tools/akoenc.cpp")
	target_link_libraries("akoenc" PRIVATE "ako-static")
	target_link_libraries("akoenc" PRIVATE "tools-common")

	target_include_directories("akoenc" PRIVATE "./library/")
	target_include_directories("akoenc" PRIVATE "./tools/thirdparty/CLI11/include/")
	target_include_directories("akoenc" PRIVATE "./tools/thirdparty/lodepng/")

	if (CMAKE_BUILD_TYPE STREQUAL "Release")
		set_property(TARGET "akoenc" PROPERTY INTERPROCEDURAL_OPTIMIZATION True)
	endif ()
endif ()


if (AKO_VIEW)
	add_executable("akoview" "./tools/akoview.cpp")
	target_link_libraries("akoview" PRIVATE "ako-static")
	target_link_libraries("akoview" PRIVATE "tools-common")

	target_include_directories("akoview" PRIVATE "./library/")
	target_include_directories("akoview" PRIVATE "./tools/thirdparty/CLI11/include/")
	target_include_directories("akoview" PRIVATE "./tools/thirdparty/lodepng/")
	target_include_directories("akoview" PRIVATE "./tools/thirdparty/glfw/include/")

	if (CMAKE_BUILD_TYPE STREQUAL "Release")
		set_property(TARGET "akoview" PROPERTY INTERPROCEDURAL_OPTIMIZATION True)
	endif ()

	set(GLFW_LIBRARY_TYPE "STATIC")
	add_subdirectory("./tools/thirdparty/glfw/")
	target_link_libraries("akoview" PRIVATE "glfw")
endif ()


if (AKO_TESTS)
	add_executable("forward-backward" "./tests/forward-backward.cpp")
	target_include_directories("forward-backward" PRIVATE "./library/")
	target_link_libraries("forward-backward" PRIVATE "ako-static")

	if (CMAKE_BUILD_TYPE STREQUAL "Release")
		set_property(TARGET "forward-backward" PROPERTY INTERPROCEDURAL_OPTIMIZATION True)
	endif ()

	add_test(NAME "ForwardBackward" COMMAND "forward-backward")

	include(CTest)
endif ()
