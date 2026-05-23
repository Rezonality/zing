cmake_minimum_required(VERSION 3.10)

message(STATUS " CMakeLists: Zing")

option(BUILD_TESTS "Build Tests" ON)
option(ZING_LIBRARY_ONLY "Only build library" OFF)

# Global Settings
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_DEBUG_POSTFIX "-debug")
set(CMAKE_RELWITHDEBINFO_POSTFIX "-reldbg")

if(NOT DEFINED VCPKG_TARGET_TRIPLET)
    if(WIN32)
        set(VCPKG_TARGET_TRIPLET x64-windows-static-md)
    elseif("${CMAKE_SYSTEM_NAME}" STREQUAL "Darwin")
        if(CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "arm64")
            set(VCPKG_TARGET_TRIPLET arm64-osx)
        else()
            set(VCPKG_TARGET_TRIPLET x64-osx)
        endif()
    endif()
endif()

set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake CACHE STRING "Vcpkg toolchain file")

project(Zing
    LANGUAGES CXX C
    VERSION 0.5.0
)

include(cmake/utils.cmake)
find_package(concurrentqueue CONFIG QUIET)
if(NOT TARGET concurrentqueue::concurrentqueue)
    find_package(unofficial-concurrentqueue CONFIG REQUIRED)
    add_library(concurrentqueue::concurrentqueue INTERFACE IMPORTED)
    set_target_properties(concurrentqueue::concurrentqueue PROPERTIES
        INTERFACE_LINK_LIBRARIES unofficial::concurrentqueue::concurrentqueue)
endif()
find_package(portaudio CONFIG REQUIRED)

set(ZING_ROOT ${CMAKE_CURRENT_LIST_DIR})
set(M3RDPARTY_DIR ${CMAKE_CURRENT_LIST_DIR}/m3rdparty)

set(ZING_INCLUDE_DIR include/)
set(ZEST_BUILD_TESTS off)

configure_file(${CMAKE_CURRENT_LIST_DIR}/cmake/config_zing_app.h.cmake ${CMAKE_BINARY_DIR}/config_zing_app.h)

enable_testing()

include(cmake/all.cmake)
add_subdirectory(libs/zest)
add_subdirectory(libs/remidi)

add_subdirectory(src)

if (NOT ${ZING_LIBRARY_ONLY})
add_subdirectory(app)
add_subdirectory(tests)
endif()
