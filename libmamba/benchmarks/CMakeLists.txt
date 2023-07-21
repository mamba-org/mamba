cmake_minimum_required(VERSION 3.16)


set(LIBMAMBA_BENCH_SRCS
    src/bench_repodata.cpp
)

message(STATUS "Building libmamba C++ benchmarks")

add_executable(bench_libmamba ${LIBMAMBA_BENCH_SRCS})
mamba_target_add_compile_warnings(bench_libmamba WARNING_AS_ERROR ${MAMBA_WARNING_AS_ERROR})

target_include_directories(
    bench_libmamba
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_SOURCE_DIR}/libmamba/src"
)

find_package(benchmark REQUIRED)

target_link_libraries(
    bench_libmamba
    PRIVATE
        libmamba
        benchmark::benchmark
        benchmark::benchmark_main
)

target_compile_features(bench_libmamba PUBLIC cxx_std_17)

add_custom_target(bench COMMAND bench_libmamba DEPENDS bench_libmamba)
