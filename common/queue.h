#ifndef __BAFS_QUEUE_H_
#define __BAFS_QUEUE_H_

#ifdef KERN
#include "../kernel/nvme/list.h"
#endif

struct queue {
  uint16_t           no;    // queue number
  uint64_t           es;    // entry size is in bytes
  uint32_t           qs;    // queue size in number of entries
  uint64_t           head;  // head ptr
  uint64_t           tail;  // tail ptr
  uint8_t            phase; // phase bit - toggles 
  volatile uint32_t* db;    // doorbell register
  volatile void*     addr;  // addr of the queue
  uint8_t*           mark;  // mark is a sentinal value used to garbage collection on queues. SQ and CQ both uses it. 
  uint8_t*           cid;   // byte array - where every element represents availablity of the cid - 0 means available and 1 means taken. Only SQ uses. 


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
