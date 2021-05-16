
cmake_minimum_required(VERSION 3.12)
project("LibAko" C)

option(AKO_EXPORT_SYMBOLS "Export symbols" ON)
option(AKO_SHARED "Build shared library"   ON)
option(AKO_STATIC "Build static library"   ON)
option(AKO_ENC "Build encoder tool"        ON)
option(AKO_DEC "Build decoder tool"        ON)

if (MSVC)
	add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif ()

if (AKO_EXPORT_SYMBOLS)
	add_compile_definitions(AKO_EXPORT_SYMBOLS)
endif ()

if (NOT MSVC) # Assuming Clang or GCC
	set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")
endif ()


set(AKO_SOURCES
	"./library/thirdparty/lz4.c"
	"./library/decode.c"
	"./library/encode.c"
	"./library/dwt-lift-pack.c"
	"./library/dwt-unlift.c"
	"./library/format.c"
	"./library/frame.c"
	"./library/version.c")


if (AKO_SHARED)
	add_library("ako" SHARED ${AKO_SOURCES})
	target_include_directories("ako" PUBLIC "./include/")
endif ()

if (AKO_STATIC)
	add_library("ako-static" STATIC ${AKO_SOURCES})
	target_include_directories("ako-static" PUBLIC "./include/")
endif ()

if (AKO_ENC)
	find_package(PNG)
	add_executable("akoenc" "./tools/akoenc.c")
	target_link_libraries("akoenc" PRIVATE "ako-static")
	target_link_libraries("akoenc" PRIVATE ${PNG_LIBRARY})
endif ()

if (AKO_DEC)
	find_package(PNG)
	add_executable("akodec" "./tools/akodec.c")
	target_link_libraries("akodec" PRIVATE "ako-static")
	target_link_libraries("akodec" PRIVATE ${PNG_LIBRARY})
endif ()
