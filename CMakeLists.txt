cmake_minimum_required(VERSION 3.20.2)
project(PMSP VERSION 0.1.0 LANGUAGES C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

include(FetchContent)
set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(USE_SHARED_MBEDTLS_LIBRARY OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    mbedtls
    GIT_REPOSITORY https://github.com/Mbed-TLS/mbedtls.git
    GIT_TAG mbedtls-3.6.3
    GIT_SHALLOW TRUE
    GIT_SUBMODULES_RECURSE TRUE
)
FetchContent_MakeAvailable(mbedtls)

add_library(pmsp STATIC src/pmsp.c src/platform/posix/net.c)
target_include_directories(pmsp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(pmsp PUBLIC mbedtls mbedx509 mbedcrypto)
if(UNIX)
    target_compile_options(pmsp PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(pmsp-get examples/https_get.c)
target_link_libraries(pmsp-get PRIVATE pmsp)
if(UNIX)
    target_compile_options(pmsp-get PRIVATE -Wall -Wextra -Wpedantic)
endif()
