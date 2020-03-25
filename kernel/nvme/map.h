#ifndef __BAFS_NVME_MAP_H__
#define __BAFS_NVME_MAP_H__

#include "ctrl.h"
#include <linux/types.h>
#include <linux/mm_types.h>

/*GPU Page size is 64KB assumed*/
#define GPU_PAGE_SHIFT 16
#define GPU_PAGE_SIZE  (1UL << GPU_PAGE_SHIFT)
#define GPU_PAGE_MASK  ~(GPU_PAGE_SIZE - 1)

struct map;


typedef void (*release)(struct map*);

/* Describes mapped memory address range*/
struct map {
  //struct list_node    list;     // header
  struct task_struct* owner;    // mapping owner
  u64                 vaddr;    // start virtual addr
  struct pci_dev*     pdev;     // ptr to physical PCIe device
  unsigned long       page_size;// page size 
  release             release;  // releasing mapping callback
  unsigned long       n_addrs;  // # of mapped pages
  void*               data;     // custom data
  u64                 addrs[1]; // bus address

};

/* map the userspace pages for the DMA*/
struct map* map_userspace(const struct ctrl* c, u64 vaddr, unsigned long n_pages);

/*unmap and release the memory*/
void unmap_and_release_map(struct map* map);


/*
#ifdef CUDA

struct map* map_cuda_device_memory(struct list* l, const struct ctrl* ctrl, u64 vaddr, unsigned long n_pages);

#endif
*/

/* search for mapping from vaddr to current task*/
//struct map* map_find(const struct list* l, u64 vaddr);

#endif /*__BAFS_NVME_MAP_H__*/
