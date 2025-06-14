if(NOT DEFINED CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
    message(STATUS "Using vcpkg toolchain from: $ENV{VCPKG_ROOT}")
endif()

cmake_minimum_required(VERSION 3.15)
project(X410)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# find_path(DETOURS_INCLUDE_DIRS "detours/detours.h")
find_path(DETOURS_INCLUDE_DIRS
    NAMES detours.h
    PATH_SUFFIXES detours
    DOC "Path to Detours headers"
    REQUIRED
)
find_library(DETOURS_LIBRARY detours REQUIRED)


add_library(X410 SHARED X410.cpp)

target_include_directories(X410 PRIVATE ${DETOURS_INCLUDE_DIRS})
target_link_libraries(X410 PRIVATE ${DETOURS_LIBRARY})
target_link_libraries(X410 PRIVATE Dbghelp)
target_compile_definitions(X410 PRIVATE BUILDING_DLL)
