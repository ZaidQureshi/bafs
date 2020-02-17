SHELL = /bin/bash
ifneq ($(xvc_bar_num),)
	XVC_FLAGS += -D__XVC_BAR_NUM__=$(xvc_bar_num)
endif

ifneq ($(xvc_bar_offset),)
	XVC_FLAGS += -D__XVC_BAR_OFFSET__=$(xvc_bar_offset)
endif

$(warning XVC_FLAGS: $(XVC_FLAGS).)

topdir := $(shell cd $(src)/.. && pwd)

TARGET_MODULE:=bafs_nvme

EXTRA_CFLAGS := -I$(topdir)/include $(XVC_FLAGS)

ifneq ($(KERNELRELEASE),)
	$(TARGET_MODULE)-objs := bafs_nvme_mod.o map.o ctrl.o list.o
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
