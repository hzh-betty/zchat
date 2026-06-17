#!/usr/bin/env bash
set -e

# =========================
# 可配置区域
# =========================

# 需要格式化的文件后缀
EXTENSIONS=(
  "cpp"
  "cc"
  "cxx"
  "h"
  "hpp"
)

# 忽略目录
IGNORE_DIRS=(
  "build"
  ".git"
  "third_party"
  "external"
)

# 忽略路径条件，支持正则
# 例如：
#   "^generated/"
#   ".*\.pb\.h$"
#   ".*\.pb\.cc$"
IGNORE_PATTERNS=(
  "^generated/"
  ".*\.pb\.h$"
  ".*\.pb\.cc$"
)

CLANG_FORMAT=${CLANG_FORMAT:-clang-format}

# =========================
# 检查 clang-format
# =========================

if ! command -v "$CLANG_FORMAT" >/dev/null 2>&1; then
  echo "Error: clang-format not found."
  echo "Please install clang-format or set CLANG_FORMAT=/path/to/clang-format"
  exit 1
fi

# =========================
# 工具函数
# =========================

has_valid_extension() {
  local file="$1"
  local ext="${file##*.}"

  for allowed in "${EXTENSIONS[@]}"; do
    if [[ "$ext" == "$allowed" ]]; then
      return 0
    fi
  done

  return 1
}

is_ignored_dir() {
  local file="$1"

  for dir in "${IGNORE_DIRS[@]}"; do
    if [[ "$file" == "$dir/"* || "$file" == */"$dir/"* ]]; then
      return 0
    fi
  done

  return 1
}

is_ignored_pattern() {
  local file="$1"

  for pattern in "${IGNORE_PATTERNS[@]}"; do
    if [[ "$file" =~ $pattern ]]; then
      return 0
    fi
  done

  return 1
}

# =========================
# 获取 staged 文件
# =========================

files=()

while IFS= read -r file; do
  [[ -f "$file" ]] || continue

  if ! has_valid_extension "$file"; then
    continue
  fi

  if is_ignored_dir "$file"; then
    continue
  fi

  if is_ignored_pattern "$file"; then
    continue
  fi

  files+=("$file")
done < <(git diff --cached --name-only --diff-filter=ACMR)

if [[ ${#files[@]} -eq 0 ]]; then
  echo "pre-commit: no staged C/C++ files to format."
  exit 0
fi

echo "Running clang-format on staged C/C++ files..."

for file in "${files[@]}"; do
  echo "  formatting $file"
  "$CLANG_FORMAT" -i "$file"
  git add "$file"
done

echo "clang-format done."

exit 0
