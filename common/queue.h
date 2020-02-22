#ifndef __BAFS_QUEUE_H_
#define __BAFS_QUEUE_H_

#ifdef KERN
#include "../kernel/nvme/list.h"
#endif

//TODO: What is the queue struct here ? What is mark refer to? and why are we usin git for admin queues?
struct queue {
  uint16_t           no;
  uint64_t           es;
  uint32_t           qs;
  uint64_t           head;
  uint64_t           tail;
  uint8_t            phase;
  volatile uint32_t* db;
  volatile void*     addr;
  uint8_t*           mark;
  uint8_t*           cid;


} __attribute__((aligned(64)));


#ifdef KERN
struct queue_k {
  struct queue q;
  struct list_node list;
  spinlock_t lock;
  dma_addr_t q_dma_addr;
};
#endif

struct cpl {
  uint32_t dword[4];
  
} __attribute__((aligned(16)));

struct cmd {
  uint32_t dword[16];
  
} __attribute__((aligned(64)));


#endif
