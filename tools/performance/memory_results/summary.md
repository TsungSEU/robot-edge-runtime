# 内存分析报告

**生成时间**: 2026年 03月 12日 星期四 17:26:29 CST
**分析目标**: ./build/src/dcp
**分析时长**: 60秒

## 分析文件

### 内存泄漏检测
- `memcheck.log`: Memcheck详细日志
- `leak_summary.txt`: 泄漏摘要

### 堆分析
- `massif.out.*`: Massif原始数据
- `massif_report.txt`: 堆使用报告

### 缓存分析
- `cachegrind.out.*`: Cachegrind原始数据
- `cachegrind_report.txt`: 缓存命中率报告

### 监控数据
- `memory_monitor.log`: 实时内存监控数据
- `memory_chart.txt`: 内存使用趋势图

## 快速查看

```bash
# 查看内存泄漏摘要
cat tools/performance/memory_results/leak_summary.txt

# 查看堆内存峰值
cat tools/performance/memory_results/massif_report.txt

# 使用ms_plot可视化（如果安装）
ms_plot tools/performance/memory_results/massif.out.*
```

## 优化建议

⚠️ 发现内存泄漏，请检查leak_summary.txt
