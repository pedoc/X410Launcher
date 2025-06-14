if(NOT DEFINED CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
    message(STATUS "Using vcpkg toolchain from: $ENV{VCPKG_ROOT}")
endif()

cmake_minimum_required(VERSION 3.15)
project(Launcher)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(ARCH_TAG "UNKNOWN")

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^[Aa][Rr][Mm]64$")
    set(ARCH_TAG "Arm64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^[Xx]86_64$" OR CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64")
    set(ARCH_TAG "X64")
endif()

set(LIBRARY_OUTPUT_NAME "${PROJECT_NAME}.${ARCH_TAG}")

# find_path(DETOURS_INCLUDE_DIRS "detours/detours.h")
find_path(DETOURS_INCLUDE_DIRS
    NAMES detours.h
    PATH_SUFFIXES detours
    DOC "Path to Detours headers"
    REQUIRED
)
find_library(DETOURS_LIBRARY detours REQUIRED)


add_library(${PROJECT_NAME} SHARED ${PROJECT_NAME}.cpp)
set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME ${LIBRARY_OUTPUT_NAME}
)

target_include_directories(${PROJECT_NAME} PRIVATE ${DETOURS_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE ${DETOURS_LIBRARY})
target_compile_definitions(${PROJECT_NAME} PRIVATE BUILDING_DLL)


add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:${PROJECT_NAME}>"
        "${CMAKE_SOURCE_DIR}/../$<TARGET_FILE_NAME:${PROJECT_NAME}>"
    COMMENT "Copying DLL to ../"
)