#ifndef __BAFS_NVME_CTRL_H__
#define __BAFS_NVME_CTRL_H__

#include "list.h"
#include "regs.h"
#include "admin.h"
#include <linux/pci.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/device.h>


/*NVMe controller Definition*/
struct ctrl {
  struct list_node           list;      // Linked list. Points to head
  struct pci_dev*            pdev;      // PCI device
  char                       name[64];  // Name of device
  int                        number;    // Device Serial Number
  dev_t                      rdev;      // Device Register
  struct class*              cls;       // Device class
  struct cdev                cdev;      // Device
  struct device*             chrdev;    // Device handle
/*below set of defns are used for reading the register space*/
  volatile u8*               reg_addr;  // NVMe controller register base address 
  u64                        reg_len;   // Register space - usually 4KB
  volatile struct nvme_regs* regs;      // structure pointing to the register with mapping
/*below set of defns are used for controlling the admin queues*/
  struct admin_queue_pair*   aqp;       // admin queue pair
  u32                        dstrd;     // CAP.DSTRD register value - dynamic. 
  u32                        page_size; // Page size of the device
  u32                        timeout;   // Time out value of the device.
  struct list                sq_list;
  struct list                cq_list;


};

/*Get controller reference*/
struct ctrl* ctrl_get(struct list* l, struct class* cls, struct pci_dev* pdev, int number);

/*Release a controller reference*/
void ctrl_put(struct ctrl* c);

/*Search for the controller device*/
struct ctrl* ctrl_find_by_pci_dev(const struct list* l, const struct pci_dev* pdev);

/*Search for the controller device by inode*/
struct ctrl* ctrl_find_by_inode(const struct list* l, const struct inode* ind);

/*Create character device with all initialization set up for file operations*/
int ctrl_chrdev_create(struct ctrl* c, dev_t first, const struct file_operations* fops);

/*Remove character device*/
void ctrl_chrdev_remove(struct ctrl* c);

#endif /*__BAFS_NVME_CTRL_H__*/
