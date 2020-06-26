/**
 * Depending on the type of package, there are different
 * compilation rules for this directory.  This comment applies
 * to packages of type "pkg."  For other types of packages,
 * please view the documentation at http://mynewt.apache.org/.
 *
 * Put source files in this directory.  All files that have a *.c
 * ending are recursively compiled in the src/ directory and its
 * descendants.  The exception here is the arch/ directory, which
 * is ignored in the default compilation.
 *
 * The arch/<your-arch>/ directories are manually added and
 * recursively compiled for all files that end with either *.c
 * or *.a.  Any directories in arch/ that don't match the
 * architecture being compiled are not compiled.
 *
 * Architecture is set by the BSP/MCU combination.
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <os/mynewt.h>
#include <hal/hal_spi.h>
#include <hal/hal_gpio.h>
#include "bsp/bsp.h"

#include "rtdoa_backhaul/rtdoa_backhaul.h"
#include "rtdoa/rtdoa.h"

#include "sensor/sensor.h"
#include "sensor/accel.h"
#include "sensor/gyro.h"
#include "sensor/mag.h"
#include "sensor/pressure.h"

#include <uwb/uwb.h>
#include <uwb/uwb_mac.h>
#include <uwb/uwb_ftypes.h>
#include <uwb_rng/uwb_rng.h>

#if MYNEWT_VAL(RTDOABH_STATS)
#include <stats/stats.h>
/* Define the stats section and records */
STATS_SECT_START(tag_stats)
    STATS_SECT_ENTRY(num_ranges)
    STATS_SECT_ENTRY(tx_ok)
    STATS_SECT_ENTRY(tx_err)
    STATS_SECT_ENTRY(rx_ok)
    STATS_SECT_ENTRY(rx_drop)
    STATS_SECT_ENTRY(rx_error)
    STATS_SECT_ENTRY(relay_ok)
    STATS_SECT_ENTRY(relay_err)
    STATS_SECT_ENTRY(a00_range)
    STATS_SECT_ENTRY(a00_rssi)
    STATS_SECT_ENTRY(a01_range)
    STATS_SECT_ENTRY(a01_rssi)
    STATS_SECT_ENTRY(a02_range)
    STATS_SECT_ENTRY(a02_rssi)
    STATS_SECT_ENTRY(a03_range)
    STATS_SECT_ENTRY(a03_rssi)
STATS_SECT_END

/* Global variable used to hold stats data */
STATS_SECT_DECL(tag_stats) g_tag_stats;

/* Define stat names for querying */
STATS_NAME_START(tag_stats)
    STATS_NAME(tag_stats, num_ranges)
    STATS_NAME(tag_stats, tx_ok)
    STATS_NAME(tag_stats, tx_err)
    STATS_NAME(tag_stats, rx_ok)
    STATS_NAME(tag_stats, rx_drop)
    STATS_NAME(tag_stats, rx_error)
    STATS_NAME(tag_stats, relay_ok)
    STATS_NAME(tag_stats, relay_err)
    STATS_NAME(tag_stats, a00_range)
    STATS_NAME(tag_stats, a00_rssi)
    STATS_NAME(tag_stats, a01_range)
    STATS_NAME(tag_stats, a01_rssi)
    STATS_NAME(tag_stats, a02_range)
    STATS_NAME(tag_stats, a02_rssi)
    STATS_NAME(tag_stats, a03_range)
    STATS_NAME(tag_stats, a03_rssi)
STATS_NAME_END(tag_stats)

#define RTDOABH_STATS_INC(x) STATS_INC(g_tag_stats,x)
#define RTDOABH_STATS_INCN(x,y) STATS_INCN(g_tag_stats,x,y)
#define RTDOABH_STATS_CLEAR(x) STATS_CLEAR(g_tag_stats,x)
#endif
#ifndef RTDOABH_STATS_INC
#define RTDOABH_STATS_INC(x) {}
#define RTDOABH_STATS_INCN(x,y) {}
#define RTDOABH_STATS_CLEAR(x) {}
#endif

#if MYNEWT_VAL(RTDOABH_USE_PROTOBUF)
#include "rtdoa_pb.h"
#endif

/* Incoming messages mempool and queue */
struct rtdoabh_msg_hdr {
    uint16_t dlen:15;
    uint16_t is_pb:1;
}__attribute__((packed, aligned(1)));

