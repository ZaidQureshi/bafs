#ifndef __BAFS_ERRORS__
#define __BAFS_ERRORS__

//#include <stdint.h>
#include "bafs_utils.h"
//#include <string.h>
//#include <errno.h>

#define _UNPACK_SCT(status) (_RDBITS(status, 10, 8))
#define _UNPACK_SC(status)  (_RDBITS(status, 7, 0))
#define _SUCCESS(status)    ((status>>1) == 0)   // if all bits are 0 then success. 

/*Refer Section 4.6.1.2.1 in rv1.4 June 2019*/
static const char* generic_cmd_status[] = {
    "Success",
    "Invalid Command Opcode",
    "Invalid Field in Command",
    "Command ID Conflict",
    "Data Transfer Error",
    "Commands Aborted Due to Power Loss Notification",
    "Internal Error. Refer Figure 145",
    "Commad Abort Requested",
    "Command Aborted due to SQ Deletion",
    "Command Aborted due to Failed Fused operation",
    "Command Aborted due to Missing Fused command",
    "Invalid Namespace or Format",
    "Command Sequence Error",
    "Invalid SGL Segment Descriptor",
    "Invalid Number of SGL Descriptor",
    "Data SGL Length Invalid",
    "Metadata SGL Length Invalid",
    "SGL Desciptor Type Invalid",
    "Invalid use of CMB",
    "PRP Offset Invalid",
    "Atomic Write Unit Exceeded",
    "Operation Denied",
    "SGL Offset Invalid",
    "Reserved",
    "Host Identifier Inconsistent Format",
    "Keep Alive Timer Expired",
    "Keep Alive Timeout Invalid",
    "Commmad Aboirted de to Prempt and Abort",
    "Santize Failed",
    "Santize in Progress",
    "SGL Data Block Granularity Invalid",
    "Command Not Supported For QUeue in CMB",
    "Namespace is Write Protected",
    "Command Interrupted",
    "Transient Transport Error",
    "Reserved"
};


static const char* generic_nvm_status[] = {
    "LBA Out of Range",
    "Capacity Exceeded",
    "Namespace Not Ready",
    "Reservation Conflict",
    "Format in Progress"
};

static const char* cmd_specific_status[] = {
    "Completion Queue Invalid",
    "Invalid Queue Identifier",
    "Invalid Queue Size",
    "Aboirt Command Limit Exceeded",
    "Reserved",
    "Asynchronous Event Request Limit Execeeded",
    "Invalid Firmware Slot",
    "Invalid Firmware Image",
    "Invalid Interrupt Vector",
    "Invalid Log Page",
    "Invalid Format",
    "Firmware Activation Request Conventional Reset",
    "Invalid Queue Deletion",
    "Feature Identifier Not Saveable",
    "Feature Not Changeable",
    "Feature Not Namespace Specific",
    "Firmware Activation Requires NVM Subsystem Reset",
    "Firmware Activation Requires Controller Reset",
    "Firmware Activation Requires Maximum Time Violation",
    "Firmware Activation Prohibited",
    "Overlapping Range",
    "Namespace Insufficient Capacity",
    "Namespace Identifier Unavailable",
    "Reserved",
    "Namespace Already Attached",
    "Namespace is Private",
    "Namespace Not Attached",
    "Thin Provisioning is not supported",
    "Controller List Invalid",
    "Device Self Test In Progress",
    "Boot Partition Write Prohibited",
    "Invalid Controller Identifier",
    "Invalid Secondary Controller State",
    "Invalid Number of Controller Resources",
    "Invalid Resource Identifier",
    "Santize Prohibited in PMR",
    "ANA Group Identifier Invalid",
    "ANA Attach Failed"
    };

static const char* nvm_cmd_specific_status[] = {
    "Conflicting Attributed",
    "Invalid Protection Information",
    "Attempted Write To Read Only Range"
};


static const char* print_error(uint8_t sct, uint8_t sc){

    switch (sct){
        case 0x00: // generic command status
            if(sc <0x23) 
                return generic_cmd_status[sc];
            else if ((sc > 0x7F ) && (sc < 0x85))
                return generic_nvm_status[(sc - 0x80)];
            else  
                return "Either Vendor Specific Error or Undefined Error";

        case 0x01: // command specific status
            if(sc < 0x26) 
                return cmd_specific_status[sc];
            else if ((sc > 0x7F) && (sc < 0x83))
                return nvm_cmd_specific_status[(sc - 0x80)];
            else
                return "Either Vendor Specific Error or Undefined Error";

        default:
            if(sct == 0x02)
                return "Media Specific Error - Error Code Not implemented";
            else 
                return "Unknown Status Code Type";
    }

} 



const char* bafs_error (uint32_t status){
    uint8_t sct = 0;
    uint8_t sc  = 0;

    sct = _UNPACK_SCT(status);
    sc  = _UNPACK_SC(status);

    printk(KERN_INFO "[debug] status field: %llx - sct: %llu - sc: %llu", status, sct, sc);

    if(sct != 0 || sc != 0){
        return print_error(sct, sc);
    }
    else if (_SUCCESS(status)){
#ifdef KERN
        return "Success";
#else 
        return strerror(0);
#endif 
    }

#ifdef KERN
        return "Unknown";
#else 
        return strerror(status);
#endif 
}



#endif //__BAFS_ERRORS__
