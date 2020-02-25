#ifndef __BAFS_UTILS_H_
#define __BAFS_UTILS_H_


/*Create lower bit mask - example bit mask for 12 is 0xFFF*/ 
#define _BIT_MASK(num_bits) ((1ULL << (num_bits)) - 1)

/*Extract single bit - example for 12 is 0x1000*/
#define _SBIT_MASK(num_bits) ((1ULL << (num_bits)))

/*Extract range of bits - example from 10 to 12. i.e bit 10, 11 and 12*/
#define _RBIT_MASK(high,low) (_BIT_MASK((high) + 1) - _BIT_MASK(lo))

/*Extract range of bits from register/value*/
#define _RDBITS(val, high, low) (((val) & _RBIT_MASK((high),(low))) >> (lo))


/*Standard fields */






#endif //__BAFS_UTILS_H_
