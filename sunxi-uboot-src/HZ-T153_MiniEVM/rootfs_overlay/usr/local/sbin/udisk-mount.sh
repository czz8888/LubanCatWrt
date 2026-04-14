#!/bin/bash
set -e

cmdline=$(cat /proc/cmdline)

# 提取 UDISK 分区设备
udiskdev=$(echo "$cmdline" | sed -n 's/.*UDISK@\([^: ]*\).*/\/dev\/\1/p')

if [ -z "$udiskdev" ]; then
    echo "[udisk] ERROR: UDISK device not found in cmdline"
    exit 1
fi

mountpoint="/mnt/udisk"
mkdir -p "$mountpoint"

echo "[udisk] Checking $udiskdev ..."
if ! fsck -y "$udiskdev"; then
    echo "[udisk] fsck failed, formatting as ext4 ..."
    mkfs.ext4 -F "$udiskdev"
fi

# 挂载
if ! mount | grep -q "$mountpoint"; then
    echo "[udisk] Mounting $udiskdev to $mountpoint"
    mount "$udiskdev" "$mountpoint"
else
    echo "[udisk] Already mounted"
fi

