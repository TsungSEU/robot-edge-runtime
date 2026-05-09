#!/bin/bash
# run_performance_tests.sh - 性能测试一键运行脚本
# 快速启动所有性能分析工具

set -e

# ========== 配置 ==========
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
BINARY="$BUILD_DIR/src/dcp"
DURATION="${1:-30}"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_section() {
    echo ""
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}  $1${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""
}

# ========== 检查 ==========
check_environment() {
    log_section "检查环境"

    # 检查二进制文件
    if [ ! -f "$BINARY" ]; then
        log_warn "二进制文件不存在: $BINARY"
        log_info "正在构建项目..."
        cd "$PROJECT_ROOT"
        mkdir -p build && cd build
        cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo
        make -j$(nproc)
    fi

    # 检查 perf
    if ! command -v perf &> /dev/null; then
        log_warn "perf 未安装，跳过 CPU 性能分析"
        log_info "安装命令: sudo apt-get install linux-tools-common linux-tools-generic"
    fi

    # 检查 valgrind
    if ! command -v valgrind &> /dev/null; then
        log_warn "valgrind 未安装，跳过内存分析"
        log_info "安装命令: sudo apt-get install valgrind"
    fi

    # 创建结果目录
    mkdir -p "$PROJECT_ROOT/tools/performance/results"
}

# ========== CPU 性能分析 ==========
run_cpu_profiling() {
    log_section "CPU 性能分析"

    if command -v perf &> /dev/null; then
        "$PROJECT_ROOT/tools/performance/perf_profiler.sh" "$BINARY" "$DURATION"
    else
        log_warn "跳过 CPU 分析 (perf 未安装)"
    fi
}

# ========== 内存分析 ==========
run_memory_profiling() {
    log_section "内存分析"

    if command -v valgrind &> /dev/null; then
        "$PROJECT_ROOT/tools/performance/memory_profiler.sh" "$BINARY" "$DURATION" --memcheck
    else
        log_warn "跳过内存分析 (valgrind 未安装)"
    fi
}

# ========== 基准测试 ==========
run_benchmarks() {
    log_section "运行基准测试"

    local bench_build="$PROJECT_ROOT/tools/performance/build"
    local bench_binary="$bench_build/run_benchmarks"

    if [ ! -f "$bench_binary" ]; then
        log_info "构建基准测试..."
        mkdir -p "$bench_build"
        cd "$bench_build"
        cmake -DCMAKE_BUILD_TYPE=Release "$PROJECT_ROOT/tools/performance"
        make -j$(nproc)
    fi

    if [ -f "$bench_binary" ]; then
        "$bench_binary" all
    else
        log_warn "基准测试构建失败"
    fi
}

# ========== 生成报告 ==========
generate_report() {
    log_section "生成汇总报告"

    local report_file="$PROJECT_ROOT/tools/performance/results/SUMMARY.md"

    cat > "$report_file" << EOF
# Aurora Edge Runtime 性能测试报告

**生成时间**: $(date)
**测试时长**: ${DURATION}秒
**二进制**: $BINARY

## 测试项目

### CPU 性能分析
- \`perf.data\`: 原始性能数据
- \`hotspots.txt\`: 热点函数列表
- \`flamegraph.svg\`: CPU 火焰图

### 内存分析
- \`memcheck.log\`: 内存泄漏检测日志
- \`leak_summary.txt\`: 泄漏摘要

### 基准测试
- \`*_benchmarks.csv\`: 各组件性能数据

## 快速查看

\`\`\`bash
# 查看热点函数
cat tools/performance/results/hotspots.txt | head -50

# 查看内存泄漏摘要
cat tools/performance/results/leak_summary.txt

# 在浏览器中打开火焰图
firefox tools/performance/results/flamegraph.svg
\`\`\`

## 下一步

1. 查看 \`docs/performance-optimization-guide.md\` 了解优化建议
2. 针对热点函数进行优化
3. 重新运行测试验证改进

EOF

    log_info "汇总报告已生成: $report_file"
}

# ========== 主函数 ==========
main() {
    echo ""
    echo "========================================"
    echo "   Aurora Edge Runtime 性能测试套件"
    echo "========================================"
    echo ""

    check_environment

    # 询问用户要运行哪些测试
    echo ""
    echo "请选择要运行的测试:"
    echo "  1) 全部 (CPU + 内存 + 基准测试)"
    echo "  2) 仅 CPU 性能分析"
    echo "  3) 仅内存分析"
    echo "  4) 仅基准测试"
    echo "  5) 自定义"
    echo ""
    read -p "请输入选项 [1-5]: " choice

    case $choice in
        1)
            run_cpu_profiling
            run_memory_profiling
            run_benchmarks
            ;;
        2)
            run_cpu_profiling
            ;;
        3)
            run_memory_profiling
            ;;
        4)
            run_benchmarks
            ;;
        5)
            echo "可用命令:"
            echo "  CPU分析:   tools/performance/perf_profiler.sh $BINARY $DURATION"
            echo "  内存分析:  tools/performance/memory_profiler.sh $BINARY $DURATION"
            echo "  基准测试:  tools/performance/build/run_benchmarks all"
            ;;
        *)
            log_warn "无效选项，运行全部测试"
            run_cpu_profiling
            run_memory_profiling
            run_benchmarks
            ;;
    esac

    generate_report

    echo ""
    log_info "性能测试完成！"
    echo "结果目录: tools/performance/results/"
    echo ""
}

main "$@"
