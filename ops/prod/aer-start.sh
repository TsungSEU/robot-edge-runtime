#!/bin/bash
#
# aer-start.sh
# AER (Aurora Edge Runtime) 服务启动脚本
#

set -e

# 项目根目录
PROJECT_DIR="/home/xucong/caicAD/01datainfra/Aurora/aurora-edge-runtime"

# 加载 ROS2 环境 (静默)
if [[ -f "/opt/ros/humble/setup.bash" ]]; then
    source /opt/ros/humble/setup.bash >/dev/null 2>&1
fi

# 加载项目环境变量
export PROJECT="Aurora Edge Runtime"
export VIN="VIN123456789"
export INSTALL_ROOT_PATH=$PROJECT_DIR
export CAR_ID=Google-RT2
export LD_LIBRARY_PATH=$PROJECT_DIR/lib:${LD_LIBRARY_PATH:-}
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp

# 设置 AMENT/COLCON 前缀路径
export AMENT_PREFIX_PATH=/opt/ros/humble${AMENT_PREFIX_PATH:+:${AMENT_PREFIX_PATH}}

# Clean up FastDDS shared memory before starting
if [[ -f "${PROJECT_DIR}/ops/common/cleanup.sh" ]]; then
    bash "${PROJECT_DIR}/ops/common/cleanup.sh"
fi

# 可执行文件路径
EXECUTABLE="${PROJECT_DIR}/build/src/aer"

# 检查可执行文件是否存在
if [[ ! -f "$EXECUTABLE" ]]; then
    echo "Error: AER executable not found: $EXECUTABLE" >&2
    exit 1
fi

# CPU 亲和性配置
CPU_AFFINITY="${AER_CPU_AFFINITY:-}"

# 构建启动命令
CMD=("$EXECUTABLE")

# 添加模式参数 (支持的参数)
if [[ -n "${AER_MODE:-}" ]]; then
    CMD+=("--mode" "${AER_MODE}")
fi

# 添加模型路径参数 (支持的参数)
if [[ -n "${AER_MODEL_PATH:-}" ]]; then
    CMD+=("--model" "${AER_MODEL_PATH}")
fi

# 添加配置路径参数 (支持的参数)
if [[ -n "${AER_CONFIG_PATH:-}" ]]; then
    CMD+=("--config" "${AER_CONFIG_PATH}")
fi

# 根据 CPU 亲和性配置启动
if [[ -n "$CPU_AFFINITY" ]]; then
    exec taskset -c "$CPU_AFFINITY" "${CMD[@]}"
else
    exec "${CMD[@]}"
fi
