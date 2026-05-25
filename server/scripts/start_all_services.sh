#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SERVER_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
ROOT_DIR="$(cd "${SERVER_DIR}/.." && pwd)"

BUILD_DIR="${ROOT_DIR}/build/dev"
CONFIG_PATH="${SERVER_DIR}/config/app.json"
ENV_PATH="${SERVER_DIR}/config/.env"
LOG_DIR="${SERVER_DIR}/logs"
RUN_DIR="${SERVER_DIR}/run"

BUILD_BEFORE_START=0
RESTART_BEFORE_START=0

usage() {
    cat <<'EOF'
Usage: server/scripts/start_all_services.sh [options]

Options:
  --build       Build all zchat services before starting.
  --restart    Stop running zchat services before starting.
  --help       Show this help.

Logs:
  server/logs/<service>.log

PID files:
  server/run/<service>.pid
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --build)
            BUILD_BEFORE_START=1
            ;;
        --restart)
            RESTART_BEFORE_START=1
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
    shift
done

SERVICES=(
    zchat_file_service
    zchat_speech_service
    zchat_transmite_service
    zchat_message_service
    zchat_friend_service
    zchat_user_service
    zchat_gateway
)

require_file() {
    local path="$1"
    if [[ ! -f "${path}" ]]; then
        echo "Missing required file: ${path}" >&2
        exit 1
    fi
}

stop_service() {
    local service="$1"
    local pid_file="${RUN_DIR}/${service}.pid"

    if [[ -f "${pid_file}" ]]; then
        local pid
        pid="$(cat "${pid_file}")"
        if [[ -n "${pid}" ]] && kill -0 "${pid}" 2>/dev/null; then
            echo "Stopping ${service} (pid ${pid})"
            kill "${pid}" 2>/dev/null || true
            for _ in {1..30}; do
                if ! kill -0 "${pid}" 2>/dev/null; then
                    break
                fi
                sleep 0.1
            done
            if kill -0 "${pid}" 2>/dev/null; then
                echo "Force stopping ${service} (pid ${pid})"
                kill -9 "${pid}" 2>/dev/null || true
            fi
        fi
        rm -f "${pid_file}"
    fi

    pkill -f "[/]${service} ${CONFIG_PATH}" 2>/dev/null || true
}

start_service() {
    local service="$1"
    local binary="${BUILD_DIR}/server/${service}"
    local log_file="${LOG_DIR}/${service}.log"
    local pid_file="${RUN_DIR}/${service}.pid"

    if [[ ! -x "${binary}" ]]; then
        echo "Missing executable: ${binary}" >&2
        echo "Run with --build, or build the project first." >&2
        exit 1
    fi

    if [[ -f "${pid_file}" ]]; then
        local old_pid
        old_pid="$(cat "${pid_file}")"
        if [[ -n "${old_pid}" ]] && kill -0 "${old_pid}" 2>/dev/null; then
            echo "${service} is already running (pid ${old_pid})"
            return
        fi
        rm -f "${pid_file}"
    fi

    echo "Starting ${service}"
    (
        cd "${ROOT_DIR}"
        nohup "${binary}" "${CONFIG_PATH}" >"${log_file}" 2>&1 &
        echo $! >"${pid_file}"
    )

    sleep 0.2
    local pid
    pid="$(cat "${pid_file}")"
    if ! kill -0 "${pid}" 2>/dev/null; then
        echo "${service} failed to start. Last log lines:" >&2
        tail -40 "${log_file}" >&2 || true
        exit 1
    fi
}

require_file "${CONFIG_PATH}"
require_file "${ENV_PATH}"

mkdir -p "${LOG_DIR}" "${RUN_DIR}"

set -a
# shellcheck source=/dev/null
source "${ENV_PATH}"
set +a

if [[ "${BUILD_BEFORE_START}" -eq 1 ]]; then
    echo "Building zchat services"
    cmake --build "${BUILD_DIR}" --target \
        zchat_file_service \
        zchat_speech_service \
        zchat_transmite_service \
        zchat_message_service \
        zchat_friend_service \
        zchat_user_service \
        zchat_gateway \
        -j"$(nproc)"
fi

if [[ "${RESTART_BEFORE_START}" -eq 1 ]]; then
    for ((i=${#SERVICES[@]}-1; i>=0; i--)); do
        stop_service "${SERVICES[$i]}"
    done
fi

for service in "${SERVICES[@]}"; do
    start_service "${service}"
done

echo
echo "All zchat services are running."
echo "HTTP gateway: http://127.0.0.1:8000"
echo "Logs: ${LOG_DIR}"
