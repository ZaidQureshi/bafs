#include "admin.h"
#include <linux/spinlock.h>
#include <linux/log2.h>

void admin_init(struct admin_queue_pair* aqp, struct ctrl* c) {



  volatile u32* cc = &c->regs->CC;

  volatile u32* csts = &c->regs->CSTS;

  u32 queue_size = ((volatile u16*)(&c->regs->CAP))[0] + 1;
  u32 cqes;
  u32 sqes;
  u32 ps;
  u32 mpsmax;

  aqp->sq.es = sizeof(struct cmd);

  aqp->cq.es = sizeof(struct cpl);

  aqp->sq.no = 0;
  aqp->cq.no = 0;
  aqp->sq.qs = queue_size;
  aqp->cq.qs = queue_size;




  aqp->c = c;
  spin_lock_init(&aqp->lock);

  aqp->cq.addr = dma_alloc_coherent(&c->pdev->dev, queue_size * aqp->cq.es,
                                    &aqp->cq_dma_addr, GFP_KERNEL);
  aqp->sq.addr = dma_alloc_coherent(&c->pdev->dev, queue_size * aqp->sq.es,
                                    &aqp->sq_dma_addr, GFP_KERNEL);



  aqp->sq.db = &c->regs->SQ0TDBL;

  c->dstrd = ((volatile u32*)(&c->regs->CAP))[1];
  mpsmax   = (c->regs->CAP & 0x00ffffffffffffff) >> 52;

  c->timeout = (c->regs->CAP & 0x000000000fffffff) >> 24;

  c->page_size = 1 << (12 + mpsmax);


  cqes = ilog2(aqp->cq.es);
  sqes = ilog2(aqp->sq.es);
  ps   = ilog2(c->page_size);

  printk(KERN_INFO "cqes: %llu\tcq.es: %llu\tsqes: %llu\tsq.es: %llu\tps: %llu\tpage_size: %llu\tmpsmax: %llu\ttimeout: %llu\n",
         (unsigned long long) cqes, (unsigned long long) aqp->cq.es, (unsigned long long) sqes, (unsigned long long) aqp->sq.es,
         (unsigned long long) ps, (unsigned long long) c->page_size, (unsigned long long) mpsmax, (unsigned long long) c->timeout);

  aqp->sq.db = (volatile u32*)(((volatile u8*)aqp->sq.db) + (1 * (4 << c->dstrd)));







  *cc = *cc & ~1;
  barrier();

  while ((csts[0] & 0x01) != 0) {
    barrier();
  }

  printk(KERN_INFO "[admin_init] Finished first loop\n");
  c->regs->AQA = ((queue_size-1) << 16) | (queue_size-1);

  c->regs->ACQ = aqp->cq_dma_addr;

  c->regs->ASQ = aqp->sq_dma_addr;

  *cc = (cqes << 20) | (sqes << 16)| (mpsmax << 7)| 0x0001;

  barrier();

  while ((csts[0] & 0x01) != 1) {
    barrier();
  }
  

  printk(KERN_INFO "[admin_init] finished second loop\n");
  
}


void admin_clean(struct admin_queue_pair* aqp) {
  dma_free_coherent(&aqp->c->pdev->dev, aqp->sq.es*aqp->sq.qs, (void*)aqp->sq.addr, aqp->sq_dma_addr);
  dma_free_coherent(&aqp->c->pdev->dev, aqp->cq.es*aqp->cq.qs, (void*)aqp->cq.addr, aqp->cq_dma_addr);

}
