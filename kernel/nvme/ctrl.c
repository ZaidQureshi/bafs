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
  }
}