#define MBUF_PKTHDR_OVERHEAD    sizeof(struct os_mbuf_pkthdr) + sizeof(struct rtdoabh_msg_hdr)
#define MBUF_MEMBLOCK_OVERHEAD  sizeof(struct os_mbuf) + MBUF_PKTHDR_OVERHEAD

#define MBUF_NUM_MBUFS      MYNEWT_VAL(RTDOABH_NUM_MBUFS)
#define MBUF_PAYLOAD_SIZE   MYNEWT_VAL(RTDOABH_MBUF_SIZE)
#define MBUF_BUF_SIZE       OS_ALIGN(MBUF_PAYLOAD_SIZE, 4)
#define MBUF_MEMBLOCK_SIZE  (MBUF_BUF_SIZE + MBUF_MEMBLOCK_OVERHEAD)
#define MBUF_MEMPOOL_SIZE   OS_MEMPOOL_SIZE(MBUF_NUM_MBUFS, MBUF_MEMBLOCK_SIZE)

static struct os_mbuf_pool g_mbuf_pool;
static struct os_mempool g_mbuf_mempool;
static os_membuf_t g_mbuf_buffer[MBUF_MEMPOOL_SIZE];
static struct os_mqueue rxpkt_q;
static struct os_sem g_sem;

static uint32_t g_msg_id = 0;
static rtdoa_backhaul_role_t g_role = RTDOABH_ROLE_INVALID;
static uint64_t g_to_dx_time = 0; /* When the current listen for backhaul expires */

static struct rtdoabh_tag_results_pkg g_result_pkg = {
    .head = {
        .fctrl = FCNTL_IEEE_RTDOABH,
        .seq_num = 0,
        .PANID = 0xDECA,
        .dst_address = 0xffff,
        .src_address = 0x0000,
        .code = DWT_RTDOABH_CODE,
    },
    .sensors = {0,0},
    .num_ranges = 0,
    .ranges = {{0}}
};

static struct rtdoabh_tag_results_pkg g_result_pkg_imu = {
    .head = {
        .fctrl = FCNTL_IEEE_RTDOABH,
        .seq_num = 0,
        .PANID = 0xDECA,
        .dst_address = 0xffff,
        .src_address = 0x0000,
        .code = DWT_RTDOABH_CODE,
    },
    .sensors = {0,0},
    .num_ranges = 0,
    .ranges = {{0}}
};

static bool tx_complete_cb(struct uwb_dev * inst, struct uwb_mac_interface * cbs);
static bool rx_complete_cb(struct uwb_dev * inst, struct uwb_mac_interface * cbs);

static struct uwb_mac_interface g_cbs = {
    .id = UWBEXT_RTDOA_BH,
    .rx_complete_cb = rx_complete_cb,
    .tx_complete_cb = tx_complete_cb,
    .reset_cb = 0
};

static void
create_mbuf_pool(void)
{
    int rc;

    rc = os_mempool_init(&g_mbuf_mempool, MBUF_NUM_MBUFS,
                         MBUF_MEMBLOCK_SIZE, &g_mbuf_buffer[0], "rtdoabh_mpool");
    assert(rc == 0);

    rc = os_mbuf_pool_init(&g_mbuf_pool, &g_mbuf_mempool, MBUF_MEMBLOCK_SIZE,
                           MBUF_NUM_MBUFS);
    assert(rc == 0);
}

#if MYNEWT_VAL(RTDOABH_STATS)
static void
update_range_stats(int anchor, int32_t range, float rssi)
{
    switch (anchor)
    {
    case(0):
        RTDOABH_STATS_CLEAR(a00_range);
        RTDOABH_STATS_INCN(a00_range, range);
        RTDOABH_STATS_CLEAR(a00_rssi);
        RTDOABH_STATS_INCN(a00_rssi, abs(floor(rssi)));
        break;
    case(1):
        RTDOABH_STATS_CLEAR(a01_range);
        RTDOABH_STATS_INCN(a01_range, range);
        RTDOABH_STATS_CLEAR(a01_rssi);
        RTDOABH_STATS_INCN(a01_rssi, abs(floor(rssi)));
        break;
    case(2):
        RTDOABH_STATS_CLEAR(a02_range);
        RTDOABH_STATS_INCN(a02_range, range);
        RTDOABH_STATS_CLEAR(a02_rssi);
        RTDOABH_STATS_INCN(a02_rssi, abs(floor(rssi)));
        break;
    case(3):
        RTDOABH_STATS_CLEAR(a03_range);
        RTDOABH_STATS_INCN(a03_range, range);
        RTDOABH_STATS_CLEAR(a03_rssi);
        RTDOABH_STATS_INCN(a03_rssi, abs(floor(rssi)));
        break;
    default:
        break;
    }
}
#endif

