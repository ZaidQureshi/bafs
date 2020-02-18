#include "list.h"
#include "ctrl.h"
#include "map.h"
#include "regs.h"
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/fs.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/mm_types.h>
#include <asm/io.h>
#include <asm/errno.h>
#include <asm/page.h>


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zaid Qureshi <zaidq2@illinois.edu>");
MODULE_DESCRIPTION("BAFS NVMe driver");
MODULE_VERSION("0.1");

#define DRIVER_NAME         "bafs_nvme"
#define PCI_CLASS_NVME      0x010802
#define PCI_CLASS_NVME_MASK 0xffffff



static const struct pci_device_id id_table[] =
  {
   { PCI_DEVICE_CLASS(PCI_CLASS_NVME, PCI_CLASS_NVME_MASK) },
   { 0 }
  };

static dev_t dev_first;


static struct class* dev_class;


static struct list ctrl_list;


static struct list host_list;

/*
static struct list cuda_list;
*/

static int num_ctrls = 8;
module_param(num_ctrls, int, 0);
MODULE_PARM_DESC(num_ctrls, "Number of controller devices");

static int curr_ctrls = 0;


static int mmap_regs(struct file* f, struct vm_area_struct* vma) {
  struct ctrl* c = NULL;

  c = ctrl_find_by_inode(&ctrl_list, f->f_inode);
  if (c == NULL) {
    printk(KERN_CRIT "[mmap_regs] Unknown controller interface\n");
    return -EBADF;
  }

  if ((vma->vm_end - vma->vm_start) > pci_resource_len(c->pdev, 0)) {
    printk(KERN_WARNING "[mmap_regs] Invalid range size\n");
    return -EINVAL;
  }

  vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
  return vm_iomap_memory(vma, pci_resource_start(c->pdev, 0), (vma->vm_end - vma->vm_start));

}


static const struct file_operations dev_fops =
  {
   .owner = THIS_MODULE,
   .mmap  = mmap_regs,
  };

static int add_pci_dev(struct pci_dev* pdev, const struct pci_device_id* id) {
    long err;
    struct ctrl* c = NULL;

    if(curr_ctrls >= num_ctrls) {
      printk(KERN_NOTICE "[add_pci_dev] Max number of controller devices added\n");
      return 0;
    }

    printk(KERN_INFO "[add_pci_dev] Adding controller device: %02x:%02x.%1x",
           pdev->bus->number, PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn));

    c = ctrl_get(&ctrl_list, dev_class, pdev, curr_ctrls);
    if (IS_ERR(c)) {
      return PTR_ERR(c);
    }

    err = pci_request_region(pdev, 0, DRIVER_NAME);
    if (err != 0) {
      ctrl_put(c);
      printk(KERN_ERR "[add_pci_dev] Failed to get controller register memory\n");
      return err;
    }

    err = pci_enable_device(pdev);
    if (err < 0) {
      pci_release_region(pdev, 0);
      ctrl_put(c);
      printk(KERN_ERR "[add_pci_dev] Failed to enable controller\n");
      return err;
    }

    err = ctrl_chrdev_create(c, dev_first, &dev_fops);
    if (err != 0) {
      pci_disable_device(pdev);
      pci_release_region(pdev, 0);
      ctrl_put(c);
      return err;
    }

    pci_set_master(pdev);

    curr_ctrls++;

    //print_hex_dump(KERN_INFO, "raw_data: ", DUMP_PREFIX_ADDRESS, 16, 1, c->reg_addr, 4*16, false);
    printk(KERN_INFO "[add_pci_dev]\tAddr: %lx\tCAP1: %lx\tCAP2: %lx\n", c->reg_addr, c->regs->CAP, c->regs->CC);
    return 0;
}

static void remove_pci_dev(struct pci_dev* pdev) {
  struct ctrl* c = NULL;
  if (pdev == NULL) {
    printk(KERN_WARNING "[remove_pci_dev] Invoked with NULL\n");
    return;
  }

  curr_ctrls--;

  c = ctrl_find_by_pci_dev(&ctrl_list, pdev);
  printk(KERN_INFO "[remove_pci_dev] c = %p", c);
  ctrl_put(c);

  pci_release_region(pdev, 0);

  pci_clear_master(pdev);
  pci_disable_device(pdev);

  printk(KERN_INFO "[remove_pci_dev] Removing controller device: %02x:%02x.%1x",
         pdev->bus->number, PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn));

}

static struct pci_driver driver =
  {
   .name = DRIVER_NAME,
   .id_table = id_table,
   .probe = add_pci_dev,
   .remove = remove_pci_dev,
  };

static unsigned long clear_map_list(struct list* l) {
  unsigned long i = 0;
  struct list_node* ptr = list_next(&l->head);
  struct map* m;

  while(ptr != NULL) {
    m = container_of(ptr, struct map, list);
    unmap_and_release_map(m);
    i++;
    ptr = list_next(&l->head);
  }
  return i;
}


static int __init bafs_nvme_init(void) {
  int err;

  list_init(&ctrl_list);
  list_init(&host_list);
  /*
  list_init(&cuda_list);
  */

  err = alloc_chrdev_region(&dev_first, 0, num_ctrls, DRIVER_NAME);
  if (err < 0) {
    printk(KERN_CRIT "[bafs_nvme_init] Failed to allocated char device region\n");
    return err;
  }

  dev_class = class_create(THIS_MODULE, DRIVER_NAME);
  if(IS_ERR(dev_class)) {
    unregister_chrdev_region(dev_first, num_ctrls);
    printk(KERN_CRIT "[bafs_nvme_init] Failed to create char device class\n");
    return PTR_ERR(dev_class);

  }

  err = pci_register_driver(&driver);
  if(err != 0) {
    class_destroy(dev_class);
    unregister_chrdev_region(dev_first, num_ctrls);
    printk(KERN_ERR "[bafs_nvme_init] Failed to register as PCI driver\n");

    return err;
  }

  printk(KERN_INFO "[bafs_nvme_init] Driver Loaded\n");
  return 0;

}

module_init(bafs_nvme_init);

static void __exit bafs_nvme_exit(void) {
  unsigned long remaining = 0;

  /*
  remaining = clear_map_list(&cuda_list);
  if (remaining != 0 ) {
    printk(KERN_WARNING "[bafs_nvme_exit] %lu CUDA memory mappings were still in use on unload\n", remaining);
  }
  */

  remaining = clear_map_list(&host_list);
  if (remaining != 0 ) {
    printk(KERN_WARNING "[bafs_nvme_exit] %lu host memory mappings were still in use on unload\n", remaining);
  }

  pci_unregister_driver(&driver);
  class_destroy(dev_class);
  unregister_chrdev_region(dev_first, num_ctrls);

  printk(KERN_INFO "[bafs_nvme_init] Driver Unloaded\n");

}

module_exit(bafs_nvme_exit);
