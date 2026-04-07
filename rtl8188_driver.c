#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/usb.h>
#include <linux/slab.h>

#include "rtl8188_driver.h"
#include <linux/netdevice.h>
#include <linux/etherdevice.h>

/*
 * Các hàm cơ bản để Linux kernel nhận diện card mạng.
 * Tạm thời làm khung (skeleton) để `ip link` có thể thấy và bật card mạng.
 */
static int rtl8188_netdev_open(struct net_device *netdev)
{
    netif_start_queue(netdev);
    return 0;
}

static int rtl8188_netdev_stop(struct net_device *netdev)
{
    netif_stop_queue(netdev);
    return 0;
}

static netdev_tx_t rtl8188_netdev_xmit(struct sk_buff *skb, struct net_device *netdev)
{
    dev_kfree_skb(skb);
    return NETDEV_TX_OK;
}

static const struct net_device_ops rtl8188_netdev_ops = {
    .ndo_open = rtl8188_netdev_open,
    .ndo_stop = rtl8188_netdev_stop,
    .ndo_start_xmit = rtl8188_netdev_xmit,
};

/*
 * Các hàm của cfg80211 để quản lý Wi-Fi
 */
static int rtl8188_cfg80211_scan(struct wiphy *wiphy, struct cfg80211_scan_request *request)
{
    pr_info("[RTL8188ETV] Nhan lenh quet WiFi (Scan) tu he thong!\n");
    // Tạm thời báo là cấu trúc chưa hỗ trợ đầy đủ việc tìm kiếm điểm truy cập
    return -EOPNOTSUPP;
}

static const struct cfg80211_ops rtl8188_cfg80211_ops = {
    .scan = rtl8188_cfg80211_scan,
};

// Định nghĩa Vendor ID của hãng Realtek
#define RTL_USB_VENDOR_ID   0x0bda
// Định nghĩa Product ID của card mạng RTL8188ETV
#define RTL_USB_PRODUCT_ID  0x0179

/* 
 * Bảng cấu hình thiết bị (Device ID table). 
 * Khi bạn cắm một cổng USB bất kì vào máy tính, hệ điều hành Linux sẽ duyệt vòng 
 * qua bảng này ở mọi module. Nếu VendorID và ProductID của thực thể phần cứng
 * khớp với bảng này thì hệ điều hành gọi hàm .probe để trao quyền điều khiển thiết bị
 * đó cho driver của chúng ta.
 */
static const struct usb_device_id rtl8188_device_table[] = {
    { USB_DEVICE(RTL_USB_VENDOR_ID, RTL_USB_PRODUCT_ID) }, // Macro USB_DEVICE giúp rút ngắn việc điền số ID
    {} /* Cần phải có một mảng rỗng cuối cùng để làm dấu hiệu đánh dấu kết thúc mảng */
};
/* Đưa bảng trên vào hệ thống để (Udev) nhận diện tự động load module. */
MODULE_DEVICE_TABLE(usb, rtl8188_device_table); 

/*
 * Hàm thăm dò (Probe function):
 * - Hàm này chạy một lần ngay khi bạn vừa cắm usb có chứa card wifi RTL8188ETV.
 * - intf: Con trỏ đến cấu trúc interface USB. Một thiết bị sẽ có nhiều endpoint.
 * - id: Thông tin mô tả thiết bị hiện tại (VID/PID).
 * Trả về 0 khi khởi tạo thành công thiết bị. Âm nếu lỗi.
 */
static int rtl8188_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
    struct rtl8188_dev *rdev;
    int ret;

    rdev = kzalloc(sizeof(*rdev), GFP_KERNEL);
    if (!rdev)
        return -ENOMEM;

    rdev->intf = intf;
    rdev->udev = usb_get_dev(interface_to_usbdev(intf));
    mutex_init(&rdev->io_mutex);
    usb_set_intfdata(intf, rdev);

    pr_info("[RTL8188ETV] Thanh cong! Da phat hien va khoi tao thiet bi WiFi.\n");
    pr_info("[RTL8188ETV] Vendor ID tim thay: %04X, Product ID tim thay: %04X\n", id->idVendor, id->idProduct);

    ret = rtl8188_load_firmware(rdev);
    if (ret) {
        pr_err("[RTL8188ETV] Loi phase 2: nap firmware that bai, ret=%d\n", ret);
        goto err_free_dev;
    }

    // Đăng ký Network Interface (wlan0)
    rdev->netdev = alloc_etherdev(0);
    if (!rdev->netdev) {
        ret = -ENOMEM;
        goto err_free_dev;
    }

    // Khai báo là một thiết bị Wi-Fi (Wiphy) thay vì chỉ là LAN
    rdev->wiphy = wiphy_new(&rtl8188_cfg80211_ops, 0);
    if (!rdev->wiphy) {
        ret = -ENOMEM;
        free_netdev(rdev->netdev);
        goto err_free_dev;
    }
    
    // Gắn thiết bị USB vào "Lõi Wi-Fi" này
    wiphy_dev(rdev->wiphy)->parent = &intf->dev;

    // Đăng ký "Lõi Wi-Fi" với hệ điều hành
    ret = wiphy_register(rdev->wiphy);
    if (ret) {
        pr_err("[RTL8188ETV] Loi dang ky wiphy, ret=%d\n", ret);
        wiphy_free(rdev->wiphy);
        free_netdev(rdev->netdev);
        goto err_free_dev;
    }

    // Gắn Lõi Wi-Fi vào Card Mạng để nó thực sự trở thành Card Wi-Fi
    SET_NETDEV_DEV(rdev->netdev, wiphy_dev(rdev->wiphy));

    ret = register_netdev(rdev->netdev);
    if (ret) {
        pr_err("[RTL8188ETV] Loi khong the dang ky net_device, ret=%d\n", ret);
        wiphy_unregister(rdev->wiphy);
        wiphy_free(rdev->wiphy);
        free_netdev(rdev->netdev);
        goto err_free_dev;
    }

    pr_info("[RTL8188ETV] Da dang ky interface Wi-Fi (wlanX) thanh cong!\n");

    return 0;

