SHELL = /bin/bash

TARGET_MODULE:=bafs_nvme

EXTRA_CFLAGS := -DKERN

ifneq ($(KERNELRELEASE),)
	$(TARGET_MODULE)-objs := bafs_nvme_mod.o map.o ctrl.o list.o admin.o
	obj-m := $(TARGET_MODULE).o
else
	BUILDSYSTEM_DIR:=/lib/modules/$(shell uname -r)/build
	PWD:=$(shell pwd)
all :
	$(MAKE) -C $(BUILDSYSTEM_DIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(BUILDSYSTEM_DIR) M=$(PWD) clean

install: all
	$(MAKE) -C $(BUILDSYSTEM_DIR) M=$(PWD) modules_install

endif
