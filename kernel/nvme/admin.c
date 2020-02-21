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

  aqp->sq.q.head = 0;
  aqp->cq.q.head = 0;

  aqp->sq.q.tail = 0;
  aqp->cq.q.tail = 0;

  aqp->sq.q.es = sizeof(struct cmd);

  aqp->cq.q.es = sizeof(struct cpl);

  aqp->sq.q.no = 0;
  aqp->cq.q.no = 0;
  aqp->sq.q.qs = queue_size;
  aqp->cq.q.qs = queue_size;




  aqp->c = c;
  spin_lock_init(&aqp->lock);

  aqp->cq.q.addr = dma_alloc_coherent(&c->pdev->dev, queue_size * aqp->cq.q.es,
                                    &aqp->cq.q_dma_addr, GFP_KERNEL);
  aqp->sq.q.addr = dma_alloc_coherent(&c->pdev->dev, queue_size * aqp->sq.q.es,
                                    &aqp->sq.q_dma_addr, GFP_KERNEL);

  aqp->cq.q.mark = kmalloc(queue_size, GFP_KERNEL);
  aqp->sq.q.mark = kmalloc(queue_size, GFP_KERNEL);

  aqp->sq.q.cid = kmalloc(65536, GFP_KERNEL);
  memset(aqp->cq.q.mark, 0, queue_size);
  memset(aqp->sq.q.mark, 0, queue_size);
  memset(aqp->sq.q.cid, 0, 65536);

  aqp->cq.q.phase = 1;
  aqp->sq.q.phase = 1;

  aqp->sq.q.db = (volatile u32*)(((volatile u8*)c->reg_addr) + (0x1000));

  printk(KERN_INFO "sq db: %llx\n", aqp->sq.q.db);

  c->dstrd = ((volatile u32*)(&c->regs->CAP))[1];
  mpsmax   = (c->regs->CAP & 0x00ffffffffffffff) >> 52;

  c->timeout = (c->regs->CAP & 0x000000000fffffff) >> 24;

  c->page_size = 1 << (12 + mpsmax);


  cqes = ilog2(aqp->cq.q.es);
  sqes = ilog2(aqp->sq.q.es);
  ps   = ilog2(c->page_size);

  printk(KERN_INFO "cqes: %llu\tcq.q.es: %llu\tsqes: %llu\tsq.q.es: %llu\tps: %llu\tpage_size: %llu\tmpsmax: %llu\ttimeout: %llu\tcq_dma_addr: %p\tsq_dma_addr: %p\n",
         (unsigned long long) cqes, (unsigned long long) aqp->cq.q.es, (unsigned long long) sqes, (unsigned long long) aqp->sq.q.es,
         (unsigned long long) ps, (unsigned long long) c->page_size, (unsigned long long) mpsmax, (unsigned long long) c->timeout, aqp->cq.q.addr, aqp->sq.q.addr);

  aqp->cq.q.db = (volatile u32*)(((volatile u8*)aqp->sq.q.db) + (1 * (4 << c->dstrd)));







  *cc = *cc & ~1;
  barrier();

  while ((csts[0] & 0x01) != 0) {
    barrier();
  }

  printk(KERN_INFO "[admin_init] Finished first loop\n");
  c->regs->AQA = ((queue_size-1) << 16) | (queue_size-1);

  c->regs->ACQ = aqp->cq.q_dma_addr;

  c->regs->ASQ = aqp->sq.q_dma_addr;


  *cc = (cqes << 20) | (sqes << 16)| (mpsmax << 7)| 0x0001;

  barrier();

  while ((csts[0] & 0x01) != 1) {
    barrier();
  }
  c->regs->INTMS = 0xffffffff;
  c->regs->INTMC = 0x0;
  

  printk(KERN_INFO "[admin_init] finished second loop\n");

  admin_dev_self_test(aqp);
  admin_dev_self_test(aqp);
  admin_dev_self_test(aqp);
  admin_dev_self_test(aqp);
  admin_dev_self_test(aqp);

}


void admin_clean(struct admin_queue_pair* aqp) {
  dma_free_coherent(&aqp->c->pdev->dev, aqp->sq.q.es*aqp->sq.q.qs, (void*)aqp->sq.q.addr, aqp->sq.q_dma_addr);
  dma_free_coherent(&aqp->c->pdev->dev, aqp->cq.q.es*aqp->cq.q.qs, (void*)aqp->cq.q.addr, aqp->cq.q_dma_addr);
  kfree(aqp->cq.q.mark);
  kfree(aqp->sq.q.mark);
  kfree(aqp->sq.q.cid);
}


