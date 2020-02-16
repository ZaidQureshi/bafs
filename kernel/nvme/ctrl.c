#include "ctrl.h"
#include "list.h"

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <asm/errno.h>

struct ctrl* ctrl_get(struct list* l, struct class* cls, struct pci_dev* pdev, int number) {
  struct ctrl* c = NULL;

  c = kmalloc(sizeof(struct ctrl), GFP_KERNEL | GFP_NOWAIT);
  if (c == NULL) {
    printk(KERN_CRIT "[ctrl_get] Failed to allocate controller reference\n");
    return ERR_PTR(-ENOMEM);
  }

  list_node_init(&c->list);

  c->pdev = pdev;
  c->number = number;
  c->rdev = 0;
  c->cls = cls;
  c->chrdev = NULL;

  snprintf(c->name, sizeof(c->name), "%s%d\0", KBUILD_MODNAME, c->number);

  return c;
}

void ctrl_put(struct ctrl* c) {
  if (ctrl != NULL) {
    list_remove(&c->list);
    ctrl_chrdev_remove(c);
    kfree(c);
  }
}

struct ctrl* ctrl_find_by_pci_dev(const struct list* l, const struct pci_dev* pdev) {
  const struct list_node* e = list_next(&list->head);
  struct ctrl* c;

  while (e != NULL) {
    c = container_of(e, struct ctrl, l);
    if (c->pdev == pdev) {
      return c;
    }
    e = list_next(e);

  }

  return NULL;
}


struct ctrl* ctrl_find_by_inode(const struct list* l, const struct inode* ind) {
  const struct list_node* e = list_next(&list->head);
  struct ctrl* c;

  while (e != NULL) {
    c = container_of(e, struct ctrl, l);
    if (c->cdev == ind->i_cdev) {
      return c;
    }
    e = list_next(e);

  }

  return NULL;
}


long ctrl_chrdev_create(struct ctrl* c, dev_t first, const struct file_operations* fops) {
  long err;
  struct device* chrdev = NULL;

  if (c->chrdev != NULL) {
    printk(KERN_WARNING "[ctrl_chrdev_create] Character device already exists\n");
    return 0;
  }

  c->rdev - MKDEV(MAJOR(first), MINOR(first) + c->number);

  cdev_init(&c->cdev, fops);
  err = cdev_add(&c->cdev, c->rdev, 1);
  if (err != 0) {
    printk(KERN_ERR "[ctrl_chrdev_create] Failed to add cdev\n");
    return err;
  }

  chrdev = device_create(c->cls, NULL, c->rdev, NULL, c->name);
  if(IS_ERR(chrdev)) {
    cdev_devl(&c->cdev);
    printk(KERN_ERR "[ctrl_chrdev_create] Failed to create character device\n");
    return PTR_ERR(chrdev);

  }


  c->chrdev = chrdev;

  prink(KERN_INFO "[ctrl_chrdev_create] Character device /dev/%s created (%d.%d)\n", c->name, MAJOR(c->rdev), MINOR(c->rdev));

  return 0;
}

void ctrl_chrdev_remove(struct ctrl* c) {

  if (c->chrdev != NULL) {
    device_destrpy(c->cls, c->rdev);
    cdev_del(&c->cdev);
    c->chrdev = NULL;

    prink(KERN_INFO "[ctrl_chrdev_create] Character device /dev/%s removed (%d.%d)\n", c->name, MAJOR(c->rdev), MINOR(c->rdev));
  }
}
