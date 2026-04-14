#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
#
# gen_allwinner_fex_img.sh - Assemble an Allwinner FEX flash-package staging
# directory for use with PhoenixSuit / LiveSuit or the Allwinner 'pack' tool.
#
# Usage:
#   gen_allwinner_fex_img.sh <staging_dir> <uboot_spl_bin> <uboot_bin>
#                            <kernel_image> <kernel_dtb> <rootfs_image>
#                            <sys_config_fex> <sys_partition_fex> <env_cfg>
#
# Outputs a directory <staging_dir> containing:
#   sys_config.fex        - board configuration (source)
#   sys_partition.fex     - partition layout (source)
#   env.cfg               - u-boot environment source
#   env.fex               - u-boot environment image (512 * REDUNDANT_ENV_SIZE)
#   boot0_sdcard.fex      - SPL (first-stage bootloader)
#   u-boot.fex            - U-Boot binary
#   boot.fex              - kernel + DTB boot image (Android boot image v2)
#   rootfs.fex            - root filesystem image
#   boot-resource.fex     - placeholder boot resource partition
#
# After this script the staging_dir can be fed to the Allwinner 'pack' tool or
# the open-source 'dragoneagle' from sunxi-pack-tools to produce the final
# LiveSuit/PhoenixSuit .img file.

set -e

[ $# -eq 9 ] || {
	echo "SYNTAX: $0 <staging_dir> <boot0_spl.bin> <uboot.bin>" \
	     "<kernel_image> <kernel_dtb> <rootfs_image>" \
	     "<sys_config.fex> <sys_partition.fex> <env.cfg>"
	exit 1
}

STAGING_DIR="$1"
BOOT0_SPL="$2"
UBOOT_BIN="$3"
KERNEL_IMAGE="$4"
KERNEL_DTB="$5"
ROOTFS_IMAGE="$6"
SYS_CONFIG_FEX="$7"
SYS_PARTITION_FEX="$8"
ENV_CFG="$9"

# Default sizes (sectors of 512 bytes)
# REDUNDANT_ENV_SIZE as configured in BoardConfig: 0x20000 = 128 KiB = 256 sectors
ENV_SIZE_SECTORS=256

mkdir -p "$STAGING_DIR"

echo "Copying board config files..."
cp -v "$SYS_CONFIG_FEX"    "$STAGING_DIR/sys_config.fex"
cp -v "$SYS_PARTITION_FEX" "$STAGING_DIR/sys_partition.fex"
cp -v "$ENV_CFG"           "$STAGING_DIR/env.cfg"

echo "Copying bootloader binaries..."
cp -v "$BOOT0_SPL"  "$STAGING_DIR/boot0_sdcard.fex"
cp -v "$UBOOT_BIN"  "$STAGING_DIR/u-boot.fex"

echo "Creating env.fex (${ENV_SIZE_SECTORS} sectors)..."
if command -v mkenvimage >/dev/null 2>&1; then
	mkenvimage -s "$((ENV_SIZE_SECTORS * 512))" \
		-o "$STAGING_DIR/env.fex" "$ENV_CFG"
else
	# Fallback: create a zero-padded file and write text lines
	dd if=/dev/zero bs=512 count="$ENV_SIZE_SECTORS" \
		of="$STAGING_DIR/env.fex" 2>/dev/null
	echo "WARNING: mkenvimage not found; env.fex is zeroed." \
	     "Install u-boot-tools and re-run." >&2
fi

echo "Creating boot.fex (Android boot image v2 with kernel + DTB)..."
if command -v mkbootimg >/dev/null 2>&1; then
	KERNEL_SIZE="$(wc -c < "$KERNEL_IMAGE")"
	# Align DTB offset to next 1 MiB boundary above kernel
	DTB_OFFSET=$(( ( (0x80000 + KERNEL_SIZE + 0x1fffff) / 0x100000 ) * 0x100000 ))
	# Header version 2 (with --dtb/--dtb_offset) requires a newer mkbootimg
	# (AOSP/Android 10+).  Older distro-packaged versions only support v0.
	if mkbootimg --help 2>&1 | grep -q -- '--header_version'; then
		mkbootimg \
			--kernel "$KERNEL_IMAGE" \
			--base 0x40000000 \
			--kernel_offset 0x80000 \
			--dtb "$KERNEL_DTB" \
			--dtb_offset "$DTB_OFFSET" \
			--header_version 2 \
			-o "$STAGING_DIR/boot.fex"
	else
		echo "WARNING: installed mkbootimg does not support --header_version 2;" \
		     "falling back to raw kernel+DTB concatenation for boot.fex." >&2
		cat "$KERNEL_IMAGE" "$KERNEL_DTB" > "$STAGING_DIR/boot.fex"
	fi
else
	echo "WARNING: mkbootimg not found; copying raw kernel as boot.fex." >&2
	cat "$KERNEL_IMAGE" "$KERNEL_DTB" > "$STAGING_DIR/boot.fex"
fi

echo "Copying rootfs..."
cp -v "$ROOTFS_IMAGE" "$STAGING_DIR/rootfs.fex"

echo "Creating placeholder boot-resource.fex..."
dd if=/dev/zero bs=512 count=128 of="$STAGING_DIR/boot-resource.fex" 2>/dev/null

echo ""
echo "FEX staging directory ready: $STAGING_DIR"
echo ""
echo "To create the final LiveSuit/PhoenixSuit image, run the Allwinner 'pack'"
echo "tool (from the Tina SDK) or the open-source 'dragoneagle' utility from"
echo "https://github.com/bradfa/sunxi-pack-tools with:"
echo "  pack -i <chip> -c <soc> -p buildroot -b <board> -k linux -n default"
