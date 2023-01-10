
cmake_minimum_required(VERSION 3.16)
project("Ako" CXX)


option(AKO_SHARED "Build shared library" ON)
option(AKO_STATIC "Build static library" ON)
option(AKO_DEC    "Build decoding tool"  ON)
option(AKO_ENC    "Build encoding tool"  ON)
option(AKO_VIEW   "Build viewer tool"    ON)
option(AKO_TESTS  "Build tests"          ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS True)
set(CMAKE_CXX_STANDARD 11)

if (MSVC)
	add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif ()

if (CMAKE_BUILD_TYPE STREQUAL "Release") # OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo"
	set(AKO_RELEASE True)
endif ()

# <optional flags>
if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang" OR "${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
	set(AKO_CFLAGS       -Wall -Wextra -pedantic -Werror -Wconversion -Wold-style-cast)
	set(AKO_TOOLS_CFLAGS -Wall -Wextra -pedantic)
endif ()

if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "MSVC")
	set(AKO_CFLAGS       /W4)
	set(AKO_TOOLS_CFLAGS /W2)
endif ()

if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
	set(CMAKE_CXX_FLAGS_DEBUG    "${CMAKE_CXX_FLAGS_DEBUG}    -fsanitize=undefined -fno-omit-frame-pointer")
	set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fsanitize=undefined -fno-omit-frame-pointer")
endif ()
# </optional flags>


####


set(AKO_SOURCES
	"./library/common/cdf-tables.cpp"
	"./library/common/conversions.cpp"
	"./library/common/defaults.cpp"
	"./library/common/essentials.cpp"
	"./library/common/utilities.cpp"
	"./library/common/version.cpp"

	"./library/decode/ans.cpp"
	"./library/decode/compression.cpp"
	"./library/decode/decode.cpp"
	"./library/decode/format.cpp"
	"./library/decode/heads.cpp"
	"./library/decode/lifting.cpp"
	"./library/decode/wavelet-cdf53.cpp"
	"./library/decode/wavelet-haar.cpp"

	"./library/encode/ans.cpp"
	"./library/encode/compression.cpp"
	"./library/encode/encode.cpp"
	"./library/encode/format.cpp"
	"./library/encode/heads.cpp"
	"./library/encode/lifting.cpp"
	"./library/encode/wavelet-cdf53.cpp"
	"./library/encode/wavelet-haar.cpp")


if (AKO_SHARED)
	add_library("ako" SHARED ${AKO_SOURCES})
	target_include_directories("ako" PRIVATE "./library/")

	target_compile_options("ako" PRIVATE ${AKO_CFLAGS})

	if (AKO_RELEASE)
		set_property(TARGET "ako" PROPERTY CXX_VISIBILITY_PRESET hidden)
		set_property(TARGET "ako" PROPERTY INTERPROCEDURAL_OPTIMIZATION True)
	endif ()

	if (!MSVC)
		target_link_libraries("ako" PRIVATE "m")
	endif ()
endif ()


if (AKO_STATIC)
	add_library("ako-static" STATIC ${AKO_SOURCES})
	target_include_directories("ako-static" PRIVATE "./library/")

	target_compile_options("ako-static" PRIVATE ${AKO_CFLAGS})

	if (AKO_RELEASE)
		set_property(TARGET "ako-static" PROPERTY INTERPROCEDURAL_OPTIMIZATION True)
	endif ()

	if (!MSVC)
		target_link_libraries("ako-static" PRIVATE "m")
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

	target_compile_options("tools-common" PRIVATE ${AKO_TOOLS_CFLAGS})

	if (AKO_RELEASE)
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

	target_compile_options("akodec" PRIVATE ${AKO_TOOLS_CFLAGS})

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

	target_compile_options("akoenc" PRIVATE ${AKO_TOOLS_CFLAGS})

	if (AKO_RELEASE)
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

	target_compile_options("akoview" PRIVATE ${AKO_TOOLS_CFLAGS})

	if (AKO_RELEASE)
		set_property(TARGET "akoview" PROPERTY INTERPROCEDURAL_OPTIMIZATION True)
	endif ()

	set(GLFW_LIBRARY_TYPE "STATIC")
	add_subdirectory("./tools/thirdparty/glfw/")
	target_link_libraries("akoview" PRIVATE "glfw")
endif ()


if (AKO_TESTS)
	add_executable("test-ans" "./tests/ans.cpp")
	target_include_directories("test-ans" PRIVATE "./library/")
	target_link_libraries("test-ans" PRIVATE "ako-static")
	target_compile_options("test-ans" PRIVATE ${AKO_CFLAGS})

	add_executable("test-ans-bits" "./tests/ans-bits.cpp")
	target_include_directories("test-ans-bits" PRIVATE "./library/")
	target_link_libraries("test-ans-bits" PRIVATE "ako-static")
	target_compile_options("test-ans-bits" PRIVATE ${AKO_CFLAGS})

	add_executable("test-forward-backward" "./tests/forward-backward.cpp")
	target_include_directories("test-forward-backward" PRIVATE "./library/")
	target_link_libraries("test-forward-backward" PRIVATE "ako-static")
	target_compile_options("test-forward-backward" PRIVATE ${AKO_CFLAGS})

	add_executable("test-kagari" "./tests/kagari.cpp")
	target_include_directories("test-kagari" PRIVATE "./library/")
	target_link_libraries("test-kagari" PRIVATE "ako-static")
	target_compile_options("test-kagari" PRIVATE ${AKO_CFLAGS})

	add_executable("test-wavelet-cdf53" "./tests/wavelet-cdf53.cpp")
	target_include_directories("test-wavelet-cdf53" PRIVATE "./library/")
	target_link_libraries("test-wavelet-cdf53" PRIVATE "ako-static")
	target_compile_options("test-wavelet-cdf53" PRIVATE ${AKO_CFLAGS})

	add_executable("test-wavelet-haar" "./tests/wavelet-haar.cpp")
	target_include_directories("test-wavelet-haar" PRIVATE "./library/")
	target_link_libraries("test-wavelet-haar" PRIVATE "ako-static")
	target_compile_options("test-wavelet-haar" PRIVATE ${AKO_CFLAGS})

	if (AKO_RELEASE)
		set_property(TARGET "test-ans" PROPERTY INTERPROCEDURAL_OPTIMIZATION True)
		set_property(TARGET "test-ans-bits" PROPERTY INTERPROCEDURAL_OPTIMIZATION True)
		set_property(TARGET "test-forward-backward" PROPERTY INTERPROCEDURAL_OPTIMIZATION True)
		set_property(TARGET "test-kagari" PROPERTY INTERPROCEDURAL_OPTIMIZATION True)
		set_property(TARGET "test-wavelet-cdf53" PROPERTY INTERPROCEDURAL_OPTIMIZATION True)
		set_property(TARGET "test-wavelet-haar" PROPERTY INTERPROCEDURAL_OPTIMIZATION True)
	endif ()

	add_test(NAME "TestAns" COMMAND "test-ans")
	add_test(NAME "TestAnsBits" COMMAND "test-ans-bits")
	add_test(NAME "TestForwardBackward" COMMAND "test-forward-backward")
	add_test(NAME "TestKagari" COMMAND "test-kagari")
	add_test(NAME "TestWaveletCdf53" COMMAND "test-wavelet-cdf53")
	add_test(NAME "TestWaveletHaar" COMMAND "test-wavelet-haar")

	include(CTest)
endif ()
