# Tên của file đầu ra module (.ko) được quyết định bởi dòng này:
obj-m += rtl8188etv_driver.o

# Khai báo file nguồn (.c) nào sẽ được dùng để build ra file .o rồi thành .ko
rtl8188etv_driver-objs := rtl8188_driver.o rtl_usb_io.o rtl_fw.o

# Lấy đường dẫn của thư mục kernel source đang chạy hiện hành thông qua uname -r
KDIR ?= /lib/modules/$(shell uname -r)/build

# Đường dẫn thư mục hiện hành của chúng ta đang chứa file mã nguồn
PWD := $(shell pwd)

# Tùy chọn biên dịch thêm để hiện cảnh báo lỗi tốt hơn
ccflags-y += -Wno-unused-variable -Wno-unused-function

# Lệnh make mặc định: Build module
all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

# Lệnh dọn dẹp các tệp tạm thời tạo ra trong quá trình build
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
