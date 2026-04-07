#!/bin/bash

# Dừng và gỡ bỏ các module gốc mặc định của hệ điều hành
echo "[+] Gỡ bỏ driver r8188eu và rtl8xxxu hiện tại..."
rmmod r8188eu 2>/dev/null
rmmod rtl8xxxu 2>/dev/null

# Đưa hai module này vào danh sách đen (blacklist) để khi khởi động lại máy, hệ thống không tự load chúng nữa
echo "[+] Thêm cấu hình blacklist vào /etc/modprobe.d/rtl_custom.conf..."
echo "blacklist r8188eu" > /etc/modprobe.d/rtl_custom.conf
echo "blacklist rtl8xxxu" >> /etc/modprobe.d/rtl_custom.conf

echo "[+] Thành công! Bây giờ thiết bị rút/cắm sẽ không bị hệ điều hành chiếm dụng."

# Có thể cập nhật lại initramfs nếu cần thiết, nhưng với USB cắm nóng thường chỉ cần thế này lệnh modprobe sẽ bỏ qua.
#file này đã disable của driver của hệ thống rồi nên không bị driver của hệ thống chiếm nữa wls35u1

# wls35u1 này là của usb, chipset của usb là rtl8188etv wifi
# B1: Cắm usb vào -> nhận, log ra dmesg là usb đã nhận, rút ra thì log rút
# dmesg không có lỗi -> ok

# B2: scan wifi khả dụng 

# B3: Kết nói wifi

# tất cả phải đẩm bảo là trong dmesg nó ko có lỗi
