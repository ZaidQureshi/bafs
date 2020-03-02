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
#define PCI_CLASS_NVME      0x010802 /*Defines this is a NVME Device with PCIID: https://pci-ids.ucw.cz/read/PD/01/08/02*/
#define PCI_CLASS_NVME_MASK 0xffffff

/*Filter for the device*/
static const struct pci_device_id id_table[] =
  {
  { PCI_DEVICE_CLASS(PCI_CLASS_NVME, PCI_CLASS_NVME_MASK) },
  { PCI_DEVICE(0x10ee, 0x903f), },
	{ PCI_DEVICE(0x10ee, 0x9038), },
	{ PCI_DEVICE(0x10ee, 0x9028), },
	{ PCI_DEVICE(0x10ee, 0x9018), },
	{ PCI_DEVICE(0x10ee, 0x9034), },
	{ PCI_DEVICE(0x10ee, 0x9024), },
	{ PCI_DEVICE(0x10ee, 0x9014), },
	{ PCI_DEVICE(0x10ee, 0x9032), },
	{ PCI_DEVICE(0x10ee, 0x9022), },
	{ PCI_DEVICE(0x10ee, 0x9012), },
	{ PCI_DEVICE(0x10ee, 0x9031), },
	{ PCI_DEVICE(0x10ee, 0x9021), },
	{ PCI_DEVICE(0x10ee, 0x9011), },

	{ PCI_DEVICE(0x10ee, 0x8011), },
	{ PCI_DEVICE(0x10ee, 0x8012), },
	{ PCI_DEVICE(0x10ee, 0x8014), },
	{ PCI_DEVICE(0x10ee, 0x8018), },
	{ PCI_DEVICE(0x10ee, 0x8021), },
	{ PCI_DEVICE(0x10ee, 0x8022), },
	{ PCI_DEVICE(0x10ee, 0x8024), },
	{ PCI_DEVICE(0x10ee, 0x8028), },
	{ PCI_DEVICE(0x10ee, 0x8031), },
	{ PCI_DEVICE(0x10ee, 0x8032), },
	{ PCI_DEVICE(0x10ee, 0x8034), },
	{ PCI_DEVICE(0x10ee, 0x8038), },

	{ PCI_DEVICE(0x10ee, 0x7011), },
	{ PCI_DEVICE(0x10ee, 0x7012), },
	{ PCI_DEVICE(0x10ee, 0x7014), },
	{ PCI_DEVICE(0x10ee, 0x7018), },
	{ PCI_DEVICE(0x10ee, 0x7021), },
	{ PCI_DEVICE(0x10ee, 0x7022), },
	{ PCI_DEVICE(0x10ee, 0x7024), },
	{ PCI_DEVICE(0x10ee, 0x7028), },
	{ PCI_DEVICE(0x10ee, 0x7031), },
	{ PCI_DEVICE(0x10ee, 0x7032), },
	{ PCI_DEVICE(0x10ee, 0x7034), },
	{ PCI_DEVICE(0x10ee, 0x7038), },

	{ PCI_DEVICE(0x10ee, 0x6828), },
	{ PCI_DEVICE(0x10ee, 0x6830), },
	{ PCI_DEVICE(0x10ee, 0x6928), },
	{ PCI_DEVICE(0x10ee, 0x6930), },
	{ PCI_DEVICE(0x10ee, 0x6A28), },
	{ PCI_DEVICE(0x10ee, 0x6A30), },
	{ PCI_DEVICE(0x10ee, 0x6D30), },

	{ PCI_DEVICE(0x10ee, 0x4808), },
	{ PCI_DEVICE(0x10ee, 0x4828), },
	{ PCI_DEVICE(0x10ee, 0x4908), },
	{ PCI_DEVICE(0x10ee, 0x4A28), },
	{ PCI_DEVICE(0x10ee, 0x4B28), },

	{ PCI_DEVICE(0x10ee, 0x2808), },

#ifdef INTERNAL_TESTING
	{ PCI_DEVICE(0x1d0f, 0x1042), 0},
#endif
	{0,}
  };




/*First character device*/
static dev_t dev_first;

/*device class*/
static struct class* dev_class;

/*List of Controller devices*/
static struct list ctrl_list;

/*list of host mapped devices*/
static struct list host_list;