s32 admin_enqueue_command(struct admin_queue_pair* aqp, struct cmd* cmd_) {
  u16 loc;
  u64 cid;
  u32 i;

  cid = 65536;
  spin_lock(&aqp->lock);

  if (((u16) (aqp->sq.q.tail - aqp->sq.q.head) % aqp->sq.q.qs) == (aqp->sq.q.qs - 1)) {

    spin_unlock(&aqp->lock);
    return -1;
  }

  loc = aqp->sq.q.tail;

  for (i = 0; i < 65536; i++) {
    if (aqp->sq.q.cid[i] == 0) {
      aqp->sq.q.cid[i] = 1;
      cid              = i;
      break;
    }
  }

  if (cid == 65536) {
    spin_unlock(&aqp->lock);
    return -1;
  }

  

  cmd_->dword[0] |= (uint32_t)((cid << 16));

  *(((volatile struct cmd*)(aqp->sq.q.addr))+loc) = *cmd_;


  aqp->sq.q.tail = (aqp->sq.q.tail + 1) % aqp->sq.q.qs;


  barrier();
  *aqp->sq.q.db = aqp->sq.q.tail;
  barrier();
  spin_unlock(&aqp->lock);
  return loc;
}

void admin_sq_mark_cleanup(struct admin_queue_pair* aqp, const u16 loc) {
  u16         i;
  u16         l;
  u64         old_head;
  struct cmd* cmd_;
  u16         cid;
  spin_lock(&aqp->lock);
  cmd_ = (((struct cmd*)(aqp->sq.q.addr))+loc);
  cid                 = cmd_->dword[0] >> 16;
  aqp->sq.q.cid[cid]  = 0;
  aqp->sq.q.mark[loc] = 1;
  old_head            = aqp->sq.q.head;

  for (i = 0; true; i++) {
    l = (old_head + i) % aqp->sq.q.qs;

    if (aqp->sq.q.mark[l]) {
      aqp->sq.q.mark[l] = 0;
      aqp->sq.q.head = (aqp->sq.q.head + 1);
      if (aqp->sq.q.head == aqp->sq.q.qs) {
        aqp->sq.q.head    = 0;
        aqp->sq.q.phase   = !aqp->sq.q.phase;
      }
    }
    else {
      break;
    }
  }

  barrier();

  spin_unlock(&aqp->lock);
}

void admin_cq_mark_cleanup(struct admin_queue_pair* aqp, const u16 loc) {

  u16 i;
  u16 l;
  u64 old_head;
  spin_lock(&aqp->lock);

  aqp->cq.q.mark[loc] = 1;
  old_head = aqp->cq.q.head;

  for (i = 0; true; i++) {
    l = (old_head + i) % aqp->cq.q.qs;

    if (aqp->cq.q.mark[l]) {
      aqp->cq.q.mark[l] = 0;
      aqp->cq.q.head = (aqp->cq.q.head + 1);
      if (aqp->cq.q.head == aqp->cq.q.qs) {
        aqp->cq.q.head    = 0;
        aqp->cq.q.phase   = !aqp->cq.q.phase;
      }
    }
    else {
      break;
    }

  }

  if (old_head != aqp->cq.q.head) {
    *aqp->cq.q.db = aqp->cq.q.head;
  }

  barrier();

  spin_unlock(&aqp->lock);
}

s32 admin_cq_poll(struct admin_queue_pair* aqp, const u16 cid) {
  u32 i;
  u64 cur_head;
  u8  cur_phase;
  u32 l;
  u32 dword;
  u8  flipped;
  spin_lock(&aqp->lock);
  barrier();
  cur_phase                          = aqp->cq.q.phase;
  cur_head = aqp->cq.q.head;
  flipped = 0;
  for(i = 0; true; i++) {
    l = (cur_head + i) % aqp->cq.q.qs;
    dword                            = (((volatile struct cpl*)aqp->cq.q.addr)+l)->dword[3];
    if ((dword & 0x0000ffff)        == cid) {
      if ((!!((dword >> 16) & 0x1)) == cur_phase) {
        spin_unlock(&aqp->lock);
        return l;
      }
    }

    if (((cur_head + i + 1) >= aqp->cq.q.qs)) {
      break;
    }
  }

  spin_unlock(&aqp->lock);
  return -1;
}


void admin_dev_self_test(struct admin_queue_pair* aqp) {
  struct cmd cmd_;
  u16 i;
  s32 ret1, ret2;
  u32 cid;
  for (i = 0; i < 16; i++) {
    cmd_.dword[i] = 0;
  }

  cmd_.dword[0] = DEV_SELF_TEST;
  cmd_.dword[10] = 0x01;

  ret1 = admin_enqueue_command(aqp, &cmd_);
  while (ret1 == -1) {
    printk(KERN_INFO "[admin_dev_self_test] retry enqueuing\n");
    ret1 = admin_enqueue_command(aqp, &cmd_);
  }

  cid = cmd_.dword[0] >> 16;


  ret2 = admin_cq_poll(aqp, cid);

  while (ret2 == -1) {
    printk(KERN_INFO "[admin_dev_self_test] retry polling\n");
    ret2 = admin_cq_poll(aqp, cid);
  }
  printk("[admin_dev_self_test] found\n");
  admin_sq_mark_cleanup(aqp, ret1);
  admin_cq_mark_cleanup(aqp, ret2);
}
void admin_create_cq(struct admin_queue_pair* aqp) {
  spin_lock(&aqp->lock);


  spin_unlock(&aqp->lock);
}
