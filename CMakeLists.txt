cmake_minimum_required(VERSION 3.10.0)
project(codedoc VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    if (DEFINED CLANG_GCC_TOOLCHAIN_PATH)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --gcc-toolchain=${CLANG_GCC_TOOLCHAIN_PATH}")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --gcc-toolchain=${CLANG_GCC_TOOLCHAIN_PATH}")
    endif()
    if(DEFINED CLANG_CXX_COMPILER_FLAGS)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CLANG_CXX_COMPILER_FLAGS}")
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	if(DEFINED GCC_CXX_COMPILER_FLAGS)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${GCC_CXX_COMPILER_FLAGS}")
    endif()
    # enable c++26 reflection. LTO not supported with annotations yet (ICE).
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-lto -freflection")
endif()

# remove docs folder on clean
set_directory_properties(PROPERTIES
    ADDITIONAL_CLEAN_FILES "${CMAKE_CURRENT_BINARY_DIR}/docs;${CMAKE_CURRENT_BINARY_DIR}/inception"
)

add_executable(codedoc doc_test.cpp)