/*
static struct list cuda_list;
*/

/*How many NVMe SSD we plan on supporting.*/
static int num_ctrls = 8; 
module_param(num_ctrls, int, 0);
MODULE_PARM_DESC(num_ctrls, "Number of controller devices");

static int curr_ctrls = 0;

/*mmaps the controller registers and returns 0 if successful mapping of regs occured to the userspace*/
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

/*Types of file operations supported for device file*/
static const struct file_operations dev_fops =
  {
   .owner = THIS_MODULE,
   .mmap  = mmap_regs,
  };


/*Add the PCie device to the kernel*/
static int add_pci_dev(struct pci_dev* pdev, const struct pci_device_id* id) {
    long err;
    struct ctrl* c = NULL;

    if(curr_ctrls >= num_ctrls) {
      printk(KERN_NOTICE "[add_pci_dev] Max number of controller devices added\n");
      return 0;
    }

    //pci_free_irq_vectors(pdev);
    /*Requests the bus to become master of DMA and enables it*/
    pci_set_master(pdev);

    printk(KERN_INFO "[add_pci_dev] Adding controller device: %02x:%02x.%1x",
           pdev->bus->number, PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn));

    /*Generate a controller reference in kernel space*/
    c = ctrl_get(&ctrl_list, dev_class, pdev, curr_ctrls);
    if (IS_ERR(c)) {
      return PTR_ERR(c);
    }

    /*Create reference to device memory. Read https://www.kernel.org/doc/Documentation/DMA-API-HOWTO.txt Only the diagram is useful. 
    * If failed, then revoke the controller reference using ctrl_put()
    * */
    err = pci_request_region(pdev, 0, DRIVER_NAME);
    if (err != 0) {
      ctrl_put(c);
      printk(KERN_ERR "[add_pci_dev] Failed to get controller register memory\n");
      return err;
    }

    /*Enable the PCIe device.  Ask low-level code to enable I/O and memory. Wake up the device if it was suspended*/
    err = pci_enable_device(pdev);
    if (err < 0) {
      pci_release_region(pdev, 0);
      ctrl_put(c);
      printk(KERN_ERR "[add_pci_dev] Failed to enable controller\n");
      return err;
    }

    /*Creates the device file for access. Also note, when it fails, we need to clean up all the previous steps one after other*/
    err = ctrl_chrdev_create(c, dev_first, &dev_fops);
    if (err != 0) {
      pci_disable_device(pdev);
      pci_release_region(pdev, 0);
      ctrl_put(c);
      return err;
    }

    curr_ctrls++;

    print_hex_dump(KERN_INFO, "raw_data: ", DUMP_PREFIX_ADDRESS, 16, 1, (u8*) c->reg_addr, 4*16, false);
    printk(KERN_INFO "[add_pci_dev]\tAddr: %p\tCAP: %llx\tCC: %x\n", c->reg_addr, c->regs->CAP, c->regs->CC);
    return 0;
}


/*Removal of PCI device involves finding the mapping address of the controller and 
* clearing all the data structs that are associated with it and finally disabling the device from access.
*/
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
   .name      = DRIVER_NAME,
   .id_table  = id_table,
   .probe     = add_pci_dev,
   .remove    = remove_pci_dev,
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

  /*Allocates a range of char device numbers. 
  * The major number will be chosen dynamically, and returned (along with the first minor number) in dev.*/
  err = alloc_chrdev_region(&dev_first, 0, num_ctrls, DRIVER_NAME);
  if (err < 0) {
    printk(KERN_CRIT "[bafs_nvme_init] Failed to allocated char device region\n");
    return err;
  }

  /*Creates the char device*/
  dev_class = class_create(THIS_MODULE, DRIVER_NAME);
  if(IS_ERR(dev_class)) {
    unregister_chrdev_region(dev_first, num_ctrls);
    printk(KERN_CRIT "[bafs_nvme_init] Failed to create char device class\n");
    return PTR_ERR(dev_class);

  }

  /*Adds the driver structure to the list of registered drivers. 
  * Returns a negative value on error, otherwise 0. 
  * If no error occurred, the driver remains registered even if no device was claimed during registration.*/
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

  printk(KERN_INFO "[bafs_nvme_exit] Driver Unloaded\n");

}

module_exit(bafs_nvme_exit);
