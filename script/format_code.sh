#!/usr/bin/env bash

set -euo pipefail

usage() {
    cat <<'EOF'
Usage: format_code.sh [--check] <path> [path ...]

Formats C/C++ source files and headers using the repository root .clang-format.
  --check   Run clang-format in dry-run mode and fail on style violations.
EOF
}

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
CLANG_FORMAT_BIN="${CLANG_FORMAT:-clang-format}"
CLANG_FORMAT_STYLE="file:${REPO_ROOT}/.clang-format"

if [[ $# -eq 0 ]]; then
    usage
    exit 1
fi

check_mode=0
paths=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --check)
            check_mode=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        --)
            shift
            while [[ $# -gt 0 ]]; do
                paths+=("$1")
                shift
            done
            ;;
        -*)
            echo "Unknown option: $1" >&2
            usage >&2
            exit 1
            ;;
        *)
            paths+=("$1")
            shift
            ;;
    esac
done

if [[ ${#paths[@]} -eq 0 ]]; then
    echo "No input paths provided." >&2
    usage >&2
    exit 1
fi

if ! command -v "${CLANG_FORMAT_BIN}" >/dev/null 2>&1; then
    echo "clang-format not found: ${CLANG_FORMAT_BIN}" >&2
    exit 1
fi

is_source_file() {
    local file="$1"
    case "${file}" in
        *.c|*.cc|*.cpp|*.cxx|*.h|*.hh|*.hpp|*.hxx|*.inl|*.ipp|*.proto)
            return 0
            ;;
        *)
            return 1
            ;;
    esac
}

collect_files() {
    local path="$1"
    if [[ -f "${path}" ]]; then
        if is_source_file "${path}"; then
            printf '%s\0' "${path}"
        fi
        return 0
    fi

    if [[ -d "${path}" ]]; then
        find "${path}" -type f \( \
            -name '*.c' -o -name '*.cc' -o -name '*.cpp' -o -name '*.cxx' -o \
            -name '*.h' -o -name '*.hh' -o -name '*.hpp' -o -name '*.hxx' -o \
            -name '*.inl' -o -name '*.ipp' -o -name '*.proto' \
        \) -print0
        return 0
    fi

    echo "Path not found: ${path}" >&2
    return 1
}

tmp_list="$(mktemp)"
cleanup() {
    rm -f "${tmp_list}"
}
trap cleanup EXIT

for path in "${paths[@]}"; do
    collect_files "${path}" >>"${tmp_list}"
done

if [[ ! -s "${tmp_list}" ]]; then
    echo "No supported source files found." >&2
    exit 0
fi

if [[ ${check_mode} -eq 1 ]]; then
    xargs -0 "${CLANG_FORMAT_BIN}" -style="${CLANG_FORMAT_STYLE}" --dry-run --Werror <"${tmp_list}"
else
    xargs -0 "${CLANG_FORMAT_BIN}" -style="${CLANG_FORMAT_STYLE}" -i <"${tmp_list}"
fi
