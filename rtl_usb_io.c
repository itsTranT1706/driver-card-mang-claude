#include <linux/kernel.h>
#include <linux/usb.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include "rtl8188_driver.h"

/* Realtek vendor command tren EP0 control */
#define RTL_USB_VENQT_CMD_REQ		0x05
#define RTL_USB_VENQT_READ		(USB_DIR_IN  | USB_TYPE_VENDOR | USB_RECIP_DEVICE)
#define RTL_USB_VENQT_WRITE		(USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE)
#define RTL_USB_CTRL_TIMEOUT_MS		500
#define RTL_USB_CTRL_RETRIES		5
#define RTL_USB_CTRL_RETRY_DELAY_US	2000

static bool rtl8188_usb_err_retryable(int ret)
{
	return ret == -EAGAIN || ret == -ETIMEDOUT || ret == -EPROTO ||
	       ret == -EILSEQ || ret == -EPIPE;
}

static int rtl8188_ctrl_read(struct rtl8188_dev *rdev, u16 addr, void *buf, u16 len)
{
	int ret;
	int tries;

	mutex_lock(&rdev->io_mutex);
	for (tries = 1; tries <= RTL_USB_CTRL_RETRIES; tries++) {
		ret = usb_control_msg(rdev->udev,
				      usb_rcvctrlpipe(rdev->udev, 0),
				      RTL_USB_VENQT_CMD_REQ,
				      RTL_USB_VENQT_READ,
				      addr,
				      0,
				      buf,
				      len,
				      RTL_USB_CTRL_TIMEOUT_MS);

		if (ret == len)
			break;

		if (ret < 0 && rtl8188_usb_err_retryable(ret) && tries < RTL_USB_CTRL_RETRIES) {
			usleep_range(RTL_USB_CTRL_RETRY_DELAY_US,
				     RTL_USB_CTRL_RETRY_DELAY_US + 500);
			continue;
		}

		break;
	}
	mutex_unlock(&rdev->io_mutex);

	if (ret < 0)
		dev_err(&rdev->intf->dev,
			"usb ctrl read failed addr=0x%04x len=%u ret=%d tries=%d\n",
			addr, len, ret, tries);
	else if (ret != len)
		return -EIO;

	return (ret < 0) ? ret : 0;
}

static int rtl8188_ctrl_write(struct rtl8188_dev *rdev, u16 addr, const void *buf, u16 len)
{
	int ret;
	int tries;

	mutex_lock(&rdev->io_mutex);
	for (tries = 1; tries <= RTL_USB_CTRL_RETRIES; tries++) {
		ret = usb_control_msg(rdev->udev,
				      usb_sndctrlpipe(rdev->udev, 0),
				      RTL_USB_VENQT_CMD_REQ,
				      RTL_USB_VENQT_WRITE,
				      addr,
				      0,
				      (void *)buf,
				      len,
				      RTL_USB_CTRL_TIMEOUT_MS);

		if (ret == len)
			break;

		if (ret < 0 && rtl8188_usb_err_retryable(ret) && tries < RTL_USB_CTRL_RETRIES) {
			usleep_range(RTL_USB_CTRL_RETRY_DELAY_US,
				     RTL_USB_CTRL_RETRY_DELAY_US + 500);
			continue;
		}

		break;
	}
	mutex_unlock(&rdev->io_mutex);

	if (ret < 0)
		dev_err(&rdev->intf->dev,
			"usb ctrl write failed addr=0x%04x len=%u ret=%d tries=%d\n",
			addr, len, ret, tries);
	else if (ret != len)
		return -EIO;

	return (ret < 0) ? ret : 0;
}

int rtl8188_read8(struct rtl8188_dev *rdev, u16 addr, u8 *val)
{
	return rtl8188_ctrl_read(rdev, addr, val, sizeof(*val));
}

int rtl8188_read16(struct rtl8188_dev *rdev, u16 addr, u16 *val)
{
	__le16 tmp;
	int ret = rtl8188_ctrl_read(rdev, addr, &tmp, sizeof(tmp));

	if (!ret) {
		*val = le16_to_cpu(tmp);
		return 0;
}

	/* Fallback: mot so chip/bridge USB chi on dinh voi truy cap 8-bit */
	{
		u8 b0, b1;

		ret = rtl8188_read8(rdev, addr, &b0);
		if (ret)
			return ret;
		ret = rtl8188_read8(rdev, addr + 1, &b1);
		if (ret)
			return ret;

		*val = (u16)b0 | ((u16)b1 << 8);
	}

	return ret;
}

int rtl8188_read32(struct rtl8188_dev *rdev, u16 addr, u32 *val)
{
	__le32 tmp;
	int ret = rtl8188_ctrl_read(rdev, addr, &tmp, sizeof(tmp));

	if (!ret) {
		*val = le32_to_cpu(tmp);
		return 0;
}

	/* Fallback 8-bit read */
	{
		u8 b0, b1, b2, b3;

		ret = rtl8188_read8(rdev, addr, &b0);
		if (ret)
			return ret;
		ret = rtl8188_read8(rdev, addr + 1, &b1);
		if (ret)
			return ret;
		ret = rtl8188_read8(rdev, addr + 2, &b2);
		if (ret)
			return ret;
		ret = rtl8188_read8(rdev, addr + 3, &b3);
		if (ret)
			return ret;

		*val = (u32)b0 | ((u32)b1 << 8) | ((u32)b2 << 16) | ((u32)b3 << 24);
	}

	return ret;
}

int rtl8188_write8(struct rtl8188_dev *rdev, u16 addr, u8 val)
{
	return rtl8188_ctrl_write(rdev, addr, &val, sizeof(val));
}

int rtl8188_write16(struct rtl8188_dev *rdev, u16 addr, u16 val)
{
	__le16 tmp = cpu_to_le16(val);
	int ret;

	ret = rtl8188_ctrl_write(rdev, addr, &tmp, sizeof(tmp));
	if (!ret)
		return 0;

	/* Fallback 8-bit write */
	ret = rtl8188_write8(rdev, addr, (u8)(val & 0xff));
	if (ret)
		return ret;

	return rtl8188_write8(rdev, addr + 1, (u8)((val >> 8) & 0xff));
}

int rtl8188_write32(struct rtl8188_dev *rdev, u16 addr, u32 val)
{
	__le32 tmp = cpu_to_le32(val);
	int ret;

	ret = rtl8188_ctrl_write(rdev, addr, &tmp, sizeof(tmp));
	if (!ret)
		return 0;

	/* Fallback 8-bit write */
	ret = rtl8188_write8(rdev, addr, (u8)(val & 0xff));
	if (ret)
		return ret;
	ret = rtl8188_write8(rdev, addr + 1, (u8)((val >> 8) & 0xff));
	if (ret)
		return ret;
	ret = rtl8188_write8(rdev, addr + 2, (u8)((val >> 16) & 0xff));
	if (ret)
		return ret;

	return rtl8188_write8(rdev, addr + 3, (u8)((val >> 24) & 0xff));
}

int rtl8188_write_block(struct rtl8188_dev *rdev, u16 addr, const u8 *buf, u16 len)
{
	return rtl8188_ctrl_write(rdev, addr, buf, len);
}
