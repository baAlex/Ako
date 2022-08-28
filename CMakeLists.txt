
cmake_minimum_required(VERSION 3.12)
project("Ako" CXX)


option(AKO_SHARED "Build shared library"   ON)
option(AKO_STATIC "Build static library"   ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS True) # For Clangd
set(CMAKE_CXX_STANDARD 11)


if (MSVC)
	add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else ()
	add_compile_options(-Werror -Wall -Wextra -Wconversion -pedantic -Wold-style-cast)
endif ()


set(AKO_SOURCES
	"./library/common/misc.cpp"
	"./library/decode/decode.cpp"
	"./library/encode/encode.cpp")


if (AKO_SHARED)
	add_library("ako" SHARED ${AKO_SOURCES})
	set_property(TARGET "ako" PROPERTY INTERPROCEDURAL_OPTIMIZATION True)
	set_property(TARGET "ako" PROPERTY CXX_VISIBILITY_PRESET hidden)
	target_include_directories("ako" PRIVATE "./library/")
endif ()


if (AKO_STATIC)
	add_library("ako-static" STATIC ${AKO_SOURCES})
	set_property(TARGET "ako-static" PROPERTY INTERPROCEDURAL_OPTIMIZATION True)
	set_property(TARGET "ako-static" PROPERTY CXX_VISIBILITY_PRESET hidden)
	target_include_directories("ako-static" PRIVATE "./library/")
endif ()
