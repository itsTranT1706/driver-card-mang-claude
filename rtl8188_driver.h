#ifndef RTL8188_DRIVER_H
#define RTL8188_DRIVER_H

#include <linux/types.h>
#include <linux/usb.h>
#include <linux/mutex.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <net/cfg80211.h>

/*
 * Thiet bi context dung chung giua cac file.
 */
struct rtl8188_dev {
	struct usb_device *udev;
	struct usb_interface *intf;
	struct mutex io_mutex;
	struct net_device *netdev;
	struct wiphy *wiphy;
};

/* USB register I/O qua endpoint control (EP0) */
int rtl8188_read8(struct rtl8188_dev *rdev, u16 addr, u8 *val);
int rtl8188_read16(struct rtl8188_dev *rdev, u16 addr, u16 *val);
int rtl8188_read32(struct rtl8188_dev *rdev, u16 addr, u32 *val);

int rtl8188_write8(struct rtl8188_dev *rdev, u16 addr, u8 val);
int rtl8188_write16(struct rtl8188_dev *rdev, u16 addr, u16 val);
int rtl8188_write32(struct rtl8188_dev *rdev, u16 addr, u32 val);
int rtl8188_write_block(struct rtl8188_dev *rdev, u16 addr, const u8 *buf, u16 len);

/* Firmware */
int rtl8188_load_firmware(struct rtl8188_dev *rdev);

#endif
