#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SERVER_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
ROOT_DIR="$(cd "${SERVER_DIR}/.." && pwd)"

BUILD_PRESET="conan2-debug"
BUILD_DIR=""
ENV_PATH="${SERVER_DIR}/config/.env"
LOG_DIR="${SERVER_DIR}/logs"
RUN_DIR="${SERVER_DIR}/run"

BUILD_BEFORE_START=0
RESTART_BEFORE_START=0

usage() {
    cat <<'EOF'
Usage: server/scripts/start_all_services.sh [options]

Options:
  --preset <name>  Select build preset. Default: conan2-debug.
                   Supported: conan2-debug, conan2-release.
  --build          Configure and build all zchat services before starting.
  --restart        Stop running zchat services before starting.
  --help           Show this help.

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
        --preset)
            if [[ $# -lt 2 ]]; then
                echo "Missing value for --preset" >&2
                usage >&2
                exit 2
            fi
            BUILD_PRESET="$2"
            shift
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

case "${BUILD_PRESET}" in
    conan2-debug|conan2-release)
        ;;
    *)
        echo "Unsupported preset: ${BUILD_PRESET}" >&2
        usage >&2
        exit 2
        ;;
esac

BUILD_DIR="${ROOT_DIR}/build/${BUILD_PRESET}"

SERVICES=(
    zchat_file_service
    zchat_speech_service
    zchat_transmite_service
    zchat_message_service
    zchat_friend_service
    zchat_user_service
    zchat_gateway
)

declare -A CONFIG_PATHS=(
    [zchat_file_service]="${SERVER_DIR}/config/file.json"
    [zchat_speech_service]="${SERVER_DIR}/config/speech.json"
    [zchat_transmite_service]="${SERVER_DIR}/config/transmite.json"
    [zchat_message_service]="${SERVER_DIR}/config/message.json"
    [zchat_friend_service]="${SERVER_DIR}/config/friend.json"
    [zchat_user_service]="${SERVER_DIR}/config/user.json"
    [zchat_gateway]="${SERVER_DIR}/config/gateway.json"
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
    local config_path="${CONFIG_PATHS[${service}]}"

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

    pkill -f "[/]${service} ${config_path}" 2>/dev/null || true
}

start_service() {
    local service="$1"
    local binary="${BUILD_DIR}/bin/${service}"
    local config_path="${CONFIG_PATHS[${service}]}"
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
        nohup "${binary}" "${config_path}" >"${log_file}" 2>&1 &
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

require_file "${ENV_PATH}"
for service in "${SERVICES[@]}"; do
    require_file "${CONFIG_PATHS[${service}]}"
done

mkdir -p "${LOG_DIR}" "${RUN_DIR}"

set -a
# shellcheck source=/dev/null
source "${ENV_PATH}"
set +a

if [[ "${BUILD_BEFORE_START}" -eq 1 ]]; then
    if [[ "${BUILD_PRESET}" == conan2-* ]]; then
        if [[ "${BUILD_PRESET}" == "conan2-debug" ]]; then
            CONAN_HOST_PROFILE="${ROOT_DIR}/profiles/linux-clang-debug"
        else
            CONAN_HOST_PROFILE="${ROOT_DIR}/profiles/linux-clang-release"
        fi
        CONAN_BUILD_PROFILE="${ROOT_DIR}/profiles/linux-clang-release"
        echo "Installing Conan dependencies for ${BUILD_PRESET}"
        conan install "${ROOT_DIR}" \
            --build=missing \
            -pr:h "${CONAN_HOST_PROFILE}" \
            -pr:b "${CONAN_BUILD_PROFILE}"
    fi
    echo "Configuring zchat services with preset ${BUILD_PRESET}"
    cmake --preset "${BUILD_PRESET}"
    echo "Building zchat services with preset ${BUILD_PRESET}"
    cmake --build --preset "${BUILD_PRESET}" --target \
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
echo "Build preset: ${BUILD_PRESET}"
echo "HTTP gateway: http://127.0.0.1:8000"
echo "Logs: ${LOG_DIR}"
