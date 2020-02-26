#ifndef __BAFS_UTILS_H_
#define __BAFS_UTILS_H_


/*Create lower bit mask - example bit mask for 12 is 0xFFF*/ 
#define _BIT_MASK(num_bits) ((1ULL << (num_bits)) - 1)

/*Extract single bit - example for 12 is 0x1000*/
#define _SBIT_MASK(num_bits) ((1ULL << (num_bits)))

/*Extract range of bits - example from 10 to 12. i.e bit 10, 11 and 12*/
#define _RBIT_MASK(high,low) (_BIT_MASK((high) + 1) - _BIT_MASK(low))

/*Extract range of bits from register/value*/
#define _RDBITS(val, high, low) (((val) & _RBIT_MASK((high),(low))) >> (low))

//TODO: Can this be converted to a constexpr?
/*Read register value*/
//example: Read ptr[31:16] --> _REG(ptr, 2, 16) -- 2 is the start position of offset and 16 is size of read
#define _REG(regptr, offset, type) \
    ((volatile uint##type##_t *) (((volatile unsigned char *) ((volatile void*)(regptr))) + (offset)))


/*Common feilds across different commands */
//Assumption: ptr is base ptr of the cmd struct
#define BAFS_IOCMD_CID(ptr)     _REG(ptr, 2, 16) 
#define BAFS_IOCMD_NSID(ptr)    _REG(ptr, 4, 32)

//completion queue ptrs
//sq head ptr in CQ element
#define BAFS_CQ_SQHP(ptr)   _REG(ptr,  8, 16)
#define BAFS_CQ_SQID(ptr)   _REG(ptr, 10, 16)
#define BAFS_CQ_CID(ptr)    _REG(ptr, 12, 16) //This is in DW3

// this is status field
#define BAFS_CQ_STS(ptr)    _REG(ptr, 14, 16)
#define BAFS_CQ_PHASE(ptr) (_REG(ptr, 14, 16) & 0x1) //this is phase bit

#endif //__BAFS_UTILS_H_