void
rtdoa_backhaul_print(struct rtdoabh_tag_results_pkg *p, bool tight)
{
    struct rtdoabh_sensor_data *d = &p->sensors;
    printf("{\"id\":\"0x%04x\"", p->head.src_address);

    if(d->is_anchor_data) {
        printf(",\"mode\":\"anchor\"");
    }
    if (d->ts) {
        double ts = uwb_dwt_usecs_to_usecs(d->ts);
        uint64_t ts_s  = (uint64_t)(ts/1000000);
        uint32_t ts_tms = (ts-ts_s*1000000)/100.0;
        printf(",\"ts\":\"%llu.%04lu\"", ts_s, ts_tms);
    }
    printf(",\"mid\":%ld", g_msg_id++); /**< OBSERVE: This is from the local node */
    if (d->sensors_valid&GPS_LAT_LONG_ENABLED) {
        printf(",\"gps\":[\"%d.%d\"", (int)d->gps_lat,
           abs((int)((d->gps_lat -(int)d->gps_lat)*1000000)));
        printf(",\"%d.%d\"]", (int)d->gps_long,
               abs((int)((d->gps_long - (int)d->gps_long)*1000000)));
    }

    if (d->sensors_valid&BATTERY_LEVELS_ENABLED) {
        float f = (float)d->battery_voltage*5.0f/128;
        printf(",\"vbat\":\"%d.%02d\"",
               (int)f, abs((int)(100*(f-(int)f)))
            );
        printf(",\"usb\":%d", d->has_usb_power);
    }

    if (d->sensors_valid&ACCELEROMETER_ENABLED) {
        printf(",\"a\":\"[");
        for (int i=0;i<3;i++) {
            float f = (float)d->acceleration[i]/1000;
            printf("%s%d.%d",(i==0)?"":",",
                   (int)f, abs((int)(1000*(f-(int)f))));
        }
        printf("]\"");
    }
    if (d->sensors_valid&GYRO_ENABLED) {
        printf(",\"g\":\"[");
        for (int i=0;i<3;i++) {
            float f = (float)d->gyro[i]/10;
            printf("%s%d.%d",(i==0)?"":",",
                   (int)f, abs((int)(10*(f-(int)f))));
        }
        printf("]\"");
    }
    if (d->sensors_valid&COMPASS_ENABLED) {
        printf(",\"m\":[%d,%d,%d]", d->compass[0], d->compass[1], d->compass[2]);
    }
    if (d->sensors_valid&PRESSURE_ENABLED) {
        printf(",\"p\":%ld", ((int32_t)d->pressure)+101300);
    }

#if MYNEWT_VAL(RTDOABH_COMPACT_MEAS)
    if (!p->num_ranges) {
        goto early_close;
    }
    printf(",\"meas\":{");
    printf("\"ref\":\"%x\",", p->ref_anchor_addr);
    const char* key[4]={"a","dd","rs","qf"};

    for (int j=0;j<4;j++) {
        printf("\"%s\":[",key[j]);
        for (int i=0;i<p->num_ranges;i++) {
            struct rtdoabh_range_data *r = &p->ranges[i];
            int is_last = (i+1<p->num_ranges);
            switch (j){
            case (0): {
                printf("\"%x\"%s", r->anchor_addr, (is_last)?",":"");
                break;
            }
            case 1: {
                int sign = (r->diff_dist_mm > 0);
                int ddist_m  = r->diff_dist_mm/1000;
                int ddist_mm = abs(r->diff_dist_mm - ddist_m*1000);
                printf("%s%d.%03d%s", (sign)?"":"-", abs(ddist_m), ddist_mm, (is_last)?",":"");
                break;
            }
            case 2: {
                float rssif = (float)r->rssi/10;
                int rssi_frac = abs((int)(10*(rssif-(int)rssif)));
                printf("%d.%d%s", (int)rssif, rssi_frac, (is_last)?",":"");
                break;
            }
            case 3: {
                printf("%x%s", r->quality, (is_last)?",":"");
                break;
            }
            } /* End switch(j) */
        }
        printf("]%s", (j==3)?"}":",");
    }
early_close:
    printf("}\n");
#else
    printf(",\"ref_anchor\":\"%x\"", p->ref_anchor_addr);
    printf(",\"meas\":[%s", (tight || p->num_ranges==0)?"":"\n ");

    for (int i=0;i<p->num_ranges;i++) {
        struct rtdoabh_range_data *r = &p->ranges[i];
        float rssif = (float)r->rssi/10;
        int dist_m  = r->diff_dist_mm/1000;
        int dist_mm = abs(r->diff_dist_mm - dist_m*1000);
        dist_m = abs(dist_m);
        printf("{\"addr\":\"%x\",\"ddist\":\"%s%d.%03d\",\"tqf\":%d,\"rssi\":\"%d.%d\"}%s%s",
               r->anchor_addr,
               (r->diff_dist_mm < 0)?"-":"", dist_m, dist_mm,
               r->quality,
               (int)rssif, abs((int)(10*(rssif-(int)rssif))),
               (i+1<p->num_ranges)?",":"]",
               (tight)?"":"\n  "
            );
    }
    printf("]}\n");
#endif


}


