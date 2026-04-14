#ifndef __AXP2202_H__
#define __AXP2202_H__

/* define AXP21 REGISTER */
#define   AXP2202_POWEROFF			(1 << 0)

#define   AXP2202_COMM_STATUS0			(0x00)
#define   AXP2202_MODE_CHGSTATUS		(0x01)
#define   AXP2202_IIN_LIM			(0x17)
#define   AXP2202_PWRON_STATUS			(0x20)
#define   AXP2202_PWROFF_STATUS			(0x21)
#define   AXP2202_OFF_CTL			(0x27)
#define   AXP2202_BTN_CHG_CFG			(0x6A)
#define   AXP2202_DATA_BUFFER3			(0xf0)

#define   AXP2202_PWRON_FLAG_MASK		BIT(0)

#define   AXP2202_BOOT_MODE_CHARGER		BIT(3)
#define   AXP2202_BOOT_MODE_REBOOT		BIT(0)


#endif /* __AXP2202_REGS_H__ */
