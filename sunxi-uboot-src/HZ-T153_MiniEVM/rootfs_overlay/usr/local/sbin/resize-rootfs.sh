#!/bin/bash
set -e

# 从 cmdline 获取 root 分区
cmdline=$(cat /proc/cmdline)
rootdev=$(echo "$cmdline" | sed -n 's/.*root=\([^ ]*\).*/\1/p')

if [ -z "$rootdev" ]; then
    echo "[resize-rootfs] ERROR: root device not found in cmdline"
    exit 1
fi

echo "[resize-rootfs] Resizing $rootdev ..."
resize2fs "$rootdev"

# 完成后禁用自己
systemctl disable resize-rootfs.service
