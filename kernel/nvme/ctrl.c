#include "ctrl.h"
#include "list.h"
#include "admin.h"

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <asm/errno.h>



/* ctrl_get creates a controller reference in the kernel space. 
*  PCIe device specific paramters are loaded to the controller struct.
*  We also perform the admin queue pair allocation in the controller
*  and initial it. 
*/


struct ctrl* ctrl_get(struct list* l, struct class* cls, struct pci_dev* pdev, int number) {
  struct ctrl* c = NULL;
#ifdef TEST
#define NUM 2
#define SIZE (4*1024ULL * 1024ULL * 1024ULL)
    dma_addr_t* dma_addrs = kmalloc(NUM*sizeof(dma_addr_t), GFP_KERNEL);
    void** addrs = kmalloc(NUM*sizeof(void*), GFP_KERNEL);
    u64 cnt = 0;
    u64 v = 0;
#endif
  c = kmalloc(sizeof(struct ctrl), GFP_KERNEL | GFP_NOWAIT);
  if (c == NULL) {
    printk(KERN_CRIT "[ctrl_get] Failed to allocate controller reference\n");
    return ERR_PTR(-ENOMEM);
  }

  list_node_init(&c->list);

  c->pdev   = pdev;
  c->number = number;
  c->rdev   = 0;
  c->cls    = cls;
  c->chrdev = NULL;

  snprintf(c->name, sizeof(c->name), "%s%d", KBUILD_MODNAME, c->number);
  c->name[sizeof(c->name) - 1] = '\0';

  /*Read the register values of the controller using PCI-IOMAP function*/
  c->reg_addr = pci_iomap(c->pdev, 0, pci_resource_len(c->pdev, 0));
  c->reg_len  = pci_resource_len(c->pdev, 0);
  c->regs     = (volatile struct nvme_regs*)c->reg_addr;

  printk(KERN_INFO "Reg ADDR: %llx\tReg LEN: %llu\tCAP: %llx\n", (long long unsigned int) c->reg_addr, (unsigned long long) c->reg_len, (long long unsigned int) c->regs->CAP);
  c->aqp      = kmalloc(sizeof(struct admin_queue_pair), GFP_KERNEL | GFP_NOWAIT);
  if (c->aqp == NULL) {
    printk(KERN_ERR "[ctrl_get] Failed to alocated admin queue\n");
    return ERR_PTR(-ENOMEM);
  }

  admin_init(c->aqp, c);

#ifdef TEST
  for (cnt = 0; cnt < NUM; cnt++) {
    addrs[cnt] = dma_alloc_coherent(&pdev->dev, SIZE, dma_addrs + cnt, GFP_KERNEL | __GFP_COMP);
    if(addrs[cnt] == NULL) {
      printk(KERN_INFO "----- dma_alloc_coherent %llu failed!\n", cnt);
      break;
    }
    else {
      v++;
      printk(KERN_INFO "+++++ dma_alloc_coherent %llu passed: %llx!\n", cnt, (u64)(dma_addrs[cnt]));
    }
  }
  for (cnt = 0; cnt < v; cnt++) {
    dma_free_coherent(&pdev->dev, SIZE, addrs[cnt], dma_addrs[cnt]);
  }
  kfree(dma_addrs);
  kfree(addrs);
#endif

  list_insert(l, &c->list);

  return c;
}


/*
*  When you release the ctrl from the kernel, we need to remove all the datastruct that
*  we have used, reset the admin queues and remove the pcie device from the kernel space. 
*/

void ctrl_put(struct ctrl* c) {
  if (c != NULL) {
    list_remove(&c->list);
    ctrl_chrdev_remove(c);
    admin_clean(c->aqp);
    kfree(c->aqp);
    kfree(c);
  }
}

/*
* Using the member function pdev inside the list, we try finding 
* the address of the ctrl of the pci device usin the below function. 
* To know more about how container_of works visit here:
* https://embetronicx.com/tutorials/linux/c-programming/understanding-of-container_of-macro-in-linux-kernel/
*/

struct ctrl* ctrl_find_by_pci_dev(const struct list* l, const struct pci_dev* pdev) {
  const struct list_node* e = list_next(&l->head);
  struct ctrl*            c;

  while (e      != NULL) {
    c            = container_of(e, struct ctrl, list);
    if (c->pdev == pdev) {
      return c;
    }
    e = list_next(e);

  }

  return NULL;
}

/*
* Using the member function inode inside the list, we try finding 
* the address of the ctrl of the pci device usin the below function. 
* To know more about how container_of works visit here:
* https://embetronicx.com/tutorials/linux/c-programming/understanding-of-container_of-macro-in-linux-kernel/
*/

struct ctrl* ctrl_find_by_inode(const struct list* l, const struct inode* ind) {
  const struct list_node* e = list_next(&l->head);
  struct ctrl*            c;

  while (e       != NULL) {
    c = container_of(e, struct ctrl, list);
    if (&c->cdev == ind->i_cdev) {
      return c;
    }
    e = list_next(e);

  }

  return NULL;
}

/* In our world, we want system calls go directly to the device drivers. We dont want to 
* involve file system mechanisms to interact with the NVMe device. This is primarily to reduce the 
* overhead. Thus we map the NVMe device as a chrdev using chrdev create function call which returns 0
* if it successfully created the deviced and setup the filoe operations such as read, write, seek, open, flush, release and llseek
* One the creation is successful, we should be able to see them in the /dev/ location. 
* As it is aparent, we need a method to remove the char device and that is done using - ctrl_chrdev_remove
*/

int ctrl_chrdev_create(struct ctrl* c, dev_t first, const struct file_operations* fops) {
  int            err;
  struct device* chrdev = NULL;

  if (c->chrdev != NULL) {
    printk(KERN_WARNING "[ctrl_chrdev_create] Character device already exists\n");
    return 0;
  }

  c->rdev = MKDEV(MAJOR(first), MINOR(first) + c->number);


  cdev_init(&c->cdev, fops);
  err = cdev_add(&c->cdev, c->rdev, 1);
  if (err != 0) {
    printk(KERN_ERR "[ctrl_chrdev_create] Failed to add cdev\n");
    return err;
  }

  chrdev = device_create(c->cls, NULL, c->rdev, NULL, c->name);
  if(IS_ERR(chrdev)) {
    cdev_del(&c->cdev);
    printk(KERN_ERR "[ctrl_chrdev_create] Failed to create character device\n");
    return PTR_ERR(chrdev);

  }


  c->chrdev = chrdev;

  printk(KERN_INFO "[ctrl_chrdev_create] Character device /dev/%s created (%d.%d)\n", c->name, MAJOR(c->rdev), MINOR(c->rdev));

  return 0;
}

void ctrl_chrdev_remove(struct ctrl* c) {

  if (c->chrdev != NULL) {
    device_destroy(c->cls, c->rdev);
    cdev_del(&c->cdev);
    c->chrdev = NULL;

    printk(KERN_INFO "[ctrl_chrdev_create] Character device /dev/%s removed (%d.%d)\n", c->name, MAJOR(c->rdev), MINOR(c->rdev));
  }
}
