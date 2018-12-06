#include "os/os.h"
#include "bsp/bsp.h"

#if MYNEWT_VAL(PAN_ENABLED)
#include <dw1000/dw1000_hal.h>
#include <pan/pan.h>
#include "pan_master.h"
#endif

#define PAN_SIZE 16
static uint16_t g_device_idx = 0;

pan_db_t g_pan_db[PAN_SIZE];

static uint8_t
first_free_slot_id(uint16_t node_addr, uint16_t role)
{
    int j;
    uint8_t slot_id=1;
    while (slot_id < 255) {
        for (j = 0; j < g_device_idx; j++) {
            if (g_pan_db[j].short_address == 0xffff || g_pan_db[j].role != role)
                continue;

            if (slot_id == g_pan_db[j].slot_id && node_addr != g_pan_db[j].short_address){
                goto next_slot;
            }
        }

        return slot_id;
    next_slot:
        slot_id++;
    }
    
    return 255;
}

pan_db_t*
pan_master_find_node(uint64_t euid, uint16_t role)
{
    uint8_t i = 0;
    for (i = 0; i < g_device_idx; i++){
        if (g_pan_db[i%PAN_SIZE].UUID == euid) {
            return &g_pan_db[i%PAN_SIZE];
        }
    }
    if (i == g_device_idx){
        // Assign new IDs
        g_pan_db[g_device_idx%PAN_SIZE].UUID = euid;
        g_pan_db[g_device_idx%PAN_SIZE].short_address = 0xDEC0 + g_device_idx + 1;
        g_pan_db[g_device_idx%PAN_SIZE].slot_id =
            first_free_slot_id(g_pan_db[g_device_idx%PAN_SIZE].short_address, role);
        g_pan_db[g_device_idx%PAN_SIZE].role = role;

        return &g_pan_db[g_device_idx++%PAN_SIZE];
    }

    return 0;
}

static void 
pan_master_cb(struct os_event * ev)
{
    assert(ev != NULL);
    assert(ev->ev_arg != NULL);
    dw1000_dev_instance_t * inst = (dw1000_dev_instance_t *)ev->ev_arg;
    dw1000_pan_instance_t * pan = inst->pan; 
    pan_frame_t * frame = pan->frames[(pan->idx)%pan->nframes]; 

    pan_db_t *db_entry = pan_master_find_node(frame->long_address, frame->role);
    if (db_entry) {
        frame->code = DWT_PAN_RESP;
        frame->pan_id = inst->PANID;
        frame->short_address = db_entry->short_address; 
        frame->slot_id = db_entry->slot_id;
        frame->role = db_entry->role;
        frame->seq_num++;
    } else {
        printf("{\"utime\":%lu,\"Warning\": \"Could not assign PAN\",{\"g_device_idx\":%d}\n", 
               os_cputime_ticks_to_usecs(os_cputime_get32()),
               g_device_idx
        );
        return;
    }

    if(g_device_idx > PAN_SIZE)
    {
        printf("{\"utime\":%lu,\"Warning\": \"PANIDs over subscribed\",{\"g_device_idx\":%d}\n", 
               os_cputime_ticks_to_usecs(os_cputime_get32()),
               g_device_idx
        );  
    }

    dw1000_write_tx(inst, frame->array, 0, sizeof(pan_frame_resp_t));
    dw1000_write_tx_fctrl(inst, sizeof(pan_frame_resp_t), 0, true); 
    pan->status.start_tx_error = dw1000_start_tx(inst).start_tx_error;

    /* PAN Request frame */
    printf("{\"utime\":%lu,\"UUID\":\"%llX\",\"seq_num\": %d}\n", 
        os_cputime_ticks_to_usecs(os_cputime_get32()),
        frame->long_address,
        frame->seq_num
    );
    printf("{\"utime\":%lu,\"UUID\":\"%llX\",\"ID\":\"%X\",\"PANID\":\"%X\",\"SLOTID\":%d}\n", 
           os_cputime_ticks_to_usecs(os_cputime_get32()),
           frame->long_address,
           frame->short_address,
           frame->pan_id,
           frame->slot_id
        );
}

void
pan_master_init(dw1000_dev_instance_t * inst)
{
    dw1000_pan_set_postprocess(inst, pan_master_cb);
}
