#ifndef __BAFS_QUEUE_H_
#define __BAFS_QUEUE_H_


struct queue {
  uint16_t no;
  uint64_t es;
  uint32_t qs;
  uint64_t head;
  uint64_t tail;
  uint8_t  phase;
  volatile uint32_t* db;
  volatile void* addr;

} __attribute__((aligned(64)));


struct cpl {
  uint32_t dword[4];
  
} __attribute__((aligned(16)));

struct cmd {
  uint32_t dword[16];
  
} __attribute__((aligned(64)));


#endif