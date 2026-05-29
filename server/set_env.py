#!/usr/bin/env python3
"""zchat 服务器环境变量管理脚本

用法:
    # 设置环境变量 (从 .env 文件读取)
    python set_env.py

    # 指定 .env 文件路径
    python set_env.py /path/to/.env

    # 导出当前环境变量为 .env 文件
    python set_env.py --export

    # 清除 zchat 相关环境变量
    python set_env.py --unset

    # 查看当前 zchat 环境变量
    python set_env.py --show
"""

import argparse
import os
import sys

ENV_KEYS = [
    "ZCHAT_ADVERTISE_HOST",
    "ZCHAT_MYSQL_USER",
    "ZCHAT_MYSQL_PASSWORD",
    "ZCHAT_REDIS_PASSWORD",
    "ZCHAT_ETCD_USERNAME",
    "ZCHAT_ETCD_PASSWORD",
    "ZCHAT_SPEECH_APP_ID",
    "ZCHAT_SPEECH_API_KEY",
    "ZCHAT_SPEECH_SECRET_KEY",
    "ZCHAT_ELASTICSEARCH_USER",
    "ZCHAT_ELASTICSEARCH_PASSWORD",
    "ZCHAT_RABBITMQ_USER",
    "ZCHAT_RABBITMQ_PASSWORD",
    "ZCHAT_SMS_ACCESS_KEY_ID",
    "ZCHAT_SMS_ACCESS_KEY_SECRET",
    "ZCHAT_SMS_SIGN_NAME",
    "ZCHAT_SMS_TEMPLATE_CODE",
]

SHELL_DETECT = {
    "bash": ("export {key}='{value}'", "~/.bashrc"),
    "zsh": ("export {key}='{value}'", "~/.zshrc"),
    "fish": ("set -gx {key} '{value}'", "~/.config/fish/config.fish"),
}


def parse_env_file(path: str) -> dict[str, str]:
    env_vars = {}
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            if "=" not in line:
                continue
            key, _, value = line.partition("=")
            key = key.strip()
            value = value.strip().strip("'\"")
            if key in ENV_KEYS:
                env_vars[key] = value
    return env_vars


def detect_shell() -> tuple[str, str]:
    shell_path = os.environ.get("SHELL", "/bin/bash")
    shell_name = os.path.basename(shell_path)
    if shell_name not in SHELL_DETECT:
        shell_name = "bash"
    fmt, rc_file = SHELL_DETECT[shell_name]
    rc_path = os.path.expanduser(rc_file)
    return shell_name, fmt, rc_path


def write_rc(env_vars: dict[str, str], rc_path: str, fmt: str) -> None:
    start_marker = "# >>> zchat env >>>"
    end_marker = "# <<< zchat env <<<"

    content = ""
    if os.path.exists(rc_path):
        with open(rc_path, encoding="utf-8") as f:
            content = f.read()

    if start_marker in content:
        before, _, rest = content.partition(start_marker)
        _, _, after = rest.partition(end_marker)
        content = before + after

    block_lines = [f"\n{start_marker}"]
    for key in ENV_KEYS:
        if key in env_vars:
            block_lines.append(fmt.format(key=key, value=env_vars[key]))
    block_lines.append(end_marker + "\n")

    new_content = content.rstrip("\n") + "\n".join(block_lines)

    with open(rc_path, "w", encoding="utf-8") as f:
        f.write(new_content)

    print(f"已写入 {rc_path}")


def cmd_set(env_file: str) -> None:
    if not os.path.exists(env_file):
        print(f"错误: 找不到 {env_file}", file=sys.stderr)
        print("提示: 复制 .env.example 为 .env 并填入真实值", file=sys.stderr)
        sys.exit(1)

    env_vars = parse_env_file(env_file)
    if not env_vars:
        print(f"警告: {env_file} 中没有找到有效的环境变量")
        sys.exit(1)

    shell_name, fmt, rc_path = detect_shell()
    write_rc(env_vars, rc_path, fmt)
    for key, value in env_vars.items():
        os.environ[key] = value
    print(f"已设置 {len(env_vars)} 个环境变量 ({shell_name})")
    print("请执行以下命令使其生效:")
    print(f"  source {rc_path}")


def cmd_export(env_file: str) -> None:
    env_vars = {}
    for key in ENV_KEYS:
        value = os.environ.get(key, "")
        env_vars[key] = value

    with open(env_file, "w", encoding="utf-8") as f:
        f.write("# zchat 服务器环境变量\n")
        f.write("# 由 set_env.py --export 自动生成\n\n")
        for key in ENV_KEYS:
            f.write(f"{key}={env_vars[key]}\n")

    print(f"已导出环境变量到 {env_file}")


def cmd_unset() -> None:
    start_marker = "# >>> zchat env >>>"
    end_marker = "# <<< zchat env <<<"

    _, _, rc_path = detect_shell()

    if os.path.exists(rc_path):
        with open(rc_path, encoding="utf-8") as f:
            content = f.read()

        if start_marker in content:
            before, _, rest = content.partition(start_marker)
            _, _, after = rest.partition(end_marker)
            content = before + after

            with open(rc_path, "w", encoding="utf-8") as f:
                f.write(content)
            print(f"已从 {rc_path} 移除 zchat 环境变量块")

    for key in ENV_KEYS:
        if key in os.environ:
            del os.environ[key]

    print("已清除当前进程的 zchat 环境变量")
    print("请执行以下命令使其在 shell 中生效:")
    print(f"  source {rc_path}")


def cmd_show() -> None:
    found = False
    for key in ENV_KEYS:
        value = os.environ.get(key)
        if value is not None:
            masked = value[:3] + "***" if len(value) > 3 else "***"
            print(f"  {key}={masked}")
            found = True
    if not found:
        print("  (未设置任何 zchat 环境变量)")
    else:
        print("  提示: 完整值请查看 .env 文件")


def main() -> None:
    parser = argparse.ArgumentParser(description="zchat 环境变量管理工具")
    parser.add_argument("env_file", nargs="?", default="config/.env", help=".env 文件路径 (默认: config/.env)")
    parser.add_argument("--export", action="store_true", help="导出当前环境变量到 .env 文件")
    parser.add_argument("--unset", action="store_true", help="清除 zchat 相关环境变量")
    parser.add_argument("--show", action="store_true", help="查看当前 zchat 环境变量 (脱敏)")
    args = parser.parse_args()

    if args.show:
        cmd_show()
    elif args.unset:
        cmd_unset()
    elif args.export:
        cmd_export(args.env_file)
    else:
        cmd_set(args.env_file)


if __name__ == "__main__":
    main()
