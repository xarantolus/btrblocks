# ---------------------------------------------------------------------------
# btrblocks
# ---------------------------------------------------------------------------

include(ExternalProject)
find_package(Git REQUIRED)

ExternalProject_Add(
    benchmark_src
    PREFIX "vendor/gbenchmark"
    GIT_REPOSITORY "https://github.com/google/benchmark.git"
    GIT_TAG 25e5c52a112a56acdc37cace73025f7327f03be7
    TIMEOUT 10
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/vendor/gbenchmark
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS} -Wno-deprecated-copy
        -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
        -DBENCHMARK_DOWNLOAD_DEPENDENCIES=ON
    UPDATE_COMMAND ""
)

ExternalProject_Get_Property(benchmark_src install_dir binary_dir)
set(BENCHMARK_INCLUDE_DIR ${install_dir}/include)
set(BENCHMARK_LIBRARY_PATH ${binary_dir}/src/libbenchmark.a)
file(MAKE_DIRECTORY ${BENCHMARK_INCLUDE_DIR})
add_library(gbenchmark STATIC IMPORTED)
set_property(TARGET gbenchmark PROPERTY IMPORTED_LOCATION ${BENCHMARK_LIBRARY_PATH})
set_property(TARGET gbenchmark APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${BENCHMARK_INCLUDE_DIR})

# Dependencies
add_dependencies(gbenchmark benchmark_src)
