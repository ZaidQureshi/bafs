#include "admin.h"
#include "map.h"
#include <linux/spinlock.h>
#include <linux/log2.h>
#include <linux/delay.h>
#include "../../common/bafs_error.h"
#include "../../common/bafs_utils.h"

void admin_init(struct admin_queue_pair* aqp, struct ctrl* c) {
  u32                cqes;
  u32                sqes;
  u32                ps;
  u32                mpsmax;
  struct queue_pair* new_qp;
//Create pointers to Controller configuration and controller status registers
  volatile u32*      cc   = &c->regs->CC;
  volatile u32*      csts = &c->regs->CSTS;

//Read the maximum individual IO queue size supported by the controller. +1 is added for easier check
  //u32 queue_size       = ((volatile u16*)(&c->regs->CAP[0]))[0] + 1;
  u32 queue_size         = _RDBITS(c->regs->CAP[0], 15, 0) + 1;
  aqp->c                 = c;
  aqp->c->max_queue_size = queue_size;
  queue_size             = queue_size > 4096 ? 4096 : queue_size;

//initialize admin sq and cq data structs
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



  aqp->num_io_queue_pairs_used = 0;
  spin_lock_init(&aqp->lock);

//TODO: merge the DMA allocs. Change in Free too. 
  aqp->cq.q.addr = dma_alloc_coherent(&c->pdev->dev, queue_size * aqp->cq.q.es,
                                      &aqp->cq.q_dma_addr, GFP_KERNEL);
  aqp->sq.q.addr = dma_alloc_coherent(&c->pdev->dev, queue_size * aqp->sq.q.es,
                                      &aqp->sq.q_dma_addr, GFP_KERNEL);


  aqp->cq.q.mark = kmalloc(queue_size, GFP_KERNEL);
  aqp->sq.q.mark = kmalloc(queue_size, GFP_KERNEL);


  aqp->sq.q.cid = kmalloc(MAX_CMD, GFP_KERNEL);
  memset(aqp->cq.q.mark, 0, queue_size);
  memset(aqp->sq.q.mark, 0, queue_size);
  memset(aqp->sq.q.cid, 0, MAX_CMD);

  aqp->cq.q.phase = 1;
  aqp->sq.q.phase = 1;


  //TODO: can we make the below to make it readable? It seems like we have a bug in the regs data struct. Revisit. 
  //aqp->sq.q.db = (volatile u32*)(&c->regs->SQ0TDBL);
  aqp->sq.q.db   = (volatile u32*)(((volatile u8*)c->reg_addr) + (0x1000));

  //printk(KERN_INFO "sq db: %llx\n", aqp->sq.q.db);

  //read the doorbel register stide
  c->dstrd   = _RDBITS(c->regs->CAP[1], (35-32), (32-32));

  //printk(KERN_WARNING "DSTRD: %lx\n", c->dstrd);
  //read the min page size
  //mpsmin = (c->regs->CAP & 0x00ffffffffffffff) >> 52;
  mpsmin   = _RDBITS(c->regs->CAP[1], (51-32), (48-32));

  c->timeout   = _RDBITS(c->regs->CAP[0], 31, 24);
  //c->timeout = (c->regs->CAP & 0x00000000ffffffff) >> 24;

  c->page_size = 1 << (12 + mpsmin);


  cqes = ilog2(aqp->cq.q.es);
  sqes = ilog2(aqp->sq.q.es);
  ps   = ilog2(c->page_size);

  printk(KERN_INFO "qs: %llu\t cqes: %llu\tcq.q.es: %llu\tsqes: %llu\tsq.q.es: %llu\tps: %llu\tpage_size: %llu\tmpsmin: %llu\ttimeout: %llu\tcq_dma_addr: %llx\tsq_dma_addr: %llx\nMPSMAX: %llu\tMPSMIN: %llu\n",
         (unsigned long long) queue_size,
         (unsigned long long) cqes, (unsigned long long) aqp->cq.q.es, (unsigned long long) sqes, (unsigned long long) aqp->sq.q.es,
         (unsigned long long) ps, (unsigned long long) c->page_size, (unsigned long long) mpsmin, (unsigned long long) c->timeout, aqp->cq.q_dma_addr, aqp->sq.q_dma_addr,
          (unsigned long long) (_RDBITS(c->regs->CAP[1], (55-32), (52-32))), (unsigned long long) (_RDBITS(c->regs->CAP[1], (51-32), (48-32))));

  aqp->cq.q.db = (volatile u32*)(((volatile u8*)aqp->sq.q.db) + (1 * (4 << c->dstrd)));



  /*The next set of steps initializes the nvme controller. 
   * Refer section 7.6.1 of v1.4 June 2019 spec for the complete step details.
   */

  //reset the controller
  *cc = *cc & ~1;
  barrier();
  //wait for the status bit to go high
  while ((csts[0] & 0x01) != 0) {
    barrier();
  }

  printk(KERN_INFO "[admin_init] Finished first loop\n");
  
  //Program the admin queue 
  c->regs->AQA = ((queue_size-1) << 16) | (queue_size-1);

  c->regs->ACQ[0] = (u32) aqp->cq.q_dma_addr;
  c->regs->ACQ[1] = (u32) (aqp->cq.q_dma_addr >> 32);

  c->regs->ASQ[0] = (u32) aqp->sq.q_dma_addr;
  c->regs->ASQ[1] = (u32) (aqp->sq.q_dma_addr >> 32);

  //program the CQ, SQ and Max page size and reset the controller. 
  // by default we have round robin arbitration scheme for SQ-CQ pair. 
  *cc = (cqes << 20) | (sqes << 16)| (mpsmin << 7)| 0x0001;

  barrier();

  //wait for the status bit to go high
  while ((csts[0] & 0x01) != 1) {
    barrier();
  }

  //disable interrupts of the device
  c->regs->INTMS = 0xffffffff;
  c->regs->INTMC = 0x0;
  
  //msleep(20000);
  printk(KERN_INFO "[admin_init] finished second loop addr: %llx \n", (uint64_t)(&c->regs->ASQ));

  admin_set_num_queues(aqp);

  aqp->queue_use_mark = kmalloc(aqp->num_io_queue_pairs_supported, GFP_KERNEL);

  aqp->io_qp_list = kmalloc(aqp->num_io_queue_pairs_supported * sizeof(struct queue_pair*), GFP_KERNEL);

  memset(aqp->io_qp_list, 0, aqp->num_io_queue_pairs_supported * sizeof(struct queue_pair*));
  memset(aqp->queue_use_mark, 0, aqp->num_io_queue_pairs_supported);

  aqp->sq_dma_addrs = kmalloc(aqp->num_io_queue_pairs_supported * sizeof(dma_addr_t), GFP_KERNEL);
  aqp->cq_dma_addrs = kmalloc(aqp->num_io_queue_pairs_supported * sizeof(dma_addr_t), GFP_KERNEL);

  new_qp = admin_create_io_queue_pair(aqp);

  

  list_init(&aqp->host_io_list);
  
}

