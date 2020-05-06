SHELL = /bin/bash

ifndef ARCH
ARCH := $(shell uname -m | sed -e 's/i.86/i386/' \
-e 's/armv[0-7]\w\+/arm/' \
-e 's/aarch64/arm64/' \
-e 's/ppc64le/powerpc/' \
)
endif


TARGET_MODULE:=bafs_nvme

EXTRA_CFLAGS := -DKERN

KBUILD_EXTRA_SYMBOLS = /usr/src/nvidia-440.82/Module.symvers

#KBUILD_EXTRA_SYMBOLS := /home/zaid/linux-stable/Module.symvers

ifneq ($(KERNELRELEASE),)
	$(TARGET_MODULE)-objs := bafs_nvme_mod.o map.o ctrl.o list.o admin.o
	obj-m := $(TARGET_MODULE).o
else
	BUILDSYSTEM_DIR:=/lib/modules/$(shell uname -r)/build
	PWD:=$(shell pwd)
all :
	$(MAKE) -C $(BUILDSYSTEM_DIR) M=$(PWD) ARCH=$(ARCH)  modules

clean:
	$(MAKE) -C $(BUILDSYSTEM_DIR) M=$(PWD) ARCH=$(ARCH) clean

install: all
	$(MAKE) -C $(BUILDSYSTEM_DIR) M=$(PWD) ARCH=$(ARCH) modules_install

endif
