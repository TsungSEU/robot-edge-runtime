#!/bin/bash
# memory_profiler.sh - 内存分析工具
# 用于内存泄漏检测、内存分配分析和堆分析

set -e

# ========== 配置 ==========
BINARY_PATH="${1:-./build/src/dcp}"
DURATION="${2:-60}"
OUTPUT_DIR="tools/performance/memory_results"
VALGRIND_LOG="${OUTPUT_DIR}/valgrind.log"
MASSIF_LOG="${OUTPUT_DIR}/massif.out"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

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

    if ! command -v valgrind &> /dev/null; then
        log_error "valgrind未安装。请安装: sudo apt-get install valgrind"
        exit 1
    fi

    if [ ! -f "$BINARY_PATH" ]; then
        log_error "二进制文件不存在: $BINARY_PATH"
        exit 1
    fi

    mkdir -p "$OUTPUT_DIR"
}

# 内存泄漏检测
run_memcheck() {
    log_info "运行内存泄漏检测 (Memcheck)..."

    local log_file="${OUTPUT_DIR}/memcheck.log"

    # 使用valgrind检测内存泄漏
    valgrind \
        --tool=memcheck \
        --leak-check=full \
        --show-leak-kinds=all \
        --track-origins=yes \
        --verbose \
        --log-file="$log_file" \
        --suppressions=/dev/null \
        -- "$BINARY_PATH" &
    VALGRIND_PID=$!

    # 运行指定时间后停止
    sleep "$DURATION"
    kill -SIGTERM $VALGRIND_PID 2>/dev/null || true
    wait $VALGRIND_PID 2>/dev/null || true

    # 分析结果
    log_info "分析Memcheck结果..."
    if grep -q "ERROR SUMMARY: 0 errors" "$log_file"; then
        log_info "未发现内存错误！"
    else
        log_warn "发现内存问题，请查看日志: $log_file"
    fi

    # 提取摘要
    grep -A 20 "LEAK SUMMARY" "$log_file" > "${OUTPUT_DIR}/leak_summary.txt" || true
}

# 堆分析
run_massif() {
    log_info "运行堆内存分析 (Massif)..."

    local massif_file="${OUTPUT_DIR}/massif.out.%p"

    # 使用massif进行堆分析
    valgrind \
        --tool=massif \
        --massif-out-file="$massif_file" \
        --heap-admin=8 \
        --stacks=yes \
        --time-unit=ms \
        --depth=30 \
        --alloc-fn=operator new \
        --alloc-fn=operator new[] \
        --alloc-fn=malloc \
        -- "$BINARY_PATH" &
    VALGRIND_PID=$!

    sleep "$DURATION"
    kill -SIGTERM $VALGRIND_PID 2>/dev/null || true
    wait $VALGRIND_PID 2>/dev/null || true

    # 找到生成的文件
    local actual_massif_file=$(ls ${OUTPUT_DIR}/massif.out.* 2>/dev/null | head -1)

    if [ -n "$actual_massif_file" ]; then
        log_info "Massif数据已保存到: $actual_massif_file"

        # 使用ms_print生成可读报告
        ms_print "$actual_massif_file" > "${OUTPUT_DIR}/massif_report.txt"

        log_info "Massif报告已保存到: ${OUTPUT_DIR}/massif_report.txt"

        # 显示摘要
        echo ""
        echo "========== 堆内存峰值 =========="
        grep "bytes" "${OUTPUT_DIR}/massif_report.txt" | head -20
    fi
}

# 缓存分析
run_cachegrind() {
    log_info "运行缓存分析 (Cachegrind)..."

    local cachegrind_file="${OUTPUT_DIR}/cachegrind.out.%p"

    valgrind \
        --tool=cachegrind \
        --cachegrind-out-file="$cachegrind_file" \
        --branch-sim=yes \
        -- "$BINARY_PATH" &
    VALGRIND_PID=$!

    sleep "$DURATION"
    kill -SIGTERM $VALGRIND_PID 2>/dev/null || true
    wait $VALGRIND_PID 2>/dev/null || true

    local actual_cachegrind_file=$(ls ${OUTPUT_DIR}/cachegrind.out.* 2>/dev/null | head -1)

    if [ -n "$actual_cachegrind_file" ]; then
        # 生成注解源代码
        cg_annotate "$actual_cachegrind_file" > "${OUTPUT_DIR}/cachegrind_report.txt"

        log_info "缓存分析报告已保存到: ${OUTPUT_DIR}/cachegrind_report.txt"
    fi
}

# 实时内存监控
run_memory_monitor() {
    log_info "启动实时内存监控..."

    local monitor_log="${OUTPUT_DIR}/memory_monitor.log"
    local pid=$(pgrep -f "$(basename $BINARY_PATH)" | head -1)

    if [ -z "$pid" ]; then
        log_warn "未找到运行中的进程"
        return
    fi

    # 清空日志文件
    > "$monitor_log"

    # 写入CSV头
    echo "timestamp,rss_mb,vsz_mb,cpu_percent,threads" >> "$monitor_log"

    local end_time=$(($(date +%s) + DURATION))

    while [ $(date +%s) -lt $end_time ]; do
        if ! kill -0 $pid 2>/dev/null; then
            log_info "进程已结束"
            break
        fi

        local rss=$(ps -p $pid -o rss= | awk '{print $1/1024}')
        local vsz=$(ps -p $pid -o vsz= | awk '{print $1/1024}')
        local cpu=$(ps -p $pid -o %cpu=)
        local threads=$(ps -p $pid -o nlwp=)
        local timestamp=$(date +"%Y-%m-%d %H:%M:%S")

        echo "$timestamp,$rss,$vsz,$cpu,$threads" >> "$monitor_log"
        sleep 1
    done

    log_info "内存监控数据已保存到: $monitor_log"
}

