#!/usr/bin/env bash
set -euo pipefail

# ====== VM Login Info ======
VM_USER="rickyduran"
VM_HOST="192.168.3.222"
VM="${VM_USER}@${VM_HOST}"

# ====== Local Project Info ======
LOCAL_PROJECT="$(cd "$(dirname "$0")/.." && pwd)"

# 本地主机中的 DTS 文件
LOCAL_DTS="${LOCAL_PROJECT}/dts/imx6ull-mmc-npi.dts"

# ====== Remote Project Info ======
REMOTE_PROJECT="/home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL"

# 虚拟机内核源码中的 DTS 文件路径
REMOTE_KERNEL_DTS="/home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/boot/dts/imx6ull-mmc-npi.dts"

REMOTE_KERNEL_DTS_DIR="$(dirname "$REMOTE_KERNEL_DTS")"

echo "=== Local project ==="
echo "$LOCAL_PROJECT"

echo "=== Remote VM ==="
echo "$VM"

echo "=== Remote project ==="
echo "$REMOTE_PROJECT"

echo "=== Step 1: Create remote project directory ==="
ssh "$VM" "mkdir -p '$REMOTE_PROJECT'"

echo "=== Step 2: Sync project by tar + ssh ==="
cd "$LOCAL_PROJECT"

tar \
  --exclude=".git" \
  --exclude="build" \
  --exclude=".vscode" \
  --exclude=".claude" \
  --exclude="node_modules" \
  --exclude="*.o" \
  --exclude="*.ko" \
  --exclude="*.mod" \
  --exclude="*.mod.c" \
  --exclude="*.cmd" \
  --exclude=".tmp_versions" \
  --exclude="Module.symvers" \
  --exclude="modules.order" \
  -czf - . | ssh "$VM" "tar -xzf - -C '$REMOTE_PROJECT'"

echo "=== Step 3: Sync DTS to kernel source ==="

if [ -f "$LOCAL_DTS" ]; then
    echo "Local DTS found:"
    echo "$LOCAL_DTS"

    echo "Creating remote DTS directory if needed..."
    ssh "$VM" "mkdir -p '$REMOTE_KERNEL_DTS_DIR'"

    echo "Backup old remote DTS if it exists..."
    ssh "$VM" "if [ -f '$REMOTE_KERNEL_DTS' ]; then cp '$REMOTE_KERNEL_DTS' '${REMOTE_KERNEL_DTS}.bak_'\$(date +%Y%m%d_%H%M%S); fi"

    echo "Copy local DTS to remote kernel source..."
    scp "$LOCAL_DTS" "$VM:$REMOTE_KERNEL_DTS"

    echo "DTS synced to:"
    echo "$REMOTE_KERNEL_DTS"
else
    echo "WARNING: Local DTS file not found:"
    echo "$LOCAL_DTS"
    echo "Skip DTS sync."
fi

echo "=== Sync finished ==="