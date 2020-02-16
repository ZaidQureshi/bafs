obj-m += bafs_nvme.o

bafs_nvme-objs :=  list.o ctrl.o map.o


all:
    make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
    make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
