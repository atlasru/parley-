cmake_minimum_required(VERSION 3.22)
project(parley_native LANGUAGES C CXX)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
get_filename_component(PARLEY_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../.." ABSOLUTE)
set(TOX "${PARLEY_ROOT}/.deps/toxcore")
set(SODIUM "${PARLEY_ROOT}/.deps/libsodium")
if(NOT EXISTS "${TOX}/third_party/cmp/cmp.c" OR NOT EXISTS "${SODIUM}/src/libsodium/sodium/core.c")
    message(FATAL_ERROR "Run pwsh scripts/native-deps.ps1 from the repository first")
endif()
# Portable upstream implementations, matching the upstream Zig build's source discovery.
# Architecture-specific implementations remain disabled unless the compiler enables them.
configure_file("${SODIUM}/builds/msvc/version.h" "${CMAKE_CURRENT_BINARY_DIR}/sodium/version.h" COPYONLY)
file(GLOB_RECURSE SODIUM_SOURCES CONFIGURE_DEPENDS "${SODIUM}/src/libsodium/*.c")
add_library(sodium STATIC ${SODIUM_SOURCES})
target_include_directories(sodium PUBLIC "${SODIUM}/src/libsodium/include" "${CMAKE_CURRENT_BINARY_DIR}"
    "${SODIUM}/src/libsodium/include/sodium" "${CMAKE_CURRENT_BINARY_DIR}/sodium")
target_compile_definitions(sodium PRIVATE CONFIGURED=1 _GNU_SOURCE=1 NATIVE_LITTLE_ENDIAN=1
    HAVE_STDINT_H=1 HAVE_INTTYPES_H=1 HAVE_ATOMIC_OPS=1 HAVE_C11_MEMORY_FENCES=1
    HAVE_GCC_MEMORY_FENCES=1 HAVE_CLOCK_GETTIME=1 HAVE_GETPID=1 HAVE_MMAP=1 HAVE_MLOCK=1
    HAVE_MADVISE=1 HAVE_MPROTECT=1 HAVE_NANOSLEEP=1 HAVE_POSIX_MEMALIGN=1 HAVE_PTHREAD=1
    HAVE_RAISE=1 HAVE_SYSCONF=1 HAVE_SYS_MMAN_H=1 HAVE_SYS_PARAM_H=1 TLS=_Thread_local)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    target_compile_definitions(sodium PRIVATE HAVE_TI_MODE=1)
endif()
target_compile_options(sodium PRIVATE -fvisibility=hidden -fno-strict-aliasing -fwrapv)
file(GLOB TOX_SOURCES CONFIGURE_DEPENDS "${TOX}/toxcore/*.c" "${TOX}/toxcore/events/*.c")
list(FILTER TOX_SOURCES EXCLUDE REGEX "(_test|_test_util)\\.c$")
add_library(toxcore STATIC ${TOX_SOURCES} "${TOX}/third_party/cmp/cmp.c" "${TOX}/toxencryptsave/toxencryptsave.c")
target_include_directories(toxcore PUBLIC "${TOX}")
target_compile_definitions(toxcore PRIVATE CMP_NO_FLOAT=1 MIN_LOGGER_LEVEL=LOGGER_LEVEL_ERROR)
target_link_libraries(toxcore PUBLIC sodium)
add_library(parley SHARED bridge.cpp)
target_compile_options(parley PRIVATE -Wall -Wextra -Werror)
target_link_options(parley PRIVATE -Wl,-z,max-page-size=16384)
target_link_libraries(parley PRIVATE toxcore)