err_free_dev:
    usb_set_intfdata(intf, NULL);
    usb_put_dev(rdev->udev);
    kfree(rdev);
    return ret;
}

/*
 * Hàm ngắt kết nối (Disconnect function):
 * - Kernel sẽ gọi hàm này ngay khu vực bạn RÚT kết nối USB của thiết bị.
 * - Bạn dùng hàm này để gỡ các cấp phát vùng nhớ, hoặc ngắt trạng thái (RX/TX) để tránh crash hệ điều hành.
 */
static void rtl8188_disconnect(struct usb_interface *intf)
{
    struct rtl8188_dev *rdev = usb_get_intfdata(intf);

    usb_set_intfdata(intf, NULL);
    if (rdev) {
        // Gỡ bỏ hệ thống Wi-Fi
        if (rdev->wiphy) {
            wiphy_unregister(rdev->wiphy);
            wiphy_free(rdev->wiphy);
        }
        if (rdev->netdev) {
            unregister_netdev(rdev->netdev);
            free_netdev(rdev->netdev);
        }
        usb_put_dev(rdev->udev);
        kfree(rdev);
    }

    pr_info("[RTL8188ETV] Thiet bi WiFi da bi rut ra khoi may hoac ngat ket noi!\n");
}

/* 
 * Cấu trúc thiết lập thông tin Driver dùng cho giao thức USB.
 * Định nghĩa các thao tác cho Kernel biết cách làm việc với driver.
 */
static struct usb_driver rtl8188_usb_driver = {
    .name       = "rtl8188etv_my_driver",    // Tên nhận dạng của driver chuyên trách này
    .probe      = rtl8188_probe,             // Kéo trỏ đến hàm khi CẮM vật lý thiết bị
    .disconnect = rtl8188_disconnect,        // Kéo trỏ đến hàm khi RÚT vật lý thiết bị
    .id_table   = rtl8188_device_table,      // Con trỏ tới mảng mã phần cứng định nghĩa phía trên
};

/*
 * Hàm khởi tạo Module hệ thống (Module init function):
 * Hàm này chạy duy nhất 1 lần khi bạn nạp module lần đầu bằng `sudo insmod rtl8188etv_driver.ko` 
 */
static int __init rtl8188_init(void)
{
    pr_info("[RTL8188ETV] Dang nap Driver moi cua do an vao he dieu hanh Kernel...\n");
    // Đăng ký toàn bộ đặc điểm của struct `rtl8188_usb_driver` vào danh sách quản lý của Kernel.
    return usb_register(&rtl8188_usb_driver);
}

/*
 * Hàm hủy bỏ Module hệ thống (Module exit function):
 * Hàm này chạy 1 lần khi bạn rút module này ra khỏi kernel bằng `sudo rmmod rtl8188etv_driver`
 */
static void __exit rtl8188_exit(void)
{
    // Bỏ đăng ký với cấu trúc usb chung của Kernel
    usb_deregister(&rtl8188_usb_driver);
    pr_info("[RTL8188ETV] Da thao bo module driver cua do an khoi bo nho!\n");
}

/* Hàm macro truyền vào các hàm vừa tạo để Linux kernel tự khởi động cùng. */
module_init(rtl8188_init);
module_exit(rtl8188_exit);

// Khai báo giấy phép, tác giả, để Kernel không chặn (Kernel requires GPL for internal hooks)
MODULE_LICENSE("GPL");
MODULE_AUTHOR("SV Do An");
MODULE_DESCRIPTION("Driver mang WiFi cho USB RTL8188ETV (Tu lam)");
