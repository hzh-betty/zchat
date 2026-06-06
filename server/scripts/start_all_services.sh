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
BUILD_JOBS="${BUILD_JOBS:-2}"
STARTUP_GRACE_SECONDS="${STARTUP_GRACE_SECONDS:-1}"

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

Environment:
  CONAN_HOME      Conan cache directory. Default: build/conan-home.
  BUILD_JOBS      Parallel build jobs. Default: 2.
  STARTUP_GRACE_SECONDS
                  Seconds to wait before verifying a started service. Default: 1.
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

    sleep "${STARTUP_GRACE_SECONDS}"
    local pid
    pid="$(cat "${pid_file}")"
    if ! kill -0 "${pid}" 2>/dev/null; then
        echo "${service} failed to start. Last log lines:" >&2
        tail -40 "${log_file}" >&2 || true
        exit 1
    fi
}

verify_service() {
    local service="$1"
    local log_file="${LOG_DIR}/${service}.log"
    local pid_file="${RUN_DIR}/${service}.pid"

    if [[ ! -f "${pid_file}" ]]; then
        echo "${service} is not running: missing pid file" >&2
        return 1
    fi

    local pid
    pid="$(cat "${pid_file}")"
    if [[ -n "${pid}" ]] && kill -0 "${pid}" 2>/dev/null; then
        return 0
    fi

    rm -f "${pid_file}"
    echo "${service} exited after startup. Last log lines:" >&2
    tail -40 "${log_file}" >&2 || true
    return 1
}

load_conan_run_env() {
    local conan_run_env="${BUILD_DIR}/generators/conanrun.sh"
    if [[ -f "${conan_run_env}" ]]; then
        # shellcheck source=/dev/null
        source "${conan_run_env}"
    fi
}

load_conan_build_env() {
    local conan_build_env="${BUILD_DIR}/generators/conanbuild.sh"
    if [[ -f "${conan_build_env}" ]]; then
        # shellcheck source=/dev/null
        source "${conan_build_env}"
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

: "${CONAN_HOME:=${ROOT_DIR}/build/conan-home}"
export CONAN_HOME

load_conan_run_env

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
            --output-folder="${BUILD_DIR}" \
            --build=missing \
            -c "tools.build:jobs=${BUILD_JOBS}" \
            -pr:h "${CONAN_HOST_PROFILE}" \
            -pr:b "${CONAN_BUILD_PROFILE}"
    fi
    load_conan_build_env
    load_conan_run_env
    echo "Configuring zchat services with preset ${BUILD_PRESET}"
    cmake --preset "${BUILD_PRESET}"
    echo "Building zchat services with preset ${BUILD_PRESET} (${BUILD_JOBS} jobs)"
    cmake --build --preset "${BUILD_PRESET}" --target \
        zchat_file_service \
        zchat_speech_service \
        zchat_transmite_service \
        zchat_message_service \
        zchat_friend_service \
        zchat_user_service \
        zchat_gateway \
        -j"${BUILD_JOBS}"
fi

if [[ "${RESTART_BEFORE_START}" -eq 1 ]]; then
    for ((i=${#SERVICES[@]}-1; i>=0; i--)); do
        stop_service "${SERVICES[$i]}"
    done
fi

for service in "${SERVICES[@]}"; do
    start_service "${service}"
done

startup_failed=0
for service in "${SERVICES[@]}"; do
    if ! verify_service "${service}"; then
        startup_failed=1
    fi
done
if [[ "${startup_failed}" -ne 0 ]]; then
    exit 1
fi

echo
echo "All zchat services are running."
echo "Build preset: ${BUILD_PRESET}"
echo "HTTP gateway: http://127.0.0.1:8000"
echo "Logs: ${LOG_DIR}"
