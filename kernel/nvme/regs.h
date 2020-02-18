#ifndef __BAFS_NVME_REGS_H_
#define __BAFS_NVME_REGS_H_

struct nvme_regs {
  u64  CAP;
  u32  VS;
  u32 INTMS;
  u32 INTMC;
  u32 CC;
  u32 RES1;
  u32 CSTS;
  u32 NSSR;
  u32 AQA;
  u64 ASQ;
  u64 ACQ;

};


#endif // __BAFS_NVME_REGS_H_
