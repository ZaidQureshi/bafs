#ifndef __BAFS_NVME_MAP_H__
#define __BAFS_NVME_MAP_H__

#include "list.h"
#include "ctrl.h"
#include <linux/types.h>
#include <linux/mm_types.h>


struct map;


typedef void (*release)(struct map*);


struct map {
  struct list_node    list;
  struct task_struct* owner;
  u64                 vaddr;
  struct pci_dev*     pdev;
  unsigned long       page_size;
  release             release;
  unsigned long       n_addrs;
  u64                 addrs[1];
  void*               data;

};


struct map* map_userspace(struct list* l, const struct ctrl* c, u64 vaddr, unsigned long n_pages);


void unmap_and_release_map(struct map* map);


/*
#ifdef CUDA

struct map* map_cuda_device_memory(struct list* l, const struct ctrl* ctrl, u64 vaddr, unsigned long n_pages);

#endif
*/

struct map* map_find(const struct list* l, u64 vaddr);

#endif