struct iobuf* iobuf_find(const struct list* l, u64 vaddr) {
  const struct list_node* e = list_next(&l->head);
  struct iobuf*             m = NULL;

  while (e != NULL) {
    m = container_of(e, struct iobuf, list);
    if ((m->map->owner == current)              &&
        ((m->virt_addr == (void*)(vaddr & PAGE_MASK)) ||
         (m->virt_addr == (void*)(vaddr & GPU_PAGE_MASK)))) {
      return m;
    }
    e = list_next(e);
  }

  return NULL;
}


void delete_host_io_buf(struct admin_queue_pair* aqp, const void* vaddr) {
  struct iobuf* io;

  io = iobuf_find(&aqp->host_io_list, (u64) vaddr);

  list_remove(&io->list);


  dma_free_coherent(&aqp->c->pdev->dev, io->num_entries*io->pages_per_entry*io->page_size, (void*)io->prp_list_phys, *(io->prp_list_dma_addr));

  unmap_and_release_map(io->map);

  kfree(io);



}

unsigned long clear_host_io_list(struct admin_queue_pair* aqp, struct list* l) {
  unsigned long i = 0;
  struct list_node* ptr = list_next(&l->head);
  struct iobuf* io;

  while(ptr != NULL) {
    io = container_of(ptr, struct iobuf, list);
    dma_free_coherent(&aqp->c->pdev->dev, io->num_entries*io->pages_per_entry*io->page_size, (void*)io->prp_list_phys, *(io->prp_list_dma_addr));
    unmap_and_release_map(io->map);
    kfree(io);

    i++;
    ptr = list_next(&l->head);
  }
  return i;
}


