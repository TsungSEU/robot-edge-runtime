#!/bin/bash
# AER运行时性能测试脚本
# 使用 perf 和 time 工具收集性能数据

set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
RESULTS_DIR="$PROJECT_ROOT/tools/performance/results/runtime"
DURATION="${1:-20}"

mkdir -p "$RESULTS_DIR"

echo "========================================"
echo "   AER运行时性能测试"
echo "========================================"
echo "测试时长: ${DURATION} 秒"
echo ""

# 1. 检查当前 AER二进制是否包含优化
echo "=== 检查二进制优化状态 ==="
AER_BIN="$PROJECT_ROOT/build/src/aer"

if strings "$AER_BIN" | grep -q "simd_utils"; then
    echo "✅ 当前版本包含 SIMD 优化"
    OPTIMIZATION_STATUS="enabled"
else
    echo "⚠️  当前版本未检测到 SIMD 优化"
    OPTIMIZATION_STATUS="disabled"
fi

if strings "$AER_BIN" | grep -q "OptimizedLegIK"; then
    echo "✅ 当前版本包含 OptimizedLegIK"
else
    echo "⚠️  当前版本未检测到 OptimizedLegIK"
fi

if strings "$AER_BIN" | grep -q "FastTrig"; then
    echo "✅ 当前版本包含 FastTrig"
else
    echo "⚠️  当前版本未检测到 FastTrig"
fi

echo ""

# 2. 清理之前的 AER进程
echo "=== 清理之前的 AER进程 ==="
pkill -9 -f "build/src/aer" 2>/dev/null || true
sleep 1
echo ""

# 3. 运行 AER并收集性能数据
echo "=== 运行 AER并收集性能数据 ==="

cd "$PROJECT_ROOT"
source resource/scripts/setup.bash > /dev/null 2>&1

export AER_MODE=humanoid
export AER_LOG_LEVEL=2
export AER_MAX_CYCLES=0

# 使用 time 命令收集基本性能指标
echo "启动 AER(时长: ${DURATION}s)..."
{
    time timeout "$DURATION" "$AER_BIN" 2>&1 | tee "$RESULTS_DIR/aer_runtime.log" || true
} 2> "$RESULTS_DIR/time_output.txt" &

AER_PID=$!
echo "DCP PID: $AER_PID"

# 使用 perf 收集 CPU 性能数据 (如果可用)
if command -v perf &> /dev/null; then
    echo "使用 perf 收集性能数据..."
    sleep 2  # 等待 AER启动
    perf stat -p "$AER_PID" -o "$RESULTS_DIR/perf_stat.txt" &
    PERF_PID=$!

    # 收集 CPU 周期数据
    perf record -p "$AER_PID" -o "$RESULTS_DIR/perf.data" sleep "$((DURATION - 5))" 2>/dev/null || true

    wait "$PERF_PID" 2>/dev/null || true
fi

# 使用 ps 定期采集 CPU/内存 数据
echo "采集 CPU/内存 数据..."
for i in $(seq 1 "$DURATION"); do
    if kill -0 "$AER_PID" 2>/dev/null; then
        ps -p "$AER_PID" -o pid,pcpu,pmem,rss,vsz,comm >> "$RESULTS_DIR/ps_samples.txt" 2>/dev/null || true
    else
        break
    fi
    sleep 1
done

# 等待 AER结束
wait "$AER_PID" 2>/dev/null || true

echo ""
echo "=== 测试完成 ==="
echo ""

# 4. 分析结果
echo "=== 性能分析结果 ==="

# 处理 ps 数据
if [ -f "$RESULTS_DIR/ps_samples.txt" ]; then
    echo "CPU/内存 使用统计:"
    awk 'NR>1 {cpu+=$3; mem+=$4; rss+=$5; count++} END {
        if (count > 0) {
            printf "  平均 CPU: %.1f%%\n", cpu/count
            printf "  平均 MEM: %.1f%%\n", mem/count
            printf "  平均 RSS: %.1f MB\n", rss/count/1024
            printf "  采样次数: %d\n", count
        }
    }' "$RESULTS_DIR/ps_samples.txt"
fi

# 处理 time 输出
if [ -f "$RESULTS_DIR/time_output.txt" ]; then
    echo ""
    echo "运行时间:"
    grep -E "real|user|sys" "$RESULTS_DIR/time_output.txt" || cat "$RESULTS_DIR/time_output.txt"
fi

# 处理 perf 数据
if [ -f "$RESULTS_DIR/perf_stat.txt" ]; then
    echo ""
    echo "CPU 性能计数器:"
    head -20 "$RESULTS_DIR/perf_stat.txt"
fi

# 统计日志中的关键信息
if [ -f "$RESULTS_DIR/aer_runtime.log" ]; then
    echo ""
    echo "DCP 运行统计:"

    # 计算路点数量
    WAYPOINTS=$(grep -c "waypoint" "$RESULTS_DIR/aer_runtime.log" || echo 0)
    echo "  处理路点数: $WAYPOINTS"

    # 计算触发次数
    TRIGGERS=$(grep -c "Triggered" "$RESULTS_DIR/aer_runtime.log" || echo 0)
    echo "  触发次数: $TRIGGERS"

    # 查找错误
    ERRORS=$(grep -c "ERROR" "$RESULTS_DIR/aer_runtime.log" || echo 0)
    echo "  错误数量: $ERRORS"
fi

echo ""
echo "详细日志: $RESULTS_DIR/aer_runtime.log"
echo "性能数据: $RESULTS_DIR/"
