/*
 */

#ifndef _RTDOA_BACKHAUL_H_
#define _RTDOA_BACKHAUL_H_

#include <inttypes.h>
#include <sensor/sensor.h>
#include <uwb/uwb.h>
#include <uwb/uwb_ftypes.h>

#include "rtdoa/rtdoa.h"
#include "rtdoa_tag/rtdoa_tag.h"

#define GPS_LAT_LONG_ENABLED    0x0001UL
#define UWB_RANGES_ENABLED      0x0002UL
#define SX1280_RANGES_ENABLED   0x0004UL
#define COMPASS_ENABLED         0x0008UL

#define ACCELEROMETER_ENABLED   0x0010UL
#define GYRO_ENABLED            0x0020UL
#define PRESSURE_ENABLED        0x0040UL
#define BATTERY_LEVELS_ENABLED  0x0080UL

#define FCNTL_IEEE_RTDOABH 0x88C1
#define DWT_RTDOABH_CODE         0x6003

struct rtdoabh_sensor_data {
    uint64_t ts;                   /**< timestamp as in master's clock frame (dwt_usecs)*/
    uint16_t sensors_valid:14;     /**< Filled in as values arrive */
    uint16_t has_usb_power:1;      /**< If power on UWB input > 3V */
    uint16_t is_anchor_data:1;     /**< Anchor to anchor ranging */
    float    gps_lat;              /**< Decimal deg */
    float    gps_long;             /**< Decimal deg */
    int8_t   battery_voltage;      /**< steps: 5/128 */
    int16_t  pressure;             /**< Pa diff from 1013hPa */
    int16_t  compass[3];           /**< mG, 6 byte */
    int16_t  acceleration[3];      /**< mm/s^2, 6 byte */
    int16_t  gyro[3];              /**< 10*deg/s, 6 byte */
}  __attribute__((packed, aligned(1))); // 38 byte

struct rtdoabh_range_data {
    uint16_t anchor_addr;
    int32_t diff_dist_mm;
    int16_t rssi:14;       /**< rssi = -(rssi_float+60)*3 */
    int16_t quality:2;
} __attribute__((packed, aligned(1))); // 8 byte

struct rtdoabh_tag_results_pkg {
    struct _ieee_rng_request_frame_t head;
    struct rtdoabh_sensor_data sensors;
    uint16_t ref_anchor_addr;
    uint8_t num_ranges;
    struct rtdoabh_range_data ranges[MYNEWT_VAL(RTDOABH_MAXNUM_RANGES)];
} __attribute__((packed, aligned(1)));

typedef enum _rtdoa_backhaul_role_t{
    RTDOABH_ROLE_INVALID,
    RTDOABH_ROLE_BRIDGE,         // Bridge UWB -> USB / UDP
    RTDOABH_ROLE_PRODUCER,
}rtdoa_backhaul_role_t;

#ifdef __cplusplus
extern "C" {
#endif

void rtdoa_backhaul_set_a2a(struct uwb_dev * inst);
void rtdoa_backhaul_set_role(struct uwb_dev * inst, rtdoa_backhaul_role_t role);
void rtdoa_backhaul_print(struct rtdoabh_tag_results_pkg *p, bool tight);
int rtdoa_backhaul_sensor_data_cb(struct sensor* sensor, void *arg, void *data, sensor_type_t type);
void rtdoa_backhaul_battery_cb(float battery_volt);
void rtdoa_backhaul_usb_cb(float usb_volt);
void rtdoa_backhaul_set_ts(uint64_t sensor_time);

struct uwb_dev_status rtdoa_backhaul_send(struct uwb_dev * inst, struct rtdoa_instance *rtdoa, uint64_t dxtime);
int rtdoa_backhaul_queue_size();
void rtdoa_backhaul_send_imu_only(uint64_t ts);
struct uwb_dev_status rtdoa_backhaul_local(struct uwb_dev * inst, struct rtdoa_instance *rtdoa);
struct uwb_dev_status rtdoa_backhaul_listen(struct uwb_dev * inst, uint64_t dx_time, uint16_t timeout_uus);
#ifdef __cplusplus
}
#endif

#endif /* _RTDOA_BACKHAUL_H */