/*Removes the entires of the admin queue from the host kernel*/

//TODO: merge the DMA allocs. Change in Free too.
void admin_clean(struct admin_queue_pair* aqp) {
  unsigned long remaining = 0;
  u32 i;
  for(i = 0; i < aqp->num_io_queue_pairs_supported; i++) {
    admin_delete_io_queue_pair(aqp, i);
  }
  dma_free_coherent(&aqp->c->pdev->dev, aqp->sq.q.es*aqp->sq.q.qs, (void*)aqp->sq.q.addr, aqp->sq.q_dma_addr);
  dma_free_coherent(&aqp->c->pdev->dev, aqp->cq.q.es*aqp->cq.q.qs, (void*)aqp->cq.q.addr, aqp->cq.q_dma_addr);
  kfree(aqp->queue_use_mark);
  kfree(aqp->io_qp_list);
  kfree(aqp->sq_dma_addrs);
  kfree(aqp->cq_dma_addrs);
  kfree(aqp->cq.q.mark);
  kfree(aqp->sq.q.mark);
  kfree(aqp->sq.q.cid);

  remaining = clear_host_io_list(aqp, &aqp->host_io_list);
  if (remaining != 0 ) {
    printk(KERN_WARNING "[admin clean] %lu host io bufs were still in use on unload\n", remaining);
  }

}




struct iobuf* create_host_io_buf(struct admin_queue_pair* aqp, const u64 page_size, const u64 pages_per_entry, const u64 num_entries, void* vaddr) {
  struct map* m;
  struct iobuf* io;
  u32 i,j;
  u64* cur_entry;

  const u64 n_pages = (page_size*pages_per_entry*num_entries) / PAGE_SIZE;
  
  m = map_userspace(aqp->c, (u64) vaddr, n_pages);

  if (!IS_ERR_OR_NULL(m)) {
    io =  (struct iobuf*) kcalloc(1, sizeof(struct iobuf*), GFP_KERNEL);
    io->map = m;
    io->page_size = page_size;
    io->pages_per_entry = pages_per_entry;
    io->num_entries = num_entries;
    io->virt_addr = (void*)m->vaddr;
    io->prp_list_phys = dma_alloc_coherent(&aqp->c->pdev->dev,
                                           num_entries*PAGE_SIZE,
                                           io->prp_list_dma_addr,
                                           GFP_KERNEL);
    for(i = 0; i < num_entries; i++) {
      cur_entry = io->prp_list_phys + (i * (PAGE_SIZE/sizeof(u64)));
      cur_entry[(PAGE_SIZE/sizeof(u64))-1] = m->addrs[(i*pages_per_entry)];
      for(j = 1; j < pages_per_entry; j++) {
        cur_entry[j-1] = m->addrs[(i*pages_per_entry) + j];
      }
    }
    list_insert(&aqp->host_io_list, &io->list);
    return io;
  }
  else {
    return (struct iobuf*)PTR_ERR(m);
  }
}




/*Submit a commnad to the admin SQ and ring the doorbel.*/

