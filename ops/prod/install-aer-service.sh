#!/bin/bash
#
# install-aer-service.sh
# 安装 AER 系统服务
# 使用方法: sudo ./install-aer-service.sh [install|uninstall|status|enable|disable|start|stop|restart]
#

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 项目路径
PROJECT_DIR="/home/xucong/caicAD/01datainfra/Aurora/aurora-edge-runtime"
SERVICE_NAME="aer"
SERVICE_FILE="${SERVICE_NAME}.service"
SYSTEM_DIR="/etc/systemd/system"
PROJECT_SERVICE_FILE="${PROJECT_DIR}/ops/prod/${SERVICE_FILE}"
CONFIG_FILE="${PROJECT_DIR}/ops/aer.conf"

# 可执行文件路径
EXECUTABLE="${PROJECT_DIR}/build/src/aer"

# 日志目录
LOG_DIR="/var/log/aer"

# PID 文件
PID_FILE="/var/run/${SERVICE_NAME}.pid"

print_usage() {
    echo -e "${BLUE}用法:${NC} $0 [命令]"
    echo ""
    echo "命令:"
    echo "  install    - 安装 systemd 服务"
    echo "  uninstall  - 卸载 systemd 服务"
    echo "  status     - 查看服务状态"
    echo "  enable     - 开机自启动"
    echo "  disable    - 取消开机自启动"
    echo "  start      - 启动服务"
    echo "  stop       - 停止服务"
    echo "  restart    - 重启服务"
    echo "  log        - 查看服务日志"
    echo "  reload     - 重新加载服务配置"
    exit 0
}

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

check_root() {
    if [[ $EUID -ne 0 ]]; then
        log_error "此操作需要 root 权限，请使用 sudo 运行"
        exit 1
    fi
}

check_executable() {
    if [[ ! -f "$EXECUTABLE" ]]; then
        log_error "可执行文件不存在: $EXECUTABLE"
        log_info "请先运行: cd ${PROJECT_DIR} && mkdir -p build && cd build && cmake .. && make -j8"
        exit 1
    fi
}

create_log_dir() {
    if [[ ! -d "$LOG_DIR" ]]; then
        sudo mkdir -p "$LOG_DIR"
        sudo chown $USER:$USER "$LOG_DIR"
        log_info "创建日志目录: $LOG_DIR"
    fi
}

install_service() {
    log_info "安装 ${SERVICE_NAME} 系统服务..."

    check_root
    check_executable

    # 创建日志目录
    create_log_dir

    # 复制服务文件
    sudo cp "$PROJECT_SERVICE_FILE" "${SYSTEM_DIR}/${SERVICE_FILE}"
    log_info "安装服务文件: ${SYSTEM_DIR}/${SERVICE_FILE}"

    # 重新加载 systemd
    sudo systemctl daemon-reload
    log_info "重新加载 systemd 配置"

    # 启用服务（不开机自启）
    sudo systemctl enable ${SERVICE_NAME}
    log_info "启用服务 ${SERVICE_NAME}"

    echo ""
    log_info "✅ 服务安装完成!"
    echo ""
    echo -e "${BLUE}常用命令:${NC}"
    echo "  启动服务:   sudo systemctl start ${SERVICE_NAME}"
    echo "  停止服务:   sudo systemctl stop ${SERVICE_NAME}"
    echo "  查看状态:   sudo systemctl status ${SERVICE_NAME}"
    echo "  查看日志:   sudo journalctl -u ${SERVICE_NAME} -f"
    echo "  开机自启:   sudo systemctl enable ${SERVICE_NAME}"
    echo "  取消自启:   sudo systemctl disable ${SERVICE_NAME}"
    echo ""
    echo -e "${YELLOW}或者使用 aer 命令:${NC}"
    echo "  aer status"
    echo "  aer logs"
}

uninstall_service() {
    log_info "卸载 ${SERVICE_NAME} 系统服务..."

    check_root

    # 停止服务
    sudo systemctl stop ${SERVICE_NAME} 2>/dev/null || true
    log_info "停止服务"

    # 禁用服务
    sudo systemctl disable ${SERVICE_NAME} 2>/dev/null || true
    log_info "禁用服务"

    # 删除服务文件
    sudo rm -f "${SYSTEM_DIR}/${SERVICE_FILE}"
    log_info "删除服务文件"

    # 重新加载 systemd
    sudo systemctl daemon-reload
    log_info "重新加载 systemd 配置"

    # 重启失败的服务
    sudo systemctl reset-failed 2>/dev/null || true

    echo ""
    log_info "✅ 服务卸载完成!"
}

show_status() {
    sudo systemctl status ${SERVICE_NAME}
}

enable_service() {
    check_root
    log_info "启用开机自启动..."
    sudo systemctl enable ${SERVICE_NAME}
    log_info "✅ 服务已设置为开机自启动"
}

disable_service() {
    check_root
    log_info "取消开机自启动..."
    sudo systemctl disable ${SERVICE_NAME}
    log_info "✅ 已取消开机自启"
}

start_service() {
    log_info "启动 ${SERVICE_NAME} 服务..."
    sudo systemctl start ${SERVICE_NAME}
    log_info "✅ 服务已启动"
}

stop_service() {
    log_info "停止 ${SERVICE_NAME} 服务..."
    sudo systemctl stop ${SERVICE_NAME}
    log_info "✅ 服务已停止"
}

restart_service() {
    log_info "重启 ${SERVICE_NAME} 服务..."
    sudo systemctl restart ${SERVICE_NAME}
    log_info "✅ 服务已重启"
}

show_log() {
    log_info "显示 ${SERVICE_NAME} 服务日志 (Ctrl+C 退出)..."
    sudo journalctl -u ${SERVICE_NAME} -f
}

reload_service() {
    log_info "重新加载服务配置..."
    check_root
    sudo systemctl daemon-reload
    sudo systemctl restart ${SERVICE_NAME}
    log_info "✅ 服务配置已重新加载"
}

# 主程序
case "${1:-}" in
    install)
        install_service
        ;;
    uninstall)
        uninstall_service
        ;;
    status)
        show_status
        ;;
    enable)
        enable_service
        ;;
    disable)
        disable_service
        ;;
    start)
        start_service
        ;;
    stop)
        stop_service
        ;;
    restart)
        restart_service
        ;;
    log)
        show_log
        ;;
    reload)
        reload_service
        ;;
    *)
        print_usage
        ;;
esac

exit 0
