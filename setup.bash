#!/usr/bin/env bash
# Aurora Edge Runtime Environment Setup

_aer_reset_env() {
    unset AER_MODEL_PATH AER_LOG_PATH AER_CSV_LOG_PATH
}

_aer_reset_env

# Environment variables
export PROJECT="Aurora Edge Runtime"
export VIN="VIN123456789"
export INSTALL_ROOT_PATH="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export RobID="CliBot-X2"
export LD_LIBRARY_PATH="${INSTALL_ROOT_PATH}/lib:${INSTALL_ROOT_PATH}/build/src:${LD_LIBRARY_PATH}"
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp
export AER_MODE="${AER_MODE:-humanoid}"  # 运行模式: auto | humanoid

# AER Configuration Paths
export AER_MODEL_PATH="${AER_MODEL_PATH:-/home/xucong/caicAD/01datainfra/Aurora/rl_planning_train/models/nav_data.onnx}"
export AER_CONFIG_PATH="${AER_CONFIG_PATH:-${INSTALL_ROOT_PATH}/config/planner_weights.yaml}"
export AER_LOG_PATH="${AER_LOG_PATH:-/tmp/aer.log}"
export AER_CSV_LOG_PATH="${AER_CSV_LOG_PATH:-/tmp/aer.csv}"