static void
process_rx_data_queue(struct os_event *ev)
{
    int rc;
    struct os_mbuf *om;
    struct rtdoabh_msg_hdr *hdr;
    int payload_len;
    struct rtdoabh_tag_results_pkg pkg;

    while ((om = os_mqueue_get(&rxpkt_q)) != NULL) {
        hdr = (struct rtdoabh_msg_hdr*)(OS_MBUF_USRHDR(om));
        hdr->dlen = hdr->dlen;

        payload_len = OS_MBUF_PKTLEN(om);
        payload_len = (payload_len > sizeof(pkg)) ? sizeof(pkg) : payload_len;
        memset(&pkg, 0, sizeof(pkg));

        if (g_role == RTDOABH_ROLE_BRIDGE) {
            rc = os_mbuf_copydata(om, 0, payload_len, &pkg);
            if (rc) {
                goto end_msg;
            }
            rtdoa_backhaul_print(&pkg, true);
        }
    end_msg:
        os_mbuf_free_chain(om);
    }
}


/**
 * Listen for data
 *
 * @param inst Pointer to struct uwb_dev.
 *
 * @return struct uwb_dev_status
 */
struct uwb_dev_status
rtdoa_backhaul_listen(struct uwb_dev * inst, uint64_t dx_time, uint16_t timeout_uus)
{
    os_error_t err = os_sem_pend(&g_sem,  OS_TIMEOUT_NEVER);
    assert(err == OS_OK);

    g_to_dx_time = dx_time + (((uint64_t)timeout_uus)<<16);
    uwb_set_delay_start(inst, dx_time);
    uwb_set_rx_timeout(inst, timeout_uus);

    if(uwb_start_rx(inst).start_rx_error){
        err = os_sem_release(&g_sem);
        assert(err == OS_OK);
        RTDOABH_STATS_INC(rx_error);
    }

    err = os_sem_pend(&g_sem, OS_TIMEOUT_NEVER);
    assert(err == OS_OK);
    err = os_sem_release(&g_sem);
    assert(err == OS_OK);
    /* Prevent rx_complete from modifying rx_timeout */
    g_to_dx_time = 0;
    return inst->status;
}

static bool
tx_complete_cb(struct uwb_dev * inst, struct uwb_mac_interface * cbs)
{
    if(inst->fctrl != FCNTL_IEEE_RTDOABH){
        return false;
    }
    if (g_to_dx_time) {
        /* Should only get here if we are repeating messages */
        return false;
    }
    if(os_sem_get_count(&g_sem) == 0) {
        os_error_t err = os_sem_release(&g_sem);
        assert(err == OS_OK);
    }
    return true;
}