s32 admin_enqueue_command(struct admin_queue_pair* aqp, struct cmd* cmd_) {
  
  u16                                              loc;
  u64                                              cid;
  u32                                              i;

  cid = MAX_CMD;
  spin_lock(&aqp->lock);

  //if queue full, wait
  if (((u16) (aqp->sq.q.tail - aqp->sq.q.head) % aqp->sq.q.qs) == (aqp->sq.q.qs - 1)) {

    spin_unlock(&aqp->lock);
    return -1;
  }
  //get loc to write
  loc = aqp->sq.q.tail;

  for (i = 0; i < MAX_CMD; i++)
  {
    if (aqp->sq.q.cid[i] == 0) {
      aqp->sq.q.cid[i] = 1;
      cid              = i;
      break;
    }
  }

  if (cid == MAX_CMD)
  {
    spin_unlock(&aqp->lock);
    return -1;
  }

  //create cmd
  cmd_->dword[0] |= (uint32_t)((cid << 16));

  *(((volatile struct cmd*)(aqp->sq.q.addr))+loc) = *cmd_;
  for(i = 0; i < (sizeof(struct cmd)/(sizeof(uint32_t))); ++i){
    printk(KERN_INFO "[admin_enqueue_command] cmd dword: %d val: %x \n", i, cmd_->dword[i]);
  }

  //increament tail ptr if successful enqueue
  aqp->sq.q.tail = (aqp->sq.q.tail + 1) % aqp->sq.q.qs;

  //write db with the next tail ptr to tell that the device can read the cmd
  barrier();
  *aqp->sq.q.db = aqp->sq.q.tail;
  barrier();
  spin_unlock(&aqp->lock);
  return loc;
}

