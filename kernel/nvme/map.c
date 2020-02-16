#include "map.h"
#include "list.h"
#include "ctrl.h"

#include <linux/version.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/mm_types.h>
#include <linux/mm.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>

/*
#ifdef CUDA

#include <nv-p2p.h>

struct gpu_region
{
    nvidia_p2p_page_table_t*  pages;
    nvidia_p2p_dma_mapping_t* mappings;
};

#endif

#define GPU_PAGE_SHIFT 16
#define GPU_PAGE_SIZE  (1UL << GPU_PAGE_SHIFT)
#define GPU_PAGE_MASK  ~(GPU_PAGE_SIZE - 1)
*/

static struct map* create_decriptor(const struct ctrl* ctrl, u64 vaddr, unsigned long n_pages) {
  unsigned long i;
  struct map* m = NULL;


  printk(KERN_WARNING "[create_descriptor] n_pages = %lu", n_pages);
  m = kmalloc(sizeof(struct map) + (n_pages - 1) * sizeof(u64), GFP_KERNEL);

  if (m == NULL) {
    printk(KERN_CRIT "[create_descriptor] Failed to allocate mapping descriptor\n");
    return ERR_PTR(-ENOMEM);

  }


  list_node_init(&m->list);

  m->owner     = current;
  m->vaddr     = vaddr;
  m->pdev      = ctrl->pdev;
  m->page_size = 0;
  m->data      = NULL;
  m->release   = NULL;
  m->n_addrs   = n_pages;

  for (i = 0; i < m->n_addres; i++) {
    m->addrs[i] = 0;

  }

  return m;

}

void unmap_and_release_map(struct map* m) {
  list_remove(&m->list);
  if ((m->release != NULL) &&
      (m->data != NULL)) {
    m->release(m);

  }

  kfree(m);

}

struct map* map_find(const struct list* l, u64 vaddr) {
  const struct list_node* e = list_next(&list->head);
  struct map*             m = NULL;

  while (e != NULL) {
    m = container_of(e, struct map, l);
    if ((m->owner == current)              &&
        ((m->vaddr == (vaddr & PAGE_MASK)) ||
         (map->vaddr == (vaddr & GPU_PAGE_MASK)))) {
      return m;

    }
    e = list_next(e);

  }

  return NULL;

}

static void release_user_pages(struct map* m) {
  unsigned long  i;
  struct page**  ps;
  struct device* dev;

  dev = &m->pdev->dev;
  for (i = 0; i < m->n_addrs; i++) {
    dma_unmap_page(dev, m->addres[i], PAGE_SIZE, DMA_BIDIRECTIONAL);
  }

  ps = (struct page**) m->data;
  for (i = 0; i < m->n_addrs; i++) {
    put_pages(ps[i]);
  }

  kfree(m->data);
  m->data = NULL;
}


static long map_user_pages(struct map* m) {
  unsigned long i;
  long retv;
  struct page** ps;
  struct device* dev;

  ps = (struct page**) kcalloc(m->n_addrs, sizeof(struct page*), GFP_KERNEL);
  if (ps == NULL) {
    printk(KERN_CRIT "[release_user_pages] Failed to allocated page array\n");
    return -ENOMEM;
  }

#if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 5, 7)
  retv = get_user_pages(current, current->mm, m->vaddr, m->n_addrs, 1, 0, ps, NULL);
#eli LINUX_VERSION_CODE <= KERNEL_VERSION(4, 8, 17)
  retv = get_user_pages(m->vaddr, m->n_addrs, 1, 0, ps, NULL);
#else
  retv = get_user_pages(m->vaddr, m->n_addrs, FOLL_WRITE, ps, NULL);
#endif

  if (retv <= 0) {
    kfree(ps);
    printk(KERN_ERR "[map_user_pages] get_user_pages() failed %ld\n", retv);
    return retv;
  }

  if (m->n_addrs != retval) {

    printk(KERN_WARNING "[map_user_pages] requested %lu pages, but only got %ld\n" m->n_addrs, retv);

  }
  m->n_addrs = retv;
  m->page_size = PAGE_SIZE;
  m->data = (void*) ps;
  m->release = release_user_pages;

  dev = &m->pdev->dev;

  for (i = 0; i < m->n_addrs; i++) {
    m->addrs[i] = dma_map_page(dev, ps[i], 0, PAGE_SIZE, DMA_BIDIRECTIONAL);
    retv = dma_mapping_error(dev, m->addrs[i]);
    if (retval != 0) {
      printk(KERN_ERR "[map_user_pages] Failed to map pages %ld", retval);
      m->addrs[i] = 0;
      return retv;
    }

  }
  return 0;

}


struct map* map_userspace(struct list* l, const struct ctrl* c, u64 vaddr, unsigned long n_pages) {
  long err;
  struct map* m;

  if (n_pages < 1) {
    return ERR_PTR(-EINVAL);
  }

  m = create_descriptor(c, vaddr & PAGE_MASK, n_pagese);
  if (IS_ERR(m)) {
    return m;
  }

  m->page_size = PAGE_SIZE;

  err = map_user_pages(m);
  if (err != 0) {
    unmap_and_release(m);
    return ERR_PTR(err);
  }

  list_insert(l, &m->list);

  return m;
}