static bool
rx_complete_cb(struct uwb_dev * inst, struct uwb_mac_interface * cbs)
{
    static uint16_t last_address = 0;
    static uint8_t last_seq_num = 0;
    int rc;
    struct os_mbuf *om;

    if (g_to_dx_time) {
        uint16_t timeout_uus = (g_to_dx_time - inst->rxtimestamp) >> 16;
        uwb_set_rx_timeout(inst, timeout_uus);
    }

    if(inst->fctrl != FCNTL_IEEE_RTDOABH){
        return false;
    }

    ieee_rng_request_frame_t *frame = (ieee_rng_request_frame_t*)inst->rxbuf;
    if (frame->code < DWT_RTDOABH_CODE) {
        return false;
    }

    if ((frame->dst_address != inst->my_short_address &&
        frame->dst_address != UWB_BROADCAST_ADDRESS) ||
        g_role == RTDOABH_ROLE_INVALID || g_role == RTDOABH_ROLE_PRODUCER) {

        if(os_sem_get_count(&g_sem) == 0) {
            os_error_t err = os_sem_release(&g_sem);
            assert(err == OS_OK);
        }
        return true;
    }

    struct rtdoabh_tag_results_pkg *pkg = (struct rtdoabh_tag_results_pkg *)inst->rxbuf;

    if (inst->frame_len >= sizeof(struct rtdoabh_sensor_data)) {
        if (pkg->head.src_address == last_address && pkg->head.seq_num == last_seq_num) {
            RTDOABH_STATS_INC(rx_drop);
            return true;
        }
        last_address = pkg->head.src_address;
        last_seq_num = pkg->head.seq_num;

        RTDOABH_STATS_INC(rx_ok);
        om = os_mbuf_get_pkthdr(&g_mbuf_pool,
                                sizeof(struct rtdoabh_msg_hdr));
        if (om) {
            struct rtdoabh_msg_hdr *hdr = (struct rtdoabh_msg_hdr*)OS_MBUF_USRHDR(om);
            hdr->dlen = inst->frame_len;

            rc = os_mbuf_copyinto(om, 0, inst->rxbuf, hdr->dlen);
            if (rc != 0) {
                return true;
            }
            rc = os_mqueue_put(&rxpkt_q, os_eventq_dflt_get(), om);
            if (rc != 0) {
                return true;
            }
        } else {
            /* Not enough memory to handle incoming packet, drop it */
            RTDOABH_STATS_INC(rx_drop);
        }
    }

    if(os_sem_get_count(&g_sem) == 0) {
        os_error_t err = os_sem_release(&g_sem);
        assert(err == OS_OK);
    }

    return true;
}

void
rtdoa_backhaul_set_role(struct uwb_dev * inst, rtdoa_backhaul_role_t role)
{
    g_role = role;
}

void
rtdoa_backhaul_set_a2a(struct uwb_dev * inst)
{
    g_result_pkg.sensors.is_anchor_data = 1;
}

int
rtdoa_backhaul_sensor_data_cb(struct sensor* sensor, void *arg, void *data, sensor_type_t type)
{
    struct sensor_accel_data *sad;
    struct sensor_mag_data *smd;
    struct sensor_gyro_data *sgd;
    struct sensor_press_data *spd;
    float conv;

    if (type == SENSOR_TYPE_ACCELEROMETER ||
        type == SENSOR_TYPE_LINEAR_ACCEL  ||
        type == SENSOR_TYPE_GRAVITY) {
        /* m/s^2 to mm/s^2 */
        conv = 1000;
        sad = (struct sensor_accel_data *) data;
        if (sad->sad_x_is_valid && sad->sad_y_is_valid && sad->sad_z_is_valid) {
            g_result_pkg_imu.sensors.acceleration[2] = (int16_t)roundf(conv*sad->sad_x);
            g_result_pkg_imu.sensors.acceleration[0] = (int16_t)roundf(conv*sad->sad_y);
            g_result_pkg_imu.sensors.acceleration[1] = (int16_t)roundf(conv*sad->sad_z);
            g_result_pkg_imu.sensors.sensors_valid |= ACCELEROMETER_ENABLED;
        }
    }

    if (type == SENSOR_TYPE_MAGNETIC_FIELD) {
        smd = (struct sensor_mag_data *) data;
        if (smd->smd_x_is_valid && smd->smd_y_is_valid && smd->smd_z_is_valid) {
            /* uT */
            g_result_pkg_imu.sensors.compass[2] = (int16_t)(smd->smd_x);
            g_result_pkg_imu.sensors.compass[1] = (int16_t)(smd->smd_y);
            g_result_pkg_imu.sensors.compass[0] = (int16_t)(smd->smd_z);
            g_result_pkg_imu.sensors.sensors_valid |= COMPASS_ENABLED;
        }
    }

    if (type == SENSOR_TYPE_GYROSCOPE) {
        sgd = (struct sensor_gyro_data *) data;

        conv = 10.0;            /* For [-2000,2000] dps sensor range */
        if (sgd->sgd_x_is_valid && sgd->sgd_y_is_valid && sgd->sgd_z_is_valid) {
            g_result_pkg_imu.sensors.gyro[2] = (int16_t)roundf(conv*sgd->sgd_x);
            g_result_pkg_imu.sensors.gyro[0] = (int16_t)roundf(conv*sgd->sgd_y);
            g_result_pkg_imu.sensors.gyro[1] = (int16_t)roundf(conv*sgd->sgd_z);
            g_result_pkg_imu.sensors.sensors_valid |= GYRO_ENABLED;
        }
    }

    if (type == SENSOR_TYPE_PRESSURE) {
        spd = (struct sensor_press_data *) data;
        if (spd->spd_press_is_valid) {
            g_result_pkg_imu.sensors.pressure = (int16_t)(spd->spd_press-101300);
            g_result_pkg_imu.sensors.sensors_valid |= PRESSURE_ENABLED;
        }
    }

    return (0);
}

