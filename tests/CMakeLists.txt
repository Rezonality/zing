if(BUILD_TESTS)

project(unittests
    LANGUAGES CXX C
    VERSION 0.5.0
)

find_package(Catch2 CONFIG REQUIRED)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)

set (TEST_ROOT ${CMAKE_CURRENT_LIST_DIR})
list(APPEND TEST_SOURCES
    ${TEST_ROOT}/CMakeLists.txt
    ${TEST_ROOT}/main.cpp)

file(GLOB_RECURSE FOUND_TEST_SOURCES "${NODEGRAPH_ROOT}/*.test.cpp")
exclude_files_from_dir_in_list("${FOUND_TEST_SOURCES}" "/m3rdparty/" FALSE)

enable_testing()

set (TEST_SOURCES
    ${FOUND_TEST_SOURCES}
    ${TEST_SOURCES}
    )

add_executable(unittests ${TEST_SOURCES})

target_include_directories(unittests PRIVATE
    ${M3RDPARTY_DIR}
    ${CMAKE_BINARY_DIR}
    include
    )

target_link_libraries(unittests
    PRIVATE
        NodeGraph::NodeGraph
        Catch2::Catch2
        Catch2::Catch2WithMain
        ${PLATFORM_LINKLIBS}
        ${CMAKE_THREAD_LIBS_INIT})

add_test(unittests unittests)

install(TARGETS unittests
    EXPORT mutils-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${LIBLEGACY_INCLUDE_DIRS}
)

SOURCE_GROUP(tests REGULAR_EXPRESSION ".*.test.(cpp|h)+")
SOURCE_GROUP(tests FILES ${TEST_SOURCES})

endif()

