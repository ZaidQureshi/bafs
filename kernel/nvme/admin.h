#ifndef __BAFS_NVME_ADMIN_H__
#define __BAFS_NVME_ADMIN_H__

#include "ctrl.h"
#include "../../common/queue.h"

#include <linux/spinlock.h>


struct admin_queue_pair {
  struct queue cq;
  struct queue sq;
  spinlock_t       lock;
  struct ctrl* c;

  dma_addr_t cq_dma_addr;
  dma_addr_t sq_dma_addr;

};


void admin_init(struct admin_queue_pair* aqp, struct ctrl* c);

void admin_clean(struct admin_queue_pair* aqp);

#endif