void
rtdoa_backhaul_battery_cb(float battery_volt)
{
    g_result_pkg_imu.sensors.battery_voltage = (int8_t)(battery_volt*128/5.0);
    g_result_pkg_imu.sensors.sensors_valid |= BATTERY_LEVELS_ENABLED;
}

void
rtdoa_backhaul_usb_cb(float usb_volt)
{
    g_result_pkg_imu.sensors.has_usb_power = usb_volt > 3.0;
}

void
rtdoa_backhaul_set_ts(uint64_t sensor_time)
{
    g_result_pkg.sensors.ts = sensor_time;
}

static int
rtdoa_local_send(uint8_t *buf, int dlen)
{
    int rc;
    struct os_mbuf *om;
    om = os_mbuf_get_pkthdr(&g_mbuf_pool,
                            sizeof(struct rtdoabh_msg_hdr));
    if (!om) {
        return OS_ENOMEM;
    }

    struct rtdoabh_msg_hdr *hdr = (struct rtdoabh_msg_hdr*)OS_MBUF_USRHDR(om);
    hdr->dlen = dlen;
    rc = os_mbuf_copyinto(om, 0, buf, hdr->dlen);
    if (rc != 0) {
        goto exit_err;
    }
    rc = os_mqueue_put(&rxpkt_q, os_eventq_dflt_get(), om);
    if (rc != 0) {
        goto exit_err;
    }

    return 0;
exit_err:
    os_mbuf_free_chain(om);
    return rc;
}

int
rtdoa_backhaul_queue_size()
{
    int queued = g_mbuf_mempool.mp_num_blocks - g_mbuf_mempool.mp_num_free;
    return queued;
}

void
rtdoa_backhaul_send_imu_only(uint64_t ts)
{
    g_result_pkg_imu.head.seq_num++;
    g_result_pkg_imu.sensors.ts = ts;
    rtdoa_local_send((uint8_t*)&g_result_pkg_imu, sizeof(struct _ieee_rng_request_frame_t) + sizeof(struct rtdoabh_sensor_data));
    memset(&g_result_pkg_imu.sensors, 0, sizeof(struct rtdoabh_sensor_data));
}

