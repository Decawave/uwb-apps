#ifndef _PAN_MASTER_H_
#define _PAN_MASTER_H_

//! Network Roles
typedef enum {
    DWT_TWR_ROLE_INVALID = 0x0,   /*!< Invalid role */
    DWT_TWR_ROLE_NODE,            /*!< Node */
    DWT_TWR_ROLE_TAG              /*!< Tag */
} dwt_twr_role_t;

typedef struct _pan_db {
    uint64_t UUID;
    uint32_t short_address;
    dwt_twr_role_t role;
    uint8_t slot_id;
}pan_db_t;

#ifdef __cplusplus
extern "C" {
#endif

pan_db_t* pan_master_find_node(uint64_t euid, uint16_t role);
void pan_master_init(dw1000_dev_instance_t * inst);

#ifdef __cplusplus
}
#endif

#endif

