#!/usr/bin/env python3
"""Local development helper for zchat services."""

from __future__ import annotations

import argparse
import os
import shlex
import signal
import socket
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Sequence


SUPPORTED_PRESETS = ("conan2-debug", "conan2-release")
DEFAULT_PRESET = "conan2-debug"
DEFAULT_BUILD_JOBS = int(os.environ.get("BUILD_JOBS", "4"))
DEFAULT_STARTUP_GRACE_SECONDS = float(os.environ.get("STARTUP_GRACE_SECONDS", "1"))


@dataclass(frozen=True)
class ProjectPaths:
    scripts_dir: Path
    server_dir: Path
    root_dir: Path
    env_file: Path
    log_dir: Path

    @classmethod
    def from_script(cls) -> "ProjectPaths":
        scripts_dir = Path(__file__).resolve().parent
        server_dir = scripts_dir.parent
        root_dir = server_dir.parent
        return cls(
            scripts_dir=scripts_dir,
            server_dir=server_dir,
            root_dir=root_dir,
            env_file=root_dir / ".env",
            log_dir=server_dir / "logs",
        )

    def build_dir(self, preset: str) -> Path:
        return self.root_dir / "build" / preset


_PATHS: ProjectPaths | None = None


def get_paths() -> ProjectPaths:
    global _PATHS
    if _PATHS is None:
        _PATHS = ProjectPaths.from_script()
    return _PATHS


def set_paths(paths: ProjectPaths | None = None) -> None:
    global _PATHS
    _PATHS = paths


@dataclass(frozen=True)
class Service:
    binary_name: str
    config_name: str
    aliases: tuple[str, ...]
    env_port_key: str = ""

    def config_path(self, paths: ProjectPaths) -> Path:
        return paths.server_dir / "config" / self.config_name


SERVICES = (
    Service("zchat_file_service", "file.json", ("file",), "ZCHAT_FILE_SERVICE_PORT"),
    Service("zchat_speech_service", "speech.json", ("speech",), "ZCHAT_SPEECH_SERVICE_PORT"),
    Service("zchat_transmite_service", "transmite.json", ("transmite", "transfer"), "ZCHAT_TRANSMITE_SERVICE_PORT"),
    Service("zchat_message_service", "message.json", ("message",), "ZCHAT_MESSAGE_SERVICE_PORT"),
    Service("zchat_friend_service", "friend.json", ("friend",), "ZCHAT_FRIEND_SERVICE_PORT"),
    Service("zchat_user_service", "user.json", ("user",), "ZCHAT_USER_SERVICE_PORT"),
    Service("zchat_gateway", "gateway.json", ("gateway",), "ZCHAT_GATEWAY_HTTP_PORT"),
)

SERVICE_BY_NAME = {service.binary_name: service for service in SERVICES}
SERVICE_BY_ALIAS = {
    alias: service
    for service in SERVICES
    for alias in (service.binary_name, service.binary_name.removeprefix("zchat_"), *service.aliases)
}


def run_command(args: Sequence[str], *, env: dict[str, str], cwd: Path | None = None) -> None:
    paths = get_paths()
    subprocess.run(args, cwd=cwd or paths.root_dir, env=env, check=True)


def require_file(path: Path) -> None:
    if not path.is_file():
        raise SystemExit(f"Missing required file: {path}")


def parse_env_file(path: Path) -> dict[str, str]:
    env_vars: dict[str, str] = {}
    with path.open(encoding="utf-8") as f:
        for raw_line in f:
            line = raw_line.strip()
            if not line or line.startswith("#") or "=" not in line:
                continue
            key, value = line.split("=", 1)
            env_vars[key.strip()] = value.strip().strip("'\"")
    return env_vars


def conan_home_env() -> dict[str, str]:
    paths = get_paths()
    env = dict(os.environ)
    env.setdefault("CONAN_HOME", str(paths.root_dir / "build/conan-home"))
    return env


def service_env() -> dict[str, str]:
    paths = get_paths()
    require_file(paths.env_file)
    env = conan_home_env()
    env.update(parse_env_file(paths.env_file))
    return env


