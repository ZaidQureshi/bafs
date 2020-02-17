#ifndef __BAFS_NVME_CTRL_H__
#define __BAFS_NVME_CTRL_H__

#include "list.h"
#include <linux/pci.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/device.h>

struct ctrl {
  struct list_node list;
  struct pci_dev* pdev;
  char name[64];
  int number;
  dev_t rdev;
  struct class* cls;
  struct cdev cdev;
  struct device* chrdev;

};

struct ctrl* ctrl_get(struct list* l, struct class* cls, struct pci_dev* pdev, int number);

void ctrl_put(struct ctrl* c);

struct ctrl* ctrl_find_by_pci_dev(const struct list* l, const struct pci_dev* pdev);

struct ctrl* ctrl_find_by_inode(const struct list* l, const struct inode* ind);

int ctrl_chrdev_create(struct ctrl* c, dev_t first, const struct file_operations* fops);


void ctrl_chrdev_remove(struct ctrl* c);


#endif