# 生成内存使用图表
generate_memory_chart() {
    log_info "生成内存使用图表..."

    local monitor_log="${OUTPUT_DIR}/memory_monitor.log"
    local chart_file="${OUTPUT_DIR}/memory_chart.txt"

    if [ ! -f "$monitor_log" ]; then
        log_warn "监控日志不存在，跳过图表生成"
        return
    fi

    # 简单的ASCII图表
    {
        echo "========== 内存使用趋势 =========="
        echo ""

        # 计算最大值用于归一化
        local max_rss=$(tail -n +2 "$monitor_log" | cut -d',' -f2 | sort -nr | head -1)
        local max_vsz=$(tail -n +2 "$monitor_log" | cut -d',' -f3 | sort -nr | head -1)

        echo "峰值 RSS: ${max_rss} MB"
        echo "峰值 VSZ: ${max_vsz} MB"
        echo ""

        # 生成ASCII图表 (最近30个样本)
        echo "最近30秒内存使用 (RSS MB):"
        tail -n 30 "$monitor_log" | while IFS=',' read -r timestamp rss vsz cpu threads; do
            local bar_length=$(echo "scale=0; $rss * 50 / $max_rss" | bc)
            local bar=$(printf "%${bar_length}s" | tr ' ' '#')
            printf "%-20s %-10s %s\n" "$timestamp" "${rss}MB" "$bar"
        done

    } > "$chart_file"

    log_info "内存图表已保存到: $chart_file"
}

# 生成汇总报告
generate_summary() {
    local summary_file="${OUTPUT_DIR}/summary.md"

    cat > "$summary_file" << EOF
# 内存分析报告

**生成时间**: $(date)
**分析目标**: $BINARY_PATH
**分析时长**: ${DURATION}秒

## 分析文件

### 内存泄漏检测
- \`memcheck.log\`: Memcheck详细日志
- \`leak_summary.txt\`: 泄漏摘要

### 堆分析
- \`massif.out.*\`: Massif原始数据
- \`massif_report.txt\`: 堆使用报告

### 缓存分析
- \`cachegrind.out.*\`: Cachegrind原始数据
- \`cachegrind_report.txt\`: 缓存命中率报告

### 监控数据
- \`memory_monitor.log\`: 实时内存监控数据
- \`memory_chart.txt\`: 内存使用趋势图

## 快速查看

\`\`\`bash
# 查看内存泄漏摘要
cat tools/performance/memory_results/leak_summary.txt

# 查看堆内存峰值
cat tools/performance/memory_results/massif_report.txt

# 使用ms_plot可视化（如果安装）
ms_plot tools/performance/memory_results/massif.out.*
\`\`\`

## 优化建议

EOF

    # 添加具体建议
    if [ -f "${OUTPUT_DIR}/leak_summary.txt" ]; then
        if grep -q "definitely lost: 0 bytes" "${OUTPUT_DIR}/leak_summary.txt"; then
            echo "✅ 未发现内存泄漏" >> "$summary_file"
        else
            echo "⚠️ 发现内存泄漏，请检查leak_summary.txt" >> "$summary_file"
        fi
    fi

    log_info "摘要报告已保存到: $summary_file"
}

# ========== 主函数 ==========
main() {
    echo "========================================"
    echo "   Aurora Edge Runtime 内存分析工具"
    echo "========================================"
    echo ""

    check_dependencies

    if [ "$1" == "--help" ] || [ "$1" == "-h" ]; then
        echo "用法: $0 [binary_path] [duration_seconds]"
        echo ""
        echo "示例:"
        echo "  $0 ./build/src/dcp 60"
        echo ""
        echo "分析模式:"
        echo "  --memcheck    仅运行内存泄漏检测"
        echo "  --massif      仅运行堆分析"
        echo "  --cachegrind  仅运行缓存分析"
        echo "  --all         运行所有分析 (默认)"
        exit 0
    fi

    case "${3:---all}" in
        --memcheck)
            run_memcheck
            ;;
        --massif)
            run_massif
            ;;
        --cachegrind)
            run_cachegrind
            ;;
        --all)
            run_memcheck
            run_massif
            # run_cachegrind  # 可选：缓存分析
            ;;
        *)
            log_error "未知模式: $3"
            exit 1
            ;;
    esac

    # 如果程序在运行，进行监控
    if pgrep -f "$(basename $BINARY_PATH)" > /dev/null; then
        run_memory_monitor
        generate_memory_chart
    fi

    generate_summary

    echo ""
    log_info "内存分析完成！"
    echo "结果目录: $OUTPUT_DIR"
}

main "$@"
