#
# Copyright (C) 2013-2016 OpenWrt.org
# Copyright (C) 2017 Yousong Zhou
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=u-boot
PKG_VERSION:=2023.04-rc4
PKG_RELEASE:=1

PKG_MAINTAINER:=LubanCat

PKG_SOURCE_PROTO:=local
PKG_SOURCE_DIR:=$(TOPDIR)/sunxi-uboot-src/u-boot-2023
PKG_UBOOT_CONFIG:=$(TOPDIR)/sunxi-uboot-src/uboot-config

include $(INCLUDE_DIR)/u-boot.mk
include $(INCLUDE_DIR)/package.mk

define U-Boot/Default
  BUILD_TARGET:=sunxi
  UBOOT_IMAGE:=u-boot-sunxi-with-spl.bin
  UENV:=default
  HIDDEN:=1
  # Board-specific device config directory under sunxi-uboot-src/.
  # Override per target when the board config lives in a subdirectory.
  BOARD_CONFIG_SUBDIR:=
endef

define U-Boot/allwinner_t153-153-demo
  BUILD_SUBTARGET:=cortexa7
  NAME:=Allwinner T153 153-Demo Board (sunxi-main)
  BUILD_DEVICES:=allwinner_t153-153-demo
  UENV:=default
  BOARD_CONFIG_SUBDIR:=HZ-T153_MiniEVM
  UBOOT_IMAGE:=u-boot-sun8iw22p1.bin
endef

UBOOT_TARGETS := allwinner_t153-153-demo

UBOOT_CONFIGURE_VARS += USE_PRIVATE_LIBGCC=yes

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) $(PKG_SOURCE_DIR)/. $(PKG_BUILD_DIR)/
endef

define Build/Configure
	$(CP) $(PKG_UBOOT_CONFIG) $(PKG_BUILD_DIR)/.config
	+$(MAKE) $(PKG_JOBS) -C $(PKG_BUILD_DIR) \
		CROSS_COMPILE=$(TARGET_CROSS) \
		$(UBOOT_CONFIGURE_VARS) \
		olddefconfig
endef

define Build/Compile
	+$(MAKE) $(PKG_JOBS) -C $(PKG_BUILD_DIR) \
		CROSS_COMPILE=$(TARGET_CROSS) \
		LICHEE_BOARD_CONFIG_DIR=$(TOPDIR)/sunxi-uboot-src$(if $(BOARD_CONFIG_SUBDIR),/$(BOARD_CONFIG_SUBDIR)) \
		$(UBOOT_MAKE_FLAGS)
endef

define Build/InstallDev
	$(INSTALL_DIR) $(STAGING_DIR_IMAGE)
	$(CP) $(PKG_BUILD_DIR)/$(UBOOT_IMAGE) $(STAGING_DIR_IMAGE)/$(BUILD_DEVICES)-u-boot-with-spl.bin
	mkimage -C none -A arm -T script -d uEnv-$(UENV).txt \
		$(STAGING_DIR_IMAGE)/$(BUILD_DEVICES)-boot.scr
endef

define Package/u-boot/install/default
endef

$(eval $(call BuildPackage/U-Boot))
