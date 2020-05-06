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
#include <linux/dma-mapping.h>


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
//static struct list host_list;

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
#ifdef TEST2

#include <linux/fs.h>
#include "nv-linux.h"
#include "nv.h"
#include "uvm8_api.h"
#include "uvm8_global.h"
#include "uvm8_gpu_replayable_faults.h"
#include "uvm8_tools_init.h"
#include "uvm8_lock.h"
#include "uvm8_test.h"
#include "uvm8_va_space.h"
#include "uvm8_va_range.h"
#include "uvm8_va_block.h"
#include "uvm8_tools.h"
#include "uvm_common.h"
#include "uvm_linux_ioctl.h"
#include "uvm8_hmm.h"
#include "uvm8_mem.h"


static NV_STATUS my_uvm_api_initialize(UVM_INITIALIZE_PARAMS *params, struct file *filp)
{
    NV_STATUS status = NV_OK;
    uvm_va_space_t *va_space = uvm_va_space_get(filp);

    if ((params->flags & ~UVM_INIT_FLAGS_MASK))
        return NV_ERR_INVALID_ARGUMENT;

    uvm_down_write_mmap_sem(&current->mm->mmap_sem);
    uvm_va_space_down_write(va_space);

    if (atomic_read(&va_space->initialized)) {
        // Already initialized - check if parameters match
        if (params->flags != va_space->initialization_flags)
            status = NV_ERR_INVALID_ARGUMENT;
    }
    else {
        va_space->initialization_flags = params->flags;

        status = uvm_hmm_mirror_register(va_space);
        if (status == NV_OK) {
            // Use release semantics to match the acquire semantics in
            // uvm_va_space_initialized. See that function for details. All
            // initialization must be complete by this point.
            atomic_set_release(&va_space->initialized, 1);
        }
    }

    uvm_va_space_up_write(va_space);
    uvm_up_write_mmap_sem(&current->mm->mmap_sem);

    return status;
}

void test_uvm(void) {
  struct file* uvm_file;
  UVM_INITIALIZE_PARAMS params;
  NV_STATUS status = NV_OK;
  uvm_va_space_t *va_space;

  uvm_file = filp_open("/dev/nvidia-uvm", O_RDWR, 0);
  if (uvm_file != NULL) {
    params.flags = UVM_INIT_FLAGS_MASK;
    va_space = uvm_va_space_get(uvm_file);
    printk(KERN_CRIT "uvm_va_space_get %llx\n", va_space);
    /*
      status = my_uvm_api_initialize(&params, uvm_file);
      if (status != NV_OK) {
      printk(KERN_CRIT "Failed to initialize!\n");
      }
      else {
      printk(KERN_CRIT "UVM Success!\n");
      }
    */

    filp_close(uvm_file,NULL);
  }

  else {
    printk(KERN_CRIT "Could not open uvm file!\n");
  }
}
#endif
static long custom_ioctl(struct file* file, unsigned int cmd, unsigned long arg) {
  long retval = 0;
  struct ctrl* ctrl = NULL;
  //struct nvm_ioctl_map request;
  struct map* map = NULL;
  u64 addr;

  ctrl = ctrl_find_by_inode(&ctrl_list, file->f_inode);
  if (ctrl == NULL) {
    printk(KERN_CRIT "Unknown controller reference\n");
    return -EBADF;
  }

  switch (cmd)
  {
    case 0:
#ifdef TEST2
      test_uvm();
#endif
      break;
    default:
      break;
  }
  return retval;
}

/*Types of file operations supported for device file*/
static const struct file_operations dev_fops =
  {
  .owner = THIS_MODULE,
  .unlocked_ioctl = custom_ioctl,
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
    dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));

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
    printk(KERN_INFO "[add_pci_dev]\tAddr: %p\tCAP0: %llx\tCAP1: %llx\tCC: %x\n", c->reg_addr, c->regs->CAP[0], c->regs->CAP[1], c->regs->CC);
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

/*
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
*/

static char* custom_devnode(struct device* dev, umode_t* mode) {
  if (!mode)
    return NULL;

  *mode = 0666;
  return NULL;
}

static int __init bafs_nvme_init(void) {
  int err;

  list_init(&ctrl_list);
  //list_init(&host_list);
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

  dev_class->devnode = custom_devnode;

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
#ifdef TEST2
  test_uvm();
#endif
  return 0;

}

module_init(bafs_nvme_init);

static void __exit bafs_nvme_exit(void) {
  //unsigned long remaining = 0;

  /*
  remaining = clear_map_list(&cuda_list);
  if (remaining != 0 ) {
    printk(KERN_WARNING "[bafs_nvme_exit] %lu CUDA memory mappings were still in use on unload\n", remaining);
  }
  */

  /*
  remaining = clear_map_list(&host_list);
  if (remaining != 0 ) {
    printk(KERN_WARNING "[bafs_nvme_exit] %lu host memory mappings were still in use on unload\n", remaining);
  }
  */
  pci_unregister_driver(&driver);
  class_destroy(dev_class);
  unregister_chrdev_region(dev_first, num_ctrls);

  printk(KERN_INFO "[bafs_nvme_exit] Driver Unloaded\n");

}

module_exit(bafs_nvme_exit);
