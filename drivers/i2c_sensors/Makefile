KDIR=/home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/build_image/build
ARCH=arm
CROSS_COMPILE=arm-linux-gnueabihf-
export ARCH CROSS_COMPILE

obj-m += mpu6050_raw.o
obj-m += ap3216c_raw.o

all:app
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules
app:
	arm-linux-gnueabihf-gcc app.c -o App

.PHONY:clean copy

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean
	sudo rm -f /home/rickyduran/sharedir/*.ko App 
copy:
	sudo cp *.ko App /home/rickyduran/sharedir