void admin_sq_mark_cleanup(struct admin_queue_pair* aqp, const u16 loc) {
  u16                                               i;
  u16                                               l;
  u64                                               old_head;
  struct cmd*                                       cmd_;
  u16                                               cid;
  
  spin_lock(&aqp->lock);

  cmd_                = (((struct cmd*)(aqp->sq.q.addr))+loc);
  cid                 = cmd_->dword[0] >> 16;
  aqp->sq.q.cid[cid]  = 0;
  aqp->sq.q.mark[loc] = 1;
  old_head            = aqp->sq.q.head;

  for (i = 0; true; i++) {
    l = (old_head + i) % aqp->sq.q.qs;

    if (aqp->sq.q.mark[l]) {
      aqp->sq.q.mark[l] = 0;
      aqp->sq.q.head    = (aqp->sq.q.head + 1);
      if (aqp->sq.q.head == aqp->sq.q.qs) {
        aqp->sq.q.head  = 0;
        aqp->sq.q.phase = !aqp->sq.q.phase;
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
  old_head            = aqp->cq.q.head;

  for (i = 0; true; i++) {
    l = (old_head + i) % aqp->cq.q.qs;

    if (aqp->cq.q.mark[l]) {
      aqp->cq.q.mark[l] = 0;
      aqp->cq.q.head    = (aqp->cq.q.head + 1);
      if (aqp->cq.q.head == aqp->cq.q.qs) {
        aqp->cq.q.head  = 0;
        aqp->cq.q.phase = !aqp->cq.q.phase;
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
  u32                                      i;
  u64                                      cur_head;
  u8                                       cur_phase;
  u32 l;
  u32                                      dword;
  u8                                       flipped;
  u32 status;
  spin_lock(&aqp->lock);
  barrier();
  cur_phase                          = aqp->cq.q.phase;
  cur_head                           = aqp->cq.q.head;
  flipped                            = 0;
  for(i                              = 0; true; i++) {
    l                                = (cur_head + i) % aqp->cq.q.qs;
    dword                            = (((volatile struct cpl*)aqp->cq.q.addr)+l)->dword[3];
    status                           = dword >> 16;
    if ((dword & 0x0000ffff)        == cid) {
      if ((!!((dword >> 16) & 0x1)) == cur_phase) {
        printk(KERN_INFO "[admin_cq_poll] status field: %lx: [%s]", (long unsigned int) (status>>1), bafs_error(status>>1));
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

  cmd_.dword[0]  = DEV_SELF_TEST;
  cmd_.dword[10] = 0x01;

  ret1         = admin_enqueue_command(aqp, &cmd_);
  while (ret1 == -1) {
    printk(KERN_INFO "[admin_dev_self_test] retry enqueuing\n");
    ret1       = admin_enqueue_command(aqp, &cmd_);
  }

  cid = cmd_.dword[0] >> 16;


  ret2         = admin_cq_poll(aqp, cid);
  while (ret2 == -1) {
    ret2 = admin_cq_poll(aqp, cid);
  }
  printk("[admin_dev_self_test] found\n");
  admin_sq_mark_cleanup(aqp, ret1);
  admin_cq_mark_cleanup(aqp, ret2);
}

void clear_cmd(struct cmd* cmd_) {
  u16 i;
  for (i           = 0; i < 16; i++) {
    cmd_->dword[i] = 0;
  }
}

void admin_set_num_queues(struct admin_queue_pair* aqp) {
  struct cmd cmd_;
  struct cpl cpl_;
  //u16      i;
  s32        ret1, ret2;
  u32 cid;
  clear_cmd(&cmd_);

  cmd_.dword[0] = SET_FEAT;     //0xCF is an opcode
  cmd_.dword[10] = 0x07;

  cmd_.dword[11] = (((u32)65534) << 16) | 65534;

  ret1 = admin_enqueue_command(aqp, &cmd_);
  while (ret1 == -1) {
    printk(KERN_INFO "[admin_set_num_queues] retry enqueuing\n");
    ret1 = admin_enqueue_command(aqp, &cmd_);
  }

  cid = cmd_.dword[0] >> 16;


  ret2   = admin_cq_poll(aqp, cid);
  while (ret2 == -1) {
    ret2 = admin_cq_poll(aqp, cid);
  }

  cpl_ = *(((volatile struct cpl*) aqp->cq.q.addr) + ret2);
  aqp->num_io_queue_pairs_supported = (cpl_.dword[0] >> 16) + 1;
  printk(KERN_INFO "Num IO CQ: %llu\tNum IO SQ: %llu\n",
         (unsigned long long) (cpl_.dword[0] >> 16), (unsigned long long) (cpl_.dword[0] & 0x0000ffff));
  printk("[admin_set_num_queues] found\n");
  admin_sq_mark_cleanup(aqp, ret1);
  admin_cq_mark_cleanup(aqp, ret2);
}


void admin_fpga_config(struct admin_queue_pair* aqp){
  struct cmd cmd_;
  struct cpl cpl_;

  s32 ret1, ret2;
  u32 cid; 
  clear_cmd(&cmd_);

  cmd_.dword[0] = 0xF0;         // BAFS specifc opcode
  cmd_.dword[1] = 0x2E5457E3;   //use for IO buffer address
  cmd_.dword[2] = 0xBADA55F1;   //use for IO buffer address
  cmd_.dword[3] = 0;            //use for IO buffer stride
  cmd_.dword[4] = 0;            //use for SQ base address
  cmd_.dword[5] = 0;            //use for SQ base address
  cmd_.dword[6] = 0;            //entry size or page size
  cmd_.dword[7]  = 0;           //number of entries
  cmd_.dword[8]  = 0;           //offset - base address of PRPlist

  ret1         = admin_enqueue_command(aqp, &cmd_);
  while (ret1 == -1) {
    printk(KERN_INFO "[admin_fpga_config] retry enqueuing\n");
    ret1 = admin_enqueue_command(aqp, &cmd_);
  }

  cid = cmd_.dword[0] >> 16;

  ret2   = admin_cq_poll(aqp, cid);
  while (ret2 == -1) {
    ret2 = admin_cq_poll(aqp, cid);
  }
  cpl_ = *(((volatile struct cpl*) aqp->cq.q.addr) + ret2);
  aqp->num_io_queue_pairs_supported = (cpl_.dword[0] >> 16) + 1;
  //printk(KERN_INFO "Num IO CQ: %llu\tNum IO SQ: %llu\n",
  //       (unsigned long long) (cpl_.dword[0] >> 16), (unsigned long long) (cpl_.dword[0] & 0x0000ffff));
  printk("[admin_fpga_config] complete\n");
  admin_sq_mark_cleanup(aqp, ret1);
  admin_cq_mark_cleanup(aqp, ret2);

}




struct queue_pair* admin_create_io_queue_pair(struct admin_queue_pair* aqp) {
  struct cmd cmd_;
  struct cpl cpl_;
  u32 i;
  s32 ret1, ret2;
  u32 cid;
  //u64      addr;
  u32        qs;
  struct queue_pair* new_qp;
  clear_cmd(&cmd_);
  
  spin_lock(&aqp->lock);

  if (aqp->num_io_queue_pairs_used == aqp->num_io_queue_pairs_supported) {
    spin_unlock(&aqp->lock);
    return NULL;
  }
  aqp->num_io_queue_pairs_used++;
  for (i                        = 0; i < aqp->num_io_queue_pairs_supported; i++) {
    if (aqp->queue_use_mark[i] == 0) {
      aqp->queue_use_mark[i] = 1;
      break;
    }
  }
  barrier();
  spin_unlock(&aqp->lock);
  if (i == aqp->num_io_queue_pairs_supported) {
    return NULL;
  }
  qs                 = (aqp->c->max_queue_size > 4096 ? 4096 : aqp->c->max_queue_size);
  aqp->io_qp_list[i] = kmalloc(sizeof(struct queue_pair), GFP_KERNEL);
  new_qp             = aqp->io_qp_list[i];


  new_qp->cq.es = aqp->cq.q.es;
  new_qp->cq.qs = qs;

  new_qp->cq.addr = dma_alloc_coherent(&aqp->c->pdev->dev,
                                       qs * new_qp->cq.es,
                                       aqp->cq_dma_addrs+i,
                                       GFP_KERNEL);


  new_qp->cq.db = (volatile u32*)(((volatile u8*)aqp->cq.q.db) + ((2*(i+1) + 1) * (4 << aqp->c->dstrd)));



  cmd_.dword[0]  = CRT_IO_CQ;
  cmd_.dword[6]  = aqp->cq_dma_addrs[i];
  cmd_.dword[7]  = aqp->cq_dma_addrs[i] >> 32;
  cmd_.dword[10] = ((qs - 1) << 16) | (i + 1);
  cmd_.dword[11] = 0x01;


  ret1   = admin_enqueue_command(aqp, &cmd_);
  while (ret1 == -1) {
    printk(KERN_INFO "[admin_create_io_queue_pair] retry enqueuing\n");
    ret1 = admin_enqueue_command(aqp, &cmd_);
  }

  cid = cmd_.dword[0] >> 16;


  ret2 = admin_cq_poll(aqp, cid);
  while (ret2 == -1) {
    ret2 = admin_cq_poll(aqp, cid);
  }

  cpl_ = *(((volatile struct cpl*) aqp->cq.q.addr) + ret2);

  admin_sq_mark_cleanup(aqp, ret1);
  admin_cq_mark_cleanup(aqp, ret2);

  clear_cmd(&cmd_);




  new_qp->sq.db = (volatile u32*)(((volatile u8*)aqp->sq.q.db) + ((2*(i+1)) * (4 << aqp->c->dstrd)));

  new_qp->sq.es = aqp->sq.q.es;
  new_qp->sq.qs = qs;

  new_qp->sq.addr = dma_alloc_coherent(&aqp->c->pdev->dev,
                                       qs * new_qp->sq.es,
                                       aqp->sq_dma_addrs+i,
                                       GFP_KERNEL);


  cmd_.dword[0]  = CRT_IO_SQ;   // opcode for creation of IO SQ queue
  cmd_.dword[6]  = aqp->sq_dma_addrs[i]; //IO Queue Address for each pair
  cmd_.dword[7]  = aqp->sq_dma_addrs[i] >> 32;
  cmd_.dword[10] = ((qs - 1) << 16) | (i + 1);
  cmd_.dword[11] = ((i+1) << 16) | 0x01;


  ret1         = admin_enqueue_command(aqp, &cmd_);
  while (ret1 == -1) {
    printk(KERN_INFO "[admin_create_io_queue_pair] retry enqueuing\n");
    ret1       = admin_enqueue_command(aqp, &cmd_);
  }

  cid = cmd_.dword[0] >> 16;


  ret2         = admin_cq_poll(aqp, cid);
  while (ret2 == -1) {
    ret2       = admin_cq_poll(aqp, cid);
  }

  cpl_ = *(((volatile struct cpl*) aqp->cq.q.addr) + ret2);

  admin_sq_mark_cleanup(aqp, ret1);
  admin_cq_mark_cleanup(aqp, ret2);


  new_qp->sq.head = 0;
  new_qp->cq.head = 0;

  new_qp->sq.tail = 0;
  new_qp->cq.tail = 0;

  new_qp->sq.no = i+1;
  new_qp->cq.no = i+1;
  return new_qp;
}


void admin_delete_io_queue_pair(struct admin_queue_pair* aqp, const u32 i) {

  struct cmd cmd_;
  //struct cpl cpl_;
  u32        cid;
  s32 ret1, ret2;
  struct queue_pair* del_qp = aqp->io_qp_list[i];
  spin_lock(&aqp->lock);
  if (aqp->queue_use_mark[i] == 0) {
    spin_unlock(&aqp->lock);
    return;
  }
  spin_unlock(&aqp->lock);
  clear_cmd(&cmd_);
  cmd_.dword[0]  = DEL_IO_SQ;
  cmd_.dword[10] = i+1;


  ret1 = admin_enqueue_command(aqp, &cmd_);
  while (ret1 == -1) {
    printk(KERN_INFO "[admin_delete_io_queue_pair] retry enqueuing\n");
    ret1 = admin_enqueue_command(aqp, &cmd_);
  }

  cid = cmd_.dword[0] >> 16;


  ret2         = admin_cq_poll(aqp, cid);
  while (ret2 == -1) {
    ret2       = admin_cq_poll(aqp, cid);
  }
  printk("[admin_delete_io_queue_pair] found\n");
  admin_sq_mark_cleanup(aqp, ret1);
  admin_cq_mark_cleanup(aqp, ret2);
  
  clear_cmd(&cmd_);
  cmd_.dword[0]  = DEL_IO_CQ;
  cmd_.dword[10] = i+1;

  ret1 = admin_enqueue_command(aqp, &cmd_);
  while (ret1 == -1) {
    printk(KERN_INFO "[admin_delete_io_queue_pair] retry enqueuing\n");
    ret1       = admin_enqueue_command(aqp, &cmd_);
  }

  cid = cmd_.dword[0] >> 16;


  ret2         = admin_cq_poll(aqp, cid);
  while (ret2 == -1) {
    ret2       = admin_cq_poll(aqp, cid);
  }
  printk("[admin_delete_io_queue_pair] found\n");
  admin_sq_mark_cleanup(aqp, ret1);
  admin_cq_mark_cleanup(aqp, ret2);

  dma_free_coherent(&aqp->c->pdev->dev,
                    del_qp->cq.qs * del_qp->cq.es,
                    (void*)del_qp->cq.addr,
                    aqp->cq_dma_addrs[i]);

  dma_free_coherent(&aqp->c->pdev->dev,
                    del_qp->sq.qs * del_qp->sq.es,
                    (void*)del_qp->sq.addr,
                    aqp->sq_dma_addrs[i]);

  kfree(aqp->io_qp_list[i]);

  spin_lock(&aqp->lock);
  aqp->queue_use_mark[i] = 0;
  barrier();
  spin_unlock(&aqp->lock);

}


