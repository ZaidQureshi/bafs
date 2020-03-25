#ifndef __BAFS_IOBUF_H__
#define __BAFS_IOBUF_H__


#include "../kernel/nvme/map.h"
#include "../kernel/nvme/list.h"

struct iobuf {
    void* dma_addr;
    void* virt_addr;
    uint64_t page_size;
    uint64_t pages_per_entry;
    uint64_t num_entries;
    uint64_t* prp_list_phys;
    dma_addr_t* prp_list_dma_addr;

#ifdef KERN
    struct list_node list;
    struct map* map;
#endif
};





#endif // __BAFS_IOBUF_H__
