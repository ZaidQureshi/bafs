#ifndef __BAFS_NVME_ADMIN_H__
#define __BAFS_NVME_ADMIN_H__

#include "ctrl.h"
#include "../../common/queue.h"
#include <linux/spinlock.h>


#define MAX_CMD             65536

/*Refer section 5 for details*/
enum admin_cmd_opcode {
                       DEL_IO_SQ      = 0x0,
                       CRT_IO_SQ      = 0x1,
                       GET_LOG_PG     = 0x2,
                       DEL_IO_CQ      = 0x4,
                       CRT_IO_CQ      = 0x5,
                       ID             = 0x6,
                       ABORT          = 0x8,
                       SET_FEAT       = 0x9,
                       GET_FEAT       = 0xa,
                       ASYNC_EVNT_REQ = 0xc,
                       NS_MNGMT       = 0xd,
                       FIRM_COMMIT    = 0x10,
                       FIRM_IMG_DOWN  = 0x11,
                       DEV_SELF_TEST  = 0x14,
                       NM_ATTACH      = 0x15,
                       KEEP_ALIVE     = 0x18,
                       DIRECTIVE_SEND = 0x19,
                       DIRECTIVE_RECV = 0x1a,
                       VIRT_MNGMT     = 0x1c,
                       NVME_MI_SEND   = 0x1d,
                       NVME_MI_RECV   = 0x1e,
                       DB_BUF_CONFIG  = 0x7c,
                       FORMAT_NVM     = 0x80,
                       SECURITY_SEND  = 0x81,
                       SECURITY_RECV  = 0x82,
                       SANITIZE       = 0x84,
                       GET_LBA_STATUS = 0x86,

};


/* Manually creates an admin queue. Creation of admin queue will require resetting of controller
*  the steps followed to reset controller with appropriate register definitions can be 
*  read at section 7.6.1 of NVMe Spec rv1.4 June 2019. 
*/
void admin_init(struct admin_queue_pair* aqp, struct ctrl* c);


/*This removes the admin queue specific data structs*/
void admin_clean(struct admin_queue_pair* aqp);


/*This is a self test opcode for the controller.*/
void admin_dev_self_test(struct admin_queue_pair* aqp);

void admin_set_num_queues(struct admin_queue_pair* aqp);

struct queue_pair* admin_create_io_queue_pair(struct admin_queue_pair* aqp);

void admin_delete_io_queue_pair(struct admin_queue_pair* aqp, const u32 i);

#endif
