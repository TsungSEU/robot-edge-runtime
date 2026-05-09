#!/bin/bash
# perf_profiler.sh - Linux性能分析工具
# 用于CPU性能分析、热点函数检测和火焰图生成

set -e

# ========== 配置 ==========
BINARY_PATH="${1:-./build/src/dcp}"
DURATION="${2:-30}"
OUTPUT_DIR="tools/performance/results"
PERF_DATA="${OUTPUT_DIR}/perf.data"
FLAMEGRAPH_DIR="${OUTPUT_DIR}/flamegraph"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 检查依赖
check_dependencies() {
    log_info "检查依赖..."

    if ! command -v perf &> /dev/null; then
        log_error "perf未安装。请安装: sudo apt-get install linux-tools-common linux-tools-generic"
        exit 1
    fi

    if [ ! -f "$BINARY_PATH" ]; then
        log_error "二进制文件不存在: $BINARY_PATH"
        exit 1
    fi

    # 检查perf事件权限
    if ! perf list 2>&1 | grep -q "cycles"; then
        log_warn "可能需要root权限或设置kernel.perf_event_paranoid"
    fi

    mkdir -p "$OUTPUT_DIR"
    mkdir -p "$FLAMEGRAPH_DIR"
}

# 检查编译时是否有调试符号
check_debug_symbols() {
    log_info "检查调试符号..."

    if file "$BINARY_PATH" | grep -q "stripped"; then
        log_error "二进制文件已剥离符号！请使用 -g 选项重新编译"
        echo "在 CMakeLists.txt 中添加: set(CMAKE_BUILD_TYPE RelWithDebInfo)"
        exit 1
    fi

    log_info "调试符号检查通过"
}

# CPU性能分析
run_cpu_profiler() {
    log_info "开始CPU性能分析 (持续 ${DURATION} 秒)..."
    log_info "启动程序: $BINARY_PATH"

    # 使用perf record采样
    sudo perf record -F 99 -g --call-graph dwarf -o "$PERF_DATA" \
        -- "$BINARY_PATH" &
    PERF_PID=$!

    # 等待指定时间
    sleep "$DURATION"

    # 停止perf
    sudo perf send SIGINT $PERF_PID 2>/dev/null || true
    wait $PERF_PID 2>/dev/null || true

    log_info "性能数据已保存到: $PERF_DATA"
}

# 生成热点函数报告
generate_hotspot_report() {
    local output_file="${OUTPUT_DIR}/hotspots.txt"

    log_info "生成热点函数报告..."

    {
        echo "========== 热点函数报告 =========="
        echo "生成时间: $(date)"
        echo "分析文件: $PERF_DATA"
        echo ""
        echo "========== Top CPU 消耗函数 =========="
        sudo perf report -i "$PERF_DATA" --stdio --no-children -g none -n | head -50

        echo ""
        echo "========== 调用树 (Top 30) =========="
        sudo perf report -i "$PERF_DATA" --stdio -g graph,0.5 -n | head -100

    } > "$output_file"

    log_info "热点报告已保存到: $output_file"
}

# 生成火焰图
generate_flamegraph() {
    log_info "生成火焰图..."

    # 下载FlameGraph工具（如果不存在）
    if [ ! -d "$FLAMEGRAPH_DIR" ]; then
        git clone https://github.com/brendangregg/FlameGraph.git "$FLAMEGRAPH_DIR" 2>/dev/null || true
    fi

    if [ ! -f "${FLAMEGRAPH_DIR}/flamegraph.pl" ]; then
        log_warn "FlameGraph工具未找到，跳过火焰图生成"
        return
    fi

    # 生成折叠调用栈
    sudo perf script -i "$PERF_DATA" | "${FLAMEGRAPH_DIR}/stackcollapse-perf.pl" \
        > "${OUTPUT_DIR}/out.perf-folded"

    # 生成火焰图
    "${FLAMEGRAPH_DIR}/flamegraph.pl" "${OUTPUT_DIR}/out.perf-folded" \
        > "${OUTPUT_DIR}/flamegraph.svg"

    # 生成反火焰图（用于分析off-CPU）
    "${FLAMEGRAPH_DIR}/flamegraph.pl" --inverted "${OUTPUT_DIR}/out.perf-folded" \
        > "${OUTPUT_DIR}/flamegraph_inverted.svg"

    log_info "火焰图已保存到:"
    echo "  - ${OUTPUT_DIR}/flamegraph.svg"
    echo "  - ${OUTPUT_DIR}/flamegraph_inverted.svg"
}

# 系统级性能分析
run_system_profiler() {
    log_info "系统级性能分析..."

    # CPU调度分析
    sudo perf sched record -- sleep "$DURATION" 2>/dev/null || true
    sudo perf sched latency > "${OUTPUT_DIR}/sched_latency.txt" 2>/dev/null || true
    sudo perf sched map > "${OUTPUT_DIR}/sched_map.txt" 2>/dev/null || true

    # 锁分析
    sudo perf lock record -- sleep "$DURATION" 2>/dev/null || true
    sudo perf lock report > "${OUTPUT_DIR}/lock_contention.txt" 2>/dev/null || true

    log_info "系统分析完成"
}

# 生成汇总报告
generate_summary() {
    local summary_file="${OUTPUT_DIR}/summary.md"

    cat > "$summary_file" << EOF
# 性能分析摘要报告

**生成时间**: $(date)
**分析目标**: $BINARY_PATH
**采样时长**: ${DURATION}秒

## 分析文件

- perf.data: 原始性能数据
- hotspots.txt: 热点函数报告
- flamegraph.svg: CPU火焰图
- flamegraph_inverted.svg: 反火焰图

## 快速查看

\`\`\`bash
# 查看热点函数
less tools/performance/results/hotspots.txt

# 在浏览器中打开火焰图
firefox tools/performance/results/flamegraph.svg
\`\`\`

## 下一步优化建议

1. 检查热点函数报告，找出CPU占用最高的函数
2. 查看火焰图，找出调用栈中的热点路径
3. 针对热点函数进行优化

EOF

    log_info "摘要报告已保存到: $summary_file"
}

# ========== 主函数 ==========
main() {
    echo "========================================"
    echo "   Aurora Edge Runtime 性能分析工具"
    echo "========================================"
    echo ""

    check_dependencies
    check_debug_symbols

    # 检查参数
    if [ "$1" == "--help" ] || [ "$1" == "-h" ]; then
        echo "用法: $0 [binary_path] [duration_seconds]"
        echo ""
        echo "示例:"
        echo "  $0 ./build/src/dcp 60"
        echo "  $0 ./build/src/dcp"
        exit 0
    fi

    # 运行分析
    run_cpu_profiler
    generate_hotspot_report
    generate_flamegraph
    # run_system_profiler  # 可选：系统级分析
    generate_summary

    echo ""
    log_info "性能分析完成！"
    echo "结果目录: $OUTPUT_DIR"
    echo ""
    echo "快速查看热点函数:"
    echo "  cat ${OUTPUT_DIR}/hotspots.txt"
    echo ""
    echo "在浏览器中查看火焰图:"
    echo "  firefox ${OUTPUT_DIR}/flamegraph.svg"
}

main "$@"