struct uwb_dev_status
rtdoa_backhaul_send(struct uwb_dev * inst, struct rtdoa_instance *rtdoa,
                    uint64_t dx_time)
{
    g_result_pkg.head.seq_num++;
    g_result_pkg.num_ranges = 0;

    g_result_pkg.head.code = DWT_RTDOABH_CODE;
    /* Write sensorinformation part of packet */
    int dlen = sizeof(struct rtdoabh_tag_results_pkg);
    int split_at = offsetof(struct rtdoabh_tag_results_pkg, num_ranges);
    if (dx_time) {
        uwb_write_tx(inst, (uint8_t*)&g_result_pkg, 0, split_at+1); /* +1 to write 0 to num rng */
    }
    g_result_pkg.ref_anchor_addr = rtdoa->req_frame->src_address;

    for(int i = 0 ; i < rtdoa->nframes;i++)
    {
        /* TODO: Should we skip ref_anchor in ranges? */
        //if (g_result_pkg.ref_anchor_addr == rtdoa->frames[i]->src_address) {
        //    continue;
        //}
        float diff = rtdoa_tdoa_between_frames(rtdoa, rtdoa->req_frame, rtdoa->frames[i]);
        if (isnan(diff)) continue;

        int32_t diff_mm = (int32_t)(diff*1000.0f + 0.5f);
        g_result_pkg.ranges[g_result_pkg.num_ranges].diff_dist_mm = diff_mm;
        if (rtdoa->frames[i]->src_address == 0) {
            continue;
        }
        float rssi = uwb_calc_rssi(inst, &rtdoa->frames[i]->diag);
        float fppl = uwb_calc_fppl(inst, &rtdoa->frames[i]->diag);
#if MYNEWT_VAL(RTDOABH_STATS)
        RTDOABH_STATS_INC(num_ranges);
        update_range_stats(i, diff_mm, rssi);
#endif
        g_result_pkg.ranges[g_result_pkg.num_ranges].anchor_addr = rtdoa->frames[i]->src_address;
        g_result_pkg.ranges[g_result_pkg.num_ranges].rssi = rssi*10;
        g_result_pkg.ranges[g_result_pkg.num_ranges].quality = (int)uwb_estimate_los(inst, rssi,fppl);
        g_result_pkg.num_ranges++;
    }

    /* Removed unused slots from packet to send */
    dlen -= sizeof(struct rtdoabh_range_data)*(sizeof(g_result_pkg.ranges)/sizeof(g_result_pkg.ranges[0]) -
                                          g_result_pkg.num_ranges);

    if (dx_time) {
        uwb_set_delay_start(inst, dx_time);
        uwb_write_tx_fctrl(inst, dlen, 0);
        uwb_write_tx(inst, ((uint8_t*)&g_result_pkg)+split_at, split_at, dlen-split_at);
        if (uwb_start_tx(inst).start_tx_error) {
            RTDOABH_STATS_INC(tx_err);
            uint32_t utime = os_cputime_ticks_to_usecs(os_cputime_get32());
            printf("{\"utime\": %lu,\"msg\": \"rtdoabh_start_tx_error(res)\"}\n",utime);
            goto exit_err;
        } else if (dlen-split_at > 1) {
            RTDOABH_STATS_INC(tx_ok);
        }
    }
    /* If we're a local bridge */
    if (g_role == RTDOABH_ROLE_BRIDGE) {
        int rc = rtdoa_local_send((uint8_t*)&g_result_pkg, dlen);
        if (rc != 0) {
            goto exit_err;
        }
    }

exit_err:
    /* Clear buffers in anticipaction of the next round */
    memset(&g_result_pkg.sensors, 0, sizeof(struct rtdoabh_sensor_data));
    memset(&g_result_pkg.ranges, 0, MYNEWT_VAL(RTDOABH_MAXNUM_RANGES)*sizeof(struct rtdoabh_range_data));
    return inst->status;
}

void
rtdoa_backhaul_init(struct uwb_dev * inst)
{
    assert(inst);

    create_mbuf_pool();
    os_mqueue_init(&rxpkt_q, process_rx_data_queue, NULL);

    g_result_pkg.head.src_address = inst->my_short_address;
    g_result_pkg_imu.head.src_address = inst->my_short_address;

    uwb_mac_append_interface(inst, &g_cbs);
}

void
rtdoa_backhaul_pkg_init(void)
{
    rtdoa_backhaul_init(uwb_dev_idx_lookup(0));

    os_error_t err = os_sem_init(&g_sem, 0x1);
    assert(err == OS_OK);
#if MYNEWT_VAL(RTDOABH_STATS)
    int rc = stats_init_and_reg(
        STATS_HDR(g_tag_stats), STATS_SIZE_INIT_PARMS(g_tag_stats,
        STATS_SIZE_32), STATS_NAME_INIT_PARMS(tag_stats), "rtdoabh");
    assert(rc == 0);
#endif
}