def source_env_script(script: Path, base_env: dict[str, str]) -> dict[str, str]:
    if not script.is_file():
        return dict(base_env)

    paths = get_paths()
    command = f"set -a; source {shlex.quote(str(script))}; env -0"
    try:
        result = subprocess.run(
            ["bash", "-lc", command],
            cwd=paths.root_dir,
            env=base_env,
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
    except subprocess.CalledProcessError as exc:
        stderr_output = exc.stderr.decode(errors="replace") if exc.stderr else ""
        print(f"Failed to source {script}:", file=sys.stderr)
        if stderr_output:
            print(stderr_output, end="", file=sys.stderr)
        raise

    env: dict[str, str] = {}
    for entry in result.stdout.split(b"\0"):
        if entry and b"=" in entry:
            key, value = entry.split(b"=", 1)
            env[key.decode()] = value.decode()
    return env


def conan_profiles(preset: str) -> tuple[Path, Path]:
    paths = get_paths()
    host_profile_name = "linux-clang-debug" if preset == "conan2-debug" else "linux-clang-release"
    return paths.root_dir / "profiles" / host_profile_name, paths.root_dir / "profiles/linux-clang-release"


def install_conan(preset: str, jobs: int, env: dict[str, str]) -> None:
    paths = get_paths()
    host_profile, build_profile = conan_profiles(preset)
    print(f"Installing Conan dependencies for {preset}")
    run_command(
        [
            "conan",
            "install",
            str(paths.root_dir),
            f"--output-folder={paths.build_dir(preset)}",
            "--build=missing",
            "-c",
            f"tools.build:jobs={jobs}",
            "-pr:h",
            str(host_profile),
            "-pr:b",
            str(build_profile),
        ],
        env=env,
    )


def conan_generators_exist(preset: str) -> bool:
    paths = get_paths()
    return (paths.build_dir(preset) / "generators").is_dir()


def build_services(preset: str, jobs: int, env: dict[str, str], services: Sequence[Service]) -> dict[str, str]:
    paths = get_paths()
    build_dir = paths.build_dir(preset)
    build_env = dict(env)
    if len(services) == len(SERVICES):
        build_env["ZCHAT_BUILD_TARGETS"] = ""
    else:
        build_env["ZCHAT_BUILD_TARGETS"] = " ".join(s.binary_name for s in services)

    print(f"Building via conan build with preset {preset} ({jobs} jobs)")
    host_profile, build_profile = conan_profiles(preset)
    run_command(
        [
            "conan",
            "build",
            str(paths.root_dir),
            f"--output-folder={build_dir}",
            "-pr:h",
            str(host_profile),
            "-pr:b",
            str(build_profile),
            "-c",
            f"tools.build:jobs={jobs}",
        ],
        env=build_env,
    )
    run_env = source_env_script(build_dir / "generators/conanrun.sh", build_env)
    return run_env


def resolve_service(name: str) -> Service:
    service = SERVICE_BY_ALIAS.get(name)
    if service is None:
        valid_names = ", ".join(sorted(SERVICE_BY_ALIAS))
        raise SystemExit(f"Unknown service: {name}\nValid service names: {valid_names}")
    return service


def selected_services(name: str | None, *, reverse: bool = False) -> list[Service]:
    if not name or name == "all":
        services = list(SERVICES)
    else:
        services = [resolve_service(name)]
    if reverse:
        services.reverse()
    return services


def pid_is_running(pid: int) -> bool:
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return False
    except PermissionError:
        return True
    return True


def print_last_log_lines(log_file: Path, limit: int = 40) -> None:
    if not log_file.is_file():
        return
    result = subprocess.run(
        ["tail", "-n", str(limit), str(log_file)],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
    )
    if result.stdout:
        print(result.stdout, end="", file=sys.stderr)


def port_is_listening(port: int, *, host: str = "127.0.0.1", timeout: float = 0.5) -> bool:
    try:
        with socket.create_connection((host, port), timeout=timeout):
            return True
    except OSError:
        return False


class LocalServiceManager:
    def __init__(self, *, paths: ProjectPaths, preset: str, env: dict[str, str]) -> None:
        self.paths = paths
        self.preset = preset
        self.env = env

    def log_file(self, service: Service) -> Path:
        return self.paths.log_dir / f"{service.binary_name}.log"

    def binary_path(self, service: Service) -> Path:
        return self.paths.build_dir(self.preset) / "bin" / service.binary_name

    def process_pattern(self, service: Service) -> str:
        return f"{self.binary_path(service)} {service.config_path(self.paths)}"

    def find_pids(self, service: Service) -> list[int]:
        result = subprocess.run(
            ["pgrep", "-f", "--", self.process_pattern(service)],
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
        )
        pids: list[int] = []
        for line in result.stdout.splitlines():
            try:
                pid = int(line)
            except ValueError:
                continue
            if pid_is_running(pid):
                pids.append(pid)
        return pids

    def stop(self, service: Service) -> None:
        for pid in self.find_pids(service):
            print(f"Stopping {service.binary_name} (pid {pid})")
            self._terminate_pid(pid, service.binary_name)

    def launch(self, service: Service) -> subprocess.Popen | None:
        binary = self.binary_path(service)
        config = service.config_path(self.paths)
        log_file = self.log_file(service)

        if not os.access(binary, os.X_OK):
            raise SystemExit(f"Missing executable: {binary}\nRun `develop.py start --build`, or build first.")

        running_pids = self.find_pids(service)
        if running_pids:
            print(f"{service.binary_name} is already running (pid {running_pids[0]})")
            return None

        print(f"Starting {service.binary_name}")
        with log_file.open("ab") as log_handle:
            return subprocess.Popen(
                [str(binary), str(config)],
                cwd=self.paths.root_dir,
                env=self.env,
                stdout=log_handle,
                stderr=subprocess.STDOUT,
                start_new_session=True,
            )

    def wait_for_startup(
        self, service: Service, process: subprocess.Popen | None, *, grace_seconds: float
    ) -> None:
        if process is None:
            return

        log_file = self.log_file(service)
        deadline = time.monotonic() + grace_seconds
        while time.monotonic() < deadline:
            if not pid_is_running(process.pid):
                print(f"{service.binary_name} failed to start. Last log lines:", file=sys.stderr)
                print_last_log_lines(log_file)
                raise SystemExit(1)
            time.sleep(0.1)

    def verify(self, service: Service) -> bool:
        if not self.find_pids(service):
            print(f"{service.binary_name} exited after startup. Last log lines:", file=sys.stderr)
            print_last_log_lines(self.log_file(service))
            return False

        if service.env_port_key:
            port_str = self.env.get(service.env_port_key, "")
            if port_str:
                try:
                    port = int(port_str)
                except ValueError:
                    pass
                else:
                    if not port_is_listening(port):
                        print(
                            f"{service.binary_name} is running but port {port} is not listening",
                            file=sys.stderr,
                        )
                        return False

        return True

    @staticmethod
    def _terminate_pid(pid: int, service_name: str) -> None:
        try:
            os.kill(pid, signal.SIGTERM)
        except ProcessLookupError:
            return

        for _ in range(30):
            if not pid_is_running(pid):
                return
            time.sleep(0.1)

        if pid_is_running(pid):
            print(f"Force stopping {service_name} (pid {pid})")
            try:
                os.kill(pid, signal.SIGKILL)
            except ProcessLookupError:
                pass


def validate_service_configs(services: Sequence[Service]) -> None:
    paths = get_paths()
    for service in services:
        require_file(service.config_path(paths))


def cmd_install(args: argparse.Namespace) -> None:
    install_conan(args.preset, args.jobs, conan_home_env())


def cmd_build(args: argparse.Namespace) -> None:
    services = selected_services(args.service)
    env = conan_home_env()
    if not args.force_install and conan_generators_exist(args.preset):
        print("Conan dependencies already installed, skipping install (use --force-install to override)")
    else:
        install_conan(args.preset, args.jobs, env)
    build_services(args.preset, args.jobs, env, services)


def cmd_start(args: argparse.Namespace) -> None:
    paths = get_paths()
    services = selected_services(args.service)
    env = service_env()
    validate_service_configs(services)
    paths.log_dir.mkdir(parents=True, exist_ok=True)

    if args.build:
        if not args.force_install and conan_generators_exist(args.preset):
            print("Conan dependencies already installed, skipping install (use --force-install to override)")
        else:
            install_conan(args.preset, args.jobs, env)
        env = build_services(args.preset, args.jobs, env, services)
    else:
        env = source_env_script(paths.build_dir(args.preset) / "generators/conanrun.sh", env)

    manager = LocalServiceManager(paths=paths, preset=args.preset, env=env)

    if args.restart:
        for service in selected_services(args.service, reverse=True):
            manager.stop(service)

    launched: list[tuple[Service, subprocess.Popen | None]] = []
    for service in services:
        process = manager.launch(service)
        launched.append((service, process))

    deadline = time.monotonic() + args.startup_grace_seconds
    while time.monotonic() < deadline:
        any_exited = any(
            process is not None and process.poll() is not None for _, process in launched
        )
        if any_exited:
            break
        time.sleep(0.1)

    startup_failed = False
    for service, process in launched:
        if process is not None and process.poll() is not None:
            print(f"{service.binary_name} failed to start. Last log lines:", file=sys.stderr)
            print_last_log_lines(manager.log_file(service))
            startup_failed = True
    if startup_failed:
        raise SystemExit(1)

    for service in services:
        if not manager.verify(service):
            startup_failed = True
    if startup_failed:
        raise SystemExit(1)

    print()
    if args.service == "all":
        print("All zchat services are running.")
    else:
        print(f"{services[0].binary_name} is running.")
    print(f"Build preset: {args.preset}")
    print(f"HTTP gateway: http://127.0.0.1:{env.get('ZCHAT_GATEWAY_HTTP_PORT', '8000')}")
    print(f"Logs: {paths.log_dir}")


def cmd_restart(args: argparse.Namespace) -> None:
    args.restart = True
    cmd_start(args)


def cmd_stop(args: argparse.Namespace) -> None:
    paths = get_paths()
    manager = LocalServiceManager(paths=paths, preset=args.preset, env=conan_home_env())
    for service in selected_services(args.service, reverse=True):
        manager.stop(service)


def cmd_test(args: argparse.Namespace) -> None:
    paths = get_paths()
    build_dir = paths.build_dir(args.preset)
    if not (build_dir / "CTestTestfile.cmake").is_file():
        raise SystemExit(f"No build artifacts in {build_dir}, run `develop.py build` first")
    env = source_env_script(build_dir / "generators/conanbuild.sh", conan_home_env())
    print(f"Running tests in {build_dir}")
    run_command(
        ["ctest", f"--test-dir={build_dir}", "--output-on-failure"],
        env=env,
    )


def add_preset_args(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--preset", default=DEFAULT_PRESET, choices=SUPPORTED_PRESETS, help="CMake preset")


def add_build_args(parser: argparse.ArgumentParser) -> None:
    add_preset_args(parser)
    parser.add_argument("--jobs", type=int, default=DEFAULT_BUILD_JOBS, help="Parallel build jobs")
    parser.add_argument("--force-install", action="store_true", help="Force Conan install even if cached")


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    if not argv or (argv[0].startswith("-") and argv[0] not in ("-h", "--help")):
        argv = ("start", *argv)

    parser = argparse.ArgumentParser(description="zchat local development helper")
    subparsers = parser.add_subparsers(dest="command", required=True)

    install_parser = subparsers.add_parser("install", help="Install Conan dependencies only")
    add_build_args(install_parser)
    install_parser.set_defaults(func=cmd_install)

    build_parser = subparsers.add_parser("build", help="Build all local services or one service")
    add_build_args(build_parser)
    build_parser.add_argument("service", nargs="?", default="all", help="Service name or alias, default: all")
    build_parser.set_defaults(func=cmd_build)

    start_parser = subparsers.add_parser("start", help="Start all local services or one service")
    add_build_args(start_parser)
    start_parser.add_argument("service", nargs="?", default="all", help="Service name or alias, default: all")
    start_parser.add_argument("--build", action="store_true", help="Configure and build before starting")
    start_parser.add_argument("--restart", action="store_true", help="Stop running services before starting")
    start_parser.add_argument(
        "--startup-grace-seconds",
        type=float,
        default=DEFAULT_STARTUP_GRACE_SECONDS,
        help="Seconds to wait before checking each process",
    )
    start_parser.set_defaults(func=cmd_start)

    restart_parser = subparsers.add_parser("restart", help="Restart all local services or one service")
    add_build_args(restart_parser)
    restart_parser.add_argument("service", nargs="?", default="all", help="Service name or alias, default: all")
    restart_parser.add_argument("--build", action="store_true", help="Configure and build before restarting")
    restart_parser.add_argument(
        "--startup-grace-seconds",
        type=float,
        default=DEFAULT_STARTUP_GRACE_SECONDS,
        help="Seconds to wait before checking each process",
    )
    restart_parser.set_defaults(func=cmd_restart)

    stop_parser = subparsers.add_parser("stop", help="Stop all local services or one service")
    add_preset_args(stop_parser)
    stop_parser.add_argument("service", nargs="?", default="all", help="Service name or alias, default: all")
    stop_parser.set_defaults(func=cmd_stop)

    test_parser = subparsers.add_parser("test", help="Run CTest in the build folder")
    add_preset_args(test_parser)
    test_parser.set_defaults(func=cmd_test)

    return parser.parse_args(argv)


def main() -> None:
    args = parse_args(sys.argv[1:])
    args.func(args)


if __name__ == "__main__":
    main()