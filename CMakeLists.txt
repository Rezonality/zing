cmake_minimum_required(VERSION 3.2)

message(STATUS " CMakeLists: Zing")

option(BUILD_TESTS "Build Tests" ON)

# Global Settings
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_DEBUG_POSTFIX "-debug")
set(CMAKE_RELWITHDEBINFO_POSTFIX "-reldbg")

if (WIN32)
set(VCPKG_TARGET_TRIPLET x64-windows-static-md)
endif()

set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake CACHE STRING "Vcpkg toolchain file")

project(Zing
    LANGUAGES CXX C
    VERSION 0.5.0
)

include(cmake/utils.cmake)

find_package(SDL2 REQUIRED)
find_package(fmt REQUIRED)
find_package(freetype CONFIG REQUIRED)
find_package(imgui REQUIRED)

set(ZING_ROOT ${CMAKE_CURRENT_LIST_DIR})
set(M3RDPARTY_DIR ${CMAKE_CURRENT_LIST_DIR}/m3rdparty)
set(ZING_INCLUDE_DIR include/)

configure_file(${CMAKE_CURRENT_LIST_DIR}/cmake/config_app.h.cmake ${CMAKE_BINARY_DIR}/config_app.h)

include(GNUInstallDirs)

enable_testing()

include(cmake/all.cmake)
add_subdirectory(src)
add_subdirectory(app)
add_subdirectory(tests)

# Make the CMake bits that ensure find_package does the right thing
install(EXPORT zing-targets
    FILE zing-targets.cmake
    NAMESPACE zing::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/zing
)

include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${ZING_ROOT}/cmake/zing-config.cmake.in
    ${CMAKE_BINARY_DIR}/cmake/zing-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/zing
)

write_basic_package_version_file(
    ${CMAKE_BINARY_DIR}/cmake/zing-config-version.cmake
    VERSION 1.0
    COMPATIBILITY AnyNewerVersion
)

install(
    FILES
        ${CMAKE_BINARY_DIR}/cmake/zing-config.cmake
        ${CMAKE_BINARY_DIR}/cmake/zing-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/zing
)

