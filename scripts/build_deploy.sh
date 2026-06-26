#!/usr/bin/env bash
# EdgeGuard 一键编译 + 部署到共享文件夹
# 用法: bash scripts/build_deploy.sh [target ...]
#
#   无参数      → 编译全部 7 个目标
#   hubd        → sensor_hubd
#   httpd       → edgeguard_httpd
#   mqttd       → edgeguard_mqttd
#   visiond     → edgeguard_visiond + edgeguard_visiond_face + face_register
#   ui          → edgeguard-ui
#
#   组合:  bash scripts/build_deploy.sh httpd ui
#
# 前提: 在 Ubuntu 18.04 虚拟机中运行
# make 自带增量编译 — 源码未改动的目标自动跳过
set -euo pipefail

# ---- 配置 ----
NCNN_DIR="${NCNN_DIR:-/home/rickyduran/ncnn}"
JPEG_DIR="${JPEG_DIR:-/opt/libjpeg-arm}"
QMAKE="${QMAKE:-/opt/qt-everywhere-src-5.11.3/bin/qmake}"
SHARED_DIR="${SHARED_DIR:-/home/rickyduran/sharedir}"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
APP_DIR="$PROJECT_ROOT/app"
UI_DIR="$PROJECT_ROOT/ui"

# ---- resolve args → list of tags ----
if [ $# -eq 0 ]; then
    TAGS="hubd httpd mqttd visiond ui"
else
    TAGS=""
    for arg in "$@"; do
        case "$arg" in
            hubd|httpd|mqttd|visiond|ui) TAGS="$TAGS $arg" ;;
            *) echo "WARNING: unknown target '$arg' — skipped" ;;
        esac
    done
fi

if [ -z "$TAGS" ]; then
    echo "No valid targets specified."
    echo "Usage: bash scripts/build_deploy.sh [hubd|httpd|mqttd|visiond|ui] ..."
    exit 1
fi

echo "============================================"
echo "EdgeGuard Build & Deploy"
echo " Targets: $TAGS"
echo "============================================"
echo "PROJECT:  $PROJECT_ROOT"
echo "NCNN_DIR: $NCNN_DIR"
echo "JPEG_DIR: $JPEG_DIR"
echo "QMAKE:    $QMAKE"
echo "SHARED:   $SHARED_DIR"
echo "============================================"

mkdir -p "$SHARED_DIR"

# ---- helper: make + copy ----
# Usage: build_step <label> <make-target> <binary-name> [make-args...]
build_step() {
    local label="$1"
    local target="$2"
    local binary="$3"
    shift 3
    echo ""
    echo "--- $label ---"
    cd "$APP_DIR"
    make "$target" "$@"
    cp -v "$APP_DIR/$binary" "$SHARED_DIR/"
}

# ---- hubd ----
if echo "$TAGS" | grep -qw "hubd"; then
    build_step "[hubd] sensor_hubd" sensor_hubd sensor_hubd
fi

# ---- httpd ----
if echo "$TAGS" | grep -qw "httpd"; then
    build_step "[httpd] edgeguard_httpd" edgeguard_httpd edgeguard_httpd
fi

# ---- mqttd ----
if echo "$TAGS" | grep -qw "mqttd"; then
    build_step "[mqttd] edgeguard_mqttd" edgeguard_mqttd edgeguard_mqttd
fi

# ---- visiond (stub + ncnn + face_register) ----
if echo "$TAGS" | grep -qw "visiond"; then
    build_step "[visiond] edgeguard_visiond (stub)" edgeguard_visiond edgeguard_visiond
    build_step "[visiond] edgeguard_visiond_face (ncnn)" edgeguard_visiond_face edgeguard_visiond_face \
        NCNN_DIR="$NCNN_DIR" JPEG_DIR="$JPEG_DIR"
    build_step "[visiond] face_register" face_register face_register \
        NCNN_DIR="$NCNN_DIR" JPEG_DIR="$JPEG_DIR"
fi

# ---- ui ----
if echo "$TAGS" | grep -qw "ui"; then
    echo ""
    echo "--- [ui] edgeguard-ui ---"
    cd "$UI_DIR"
    if [ ! -f Makefile ] || [ EdgeGuardUI.pro -nt Makefile ]; then
        echo "[qmake] generating Makefile..."
        "$QMAKE" EdgeGuardUI.pro
    fi
    make
    cp -v "$UI_DIR/edgeguard-ui" "$SHARED_DIR/"
fi

# ---- summary ----
echo ""
echo "============================================"
echo "完成! 共享文件夹内容:"
echo "============================================"
ls -lh "$SHARED_DIR/"
echo ""
echo "下一步 — 在开发板上执行:"
echo "  sudo sh /imx6ull/scripts/deploy.sh $*"
