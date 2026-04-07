#include <linux/kernel.h>
#include <linux/firmware.h>
#include <linux/moduleparam.h>
#include <linux/delay.h>

#include "rtl8188_driver.h"

/* Firmware mac dinh trong he thong */
#define RTL8188_FW_PATH_DEFAULT		"rtlwifi/rtl8188eufw.bin"

/* Cac thanh ghi/bit lien quan den MCU firmware download (skeleton) */
#define REG_MCUFWDL			0x0080
#define MCUFWDL_EN			BIT(0)
#define MCUFWDL_RDY			BIT(1)

/* Dia chi mapping download firmware (skeleton cho phase 2) */
#define RTL8188_FW_DL_START		0x1000

static bool fw_upload_enable = true;
module_param(fw_upload_enable, bool, 0644);
MODULE_PARM_DESC(fw_upload_enable,
		 "Bat upload firmware xuong thiet bi qua USB control transfer (mac dinh: tat)");

static ushort fw_chunk_size = 196;
module_param(fw_chunk_size, ushort, 0644);
MODULE_PARM_DESC(fw_chunk_size, "Kich thuoc chunk upload firmware (byte), mac dinh: 196");

static ushort fw_upload_retries = 3;
module_param(fw_upload_retries, ushort, 0644);
MODULE_PARM_DESC(fw_upload_retries, "So lan retry upload firmware neu that bai");

static ushort fw_ready_poll_max = 50;
module_param(fw_ready_poll_max, ushort, 0644);
MODULE_PARM_DESC(fw_ready_poll_max, "So lan poll REG_MCUFWDL de doi bit RDY");

static ushort fw_ready_poll_us = 2000;
module_param(fw_ready_poll_us, ushort, 0644);
MODULE_PARM_DESC(fw_ready_poll_us, "Khoang tre us giua moi lan poll REG_MCUFWDL");

static bool fw_upload_strict;
module_param(fw_upload_strict, bool, 0644);
MODULE_PARM_DESC(fw_upload_strict,
		 "Neu bat, upload firmware that bai se fail probe (mac dinh: tat)");

static bool fw_sanity_read_enable;
module_param(fw_sanity_read_enable, bool, 0644);
MODULE_PARM_DESC(fw_sanity_read_enable,
		 "Thu doc REG_MCUFWDL luc probe de sanity check (mac dinh: tat)");

static int rtl8188_wait_fwdl_ready(struct rtl8188_dev *rdev)
{
	u16 i;
	u32 val = 0;
	int ret;

	for (i = 0; i < fw_ready_poll_max; i++) {
		ret = rtl8188_read32(rdev, REG_MCUFWDL, &val);
		if (ret)
			return ret;

		if (val & MCUFWDL_RDY)
			return 0;

		usleep_range(fw_ready_poll_us, fw_ready_poll_us + 500);
	}

	dev_err(&rdev->intf->dev,
		"[RTL8188ETV] Timeout doi MCUFWDL_RDY (reg=0x%08x)\n", val);
	return -ETIMEDOUT;
}

static int rtl8188_upload_firmware(struct rtl8188_dev *rdev, const struct firmware *fw)
{
	u16 addr = RTL8188_FW_DL_START;
	size_t offset = 0;
	int ret;
	u16 effective_chunk = fw_chunk_size;

	if (!effective_chunk)
		effective_chunk = 196;
	if (effective_chunk > 512)
		effective_chunk = 512;

	ret = rtl8188_write32(rdev, REG_MCUFWDL, MCUFWDL_EN);
	if (ret)
		return ret;

	while (offset < fw->size) {
		u16 chunk = min_t(u16, effective_chunk, fw->size - offset);

		ret = rtl8188_write_block(rdev, addr, fw->data + offset, chunk);
		if (ret)
			return ret;

		offset += chunk;
		addr += chunk;
	}

	ret = rtl8188_write32(rdev, REG_MCUFWDL, MCUFWDL_EN | MCUFWDL_RDY);
	if (ret)
		return ret;

	ret = rtl8188_wait_fwdl_ready(rdev);
	if (ret)
		return ret;

	dev_info(&rdev->intf->dev,
		 "[RTL8188ETV] Firmware upload done: %zu bytes, chunk=%u\n",
		 fw->size, effective_chunk);

	return 0;
}

int rtl8188_load_firmware(struct rtl8188_dev *rdev)
{
	const struct firmware *fw = NULL;
	u32 fwdl_reg = 0;
	u16 attempt;
	int ret;

	if (fw_sanity_read_enable) {
		ret = rtl8188_read32(rdev, REG_MCUFWDL, &fwdl_reg);
		if (!ret)
			dev_info(&rdev->intf->dev,
				 "[RTL8188ETV] Sanity check REG_MCUFWDL=0x%08x\n", fwdl_reg);
		else
			dev_warn(&rdev->intf->dev,
				 "[RTL8188ETV] Canh bao: khong doc duoc REG_MCUFWDL (ret=%d)\n", ret);
	}

	ret = request_firmware(&fw, RTL8188_FW_PATH_DEFAULT, &rdev->intf->dev);
	if (ret) {
		dev_err(&rdev->intf->dev,
			"[RTL8188ETV] Khong tim thay firmware %s (ret=%d)\n",
			RTL8188_FW_PATH_DEFAULT, ret);
		return ret;
	}

	dev_info(&rdev->intf->dev,
		 "[RTL8188ETV] Da nap firmware vao RAM host: %s (%zu bytes)\n",
		 RTL8188_FW_PATH_DEFAULT, fw->size);

	if (fw_upload_enable) {
		ret = -EIO;
		for (attempt = 1; attempt <= max_t(u16, 1, fw_upload_retries); attempt++) {
			ret = rtl8188_upload_firmware(rdev, fw);
			if (!ret) {
				dev_info(&rdev->intf->dev,
					 "[RTL8188ETV] Upload firmware thanh cong o lan thu %u\n",
					 attempt);
				break;
			}

			dev_warn(&rdev->intf->dev,
				 "[RTL8188ETV] Upload firmware loi o lan thu %u (ret=%d)\n",
				 attempt, ret);
		}

		if (ret)
			dev_err(&rdev->intf->dev,
				"[RTL8188ETV] Upload firmware that bai sau retry (ret=%d)\n", ret);

		if (ret && !fw_upload_strict) {
			dev_warn(&rdev->intf->dev,
				 "[RTL8188ETV] Bo qua loi upload (fw_upload_strict=0), tiep tuc probe\n");
			ret = 0;
		}
	} else {
		dev_info(&rdev->intf->dev,
			 "[RTL8188ETV] Tam chua upload xuong chip (fw_upload_enable=0)\n");
		ret = 0;
	}

	release_firmware(fw);
	return ret;
}
