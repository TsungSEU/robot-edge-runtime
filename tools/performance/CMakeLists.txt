cmake_minimum_required(VERSION 3.16)
project(aurora_benchmarks CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 优化选项
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")

# 基准测试可执行文件
add_executable(run_benchmarks run_benchmarks.cpp)

# 包含目录
target_include_directories(run_benchmarks PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tools/performance
)

# 链接库
target_link_libraries(run_benchmarks
    pthread
    m
)

# 安装目标
install(TARGETS run_benchmarks DESTINATION bin)

# 性能分析脚本
install(FILES
    perf_profiler.sh
    memory_profiler.sh
    DESTINATION tools/performance
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                GROUP_READ GROUP_EXECUTE
                WORLD_READ WORLD_EXECUTE
)
