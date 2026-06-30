#!/bin/bash
#
# ECIDS Core - Service Management Script
# Usage: ./scripts/start.sh {start|stop|restart|status|build}
#

set -e

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
APP_NAME="ecids_core_node"
BUILD_DIR="${PROJECT_DIR}/build"
PID_FILE="${PROJECT_DIR}/${APP_NAME}.pid"
LOG_DIR="${PROJECT_DIR}/logs"

CERT_PATH="${CERT_PATH:-${PROJECT_DIR}/certs/server.crt}"
KEY_PATH="${KEY_PATH:-${PROJECT_DIR}/certs/server.key}"

mkdir -p "${LOG_DIR}"

kill_all_instances() {
    local pids
    pids=$(pgrep -f "${APP_NAME}" 2>/dev/null)
    if [ -n "$pids" ]; then
        echo "[Core] Stopping all instances: $pids"
        for pid in $pids; do
            kill -TERM "$pid" 2>/dev/null || true
        done
        local count=0
        while pgrep -f "${APP_NAME}" > /dev/null 2>&1; do
            sleep 0.5
            count=$((count + 1))
            if [ $count -ge 20 ]; then
                echo "[Core] Force-killing survivors..."
                pids=$(pgrep -f "${APP_NAME}" 2>/dev/null)
                for pid in $pids; do
                    kill -9 "$pid" 2>/dev/null || true
                done
                break
            fi
        done
        echo "[Core] All instances stopped."
    fi
    rm -f "${PID_FILE}"
}

is_running() {
    if [ -f "${PID_FILE}" ]; then
        local pid
        pid=$(cat "${PID_FILE}")
        if kill -0 "${pid}" 2>/dev/null; then
            return 0
        fi
    fi
    return 1
}

do_build() {
    echo "[Core] Building..."
    mkdir -p "${BUILD_DIR}"
    cd "${BUILD_DIR}"
    cmake ..
    make -j$(nproc)
    echo "[Core] Build complete."
}

do_start() {
    # Kill any existing instances first (prevents duplicates from manual starts)
    kill_all_instances
    sleep 0.5

    if [ ! -f "${BUILD_DIR}/${APP_NAME}" ]; then
        do_build
    fi

    echo "[Core] Starting..."
    cd "${PROJECT_DIR}"
    nohup "${BUILD_DIR}/${APP_NAME}" "${PROJECT_DIR}/config" "${CERT_PATH}" "${KEY_PATH}" \
        > "${LOG_DIR}/${APP_NAME}.out" 2> "${LOG_DIR}/${APP_NAME}.err" &
    echo $! > "${PID_FILE}"
    sleep 1

    if is_running; then
        echo "[Core] Started (PID: $(cat "${PID_FILE}"))"
    else
        echo "[Core] Failed to start — check ${LOG_DIR}/${APP_NAME}.err"
        rm -f "${PID_FILE}"
        return 1
    fi
}

do_stop() {
    kill_all_instances
}

do_status() {
    if is_running; then
        echo "[Core] Running (PID: $(cat "${PID_FILE}"))"
    else
        echo "[Core] Not running."
    fi
}

case "${1:-}" in
    start)   do_start ;;
    stop)    do_stop ;;
    restart) do_stop; sleep 1; do_start ;;
    status)  do_status ;;
    build)   do_build ;;
    *)
        echo "Usage: $0 {start|stop|restart|status|build}"
        exit 1
        ;;
esac
