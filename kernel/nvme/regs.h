#ifndef __BAFS_NVME_REGS_H_
#define __BAFS_NVME_REGS_H_



#define _REG(p, offs, bits)                     \
    ((volatile uint##bits##_t *) (((volatile unsigned char*) ((volatile void*) (p))) + (offs)))


#define RES1_START 0x18
#define RES1_END   0x1b
#define RES1_SIZE  (RES1_END - RES1_START + 1)

#define RES2_START 0x5c
#define RES2_END   0xdff
#define RES2_SIZE (RES2_END - RES2_START + 1)

#define RES3_START 0xe1c
#define RES3_END   0xfff
#define RES3_SIZE (RES3_END - RES3_START + 1)


struct nvme_regs {
  u64 CAP;
  u32 VS;
  u32 INTMS;
  u32 INTMC;
  u32 CC;
  u32 RES1[RES1_SIZE/sizeof(u32)];
  u32 CSTS;
  u32 NSSR;
  u32 AQA;
  u64 ASQ;
  u64 ACQ;
  u32 CMBLOC;
  u32 CMBSZ;
  u32 BPINFO;
  u32 BPRSEL;
  u32 BPMBL;
  u64 CMBMSC;
  u32 CMBSTS;
  u32 RES2[RES2_SIZE/sizeof(u32)];
  u32 PMRCAP;
  u32 PMRCTL;
  u32 PMRSTS;
  u32 PMREBS;
  u32 PMRSWTP;
  u32 PMRMSC;
  u32 RES3[RES3_SIZE/sizeof(u32)];
  u32 SQ0TDBL;

};


#endif                          // __BAFS_NVME_REGS_H_
