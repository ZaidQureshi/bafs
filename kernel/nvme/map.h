#ifndef __BAFS_NVME_MAP_H__
#define __BAFS_NVME_MAP_H__

#include "list.h"
#include "ctrl.h"
#include <linux/types.h>
#include <linux/mm_types.h>


struct map;


typedef void (*release)(struct map*);

/* Describes mapped memory address range*/
struct map {
  struct list_node    list;     // header
  struct task_struct* owner;    // mapping owner
  u64                 vaddr;    // start virtual addr
  struct pci_dev*     pdev;     // ptr to physical PCIe device
  unsigned long       page_size;// page size 
  release             release;  // releasing mapping callback
  unsigned long       n_addrs;  // # of mapped pages
  u64                 addrs[1]; // bus address
  void*               data;     // custom data
};

/* map the userspace pages for the DMA*/
struct map* map_userspace(struct list* l, const struct ctrl* c, u64 vaddr, unsigned long n_pages);

/*unmap and release the memory*/
void unmap_and_release_map(struct map* map);


/*
#ifdef CUDA

struct map* map_cuda_device_memory(struct list* l, const struct ctrl* ctrl, u64 vaddr, unsigned long n_pages);

#endif
*/

/* search for mapping from vaddr to current task*/
struct map* map_find(const struct list* l, u64 vaddr);

#endif /*__BAFS_NVME_MAP_H__*/
