/*
 * Bosch BHI-160 sensor access library
 *
 * Iwo.Mergler@netcommwireless.com
 */

#ifndef _BHI_160_H
#define _BHI_160_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include <i2c_linux.h>

#ifndef BHI160_I2C_ADDR
#define BHI160_I2C_ADDR 0x28
#endif

#ifndef BHI160_I2C_BUS
#define BHI160_I2C_BUS 0x00
#endif

/* FIFO read data access */
#define REG_BUF_OUT             0x00
#define REG_BUF_OUT_SZ          0x32

/* Fifo flush. write sensor number for specific sensor only flush */
#define REG_FIFO_FLUSH          0x32
#define REG_FIFO_FLUSH_SZ       0x01
#define FIFO_FLUSH_NOP          0x00
#define FIFO_FLUSH_ALL          0xff

/* Main control register */
#define REG_CHIP_CTRL           0x34
#define REG_CHIP_CTRL_SZ        0x01
#define CHIP_CTRL_CPU_RUN       0x01 // CPU run request
#define CHIP_CTRL_UPLD_EN       0x02 // Host upload enable

/* status for host */
#define REG_HOST_STS            0x35
#define REG_HOST_STS_SZ         0x01
#define HOST_STS_RESET          0x01 // Had a reset (POR or requested)
#define HOST_STS_ALG_STBY       0x02 // ACK for host standby request
#define HOST_STS_IF_ID_MSK      0x1c // 0=Android K, 1=Android L
#define HOST_STS_ALG_ID_MSH     0xe0 // Algorithm ID, 0 Bosch BSX fusion library

/* Interrupt status */
#define REG_INT_STS             0x36
#define REG_INT_STS_SZ          0x01
#define INT_STS_HOST            0x01 // Host interrupt pin state
#define INT_STS_WKUP_WMRK       0x02 // Wakeup FIFO watermark reached
#define INT_STS_WKUP_LAT        0x04 // Wakeup latency timout
#define INT_STS_WKUP_IMM        0x08 // Wakeup immediate (for no latency config)
#define INT_STS_NWK_WMRK        0x10 // Non-Wakeup FIFO watermark
#define INT_STS_NWK_LAT         0x20 // Non-Wakeup latency
#define INT_STS_NWK_IMM         0x40 // Non-Wakeup immediate

/* status for chip, boot sequence */
#define REG_CHIP_STS            0x37
#define REG_CHIP_STS_SZ         0x01
#define CHIP_STS_EE_DET         0x01 // EEPROM detected
#define CHIP_STS_EE_UPLD_DONE   0x02 // EEPROM upload done
#define CHIP_STS_EE_UPLD_ERR    0x04 // EEPROM upload error
#define CHIP_STS_FW_IDLE        0x08 // Firmware idle (halted)
#define CHIP_STS_NO_EE          0x10 // No EEPROM detected

/* Bytes available in FIFO buffer */
#define REG_BYTES_REM           0x38
#define REG_BYTES_REM_SZ        0x02 // 2 bytes, single access, LE

/* Parameter acknowledge - feedback for parameter page operations */
#define REG_PRM_ACK             0x3a
#define REG_PRM_ACK_SZ          0x01
#define PRM_ACK_ERR             0x80 // Sets to parameter number of request or error

/* Parameter read buffer, large enough for any parameter structure */
#define REG_PRM_READ            0x3b
#define REG_PRM_READ_SZ         0x10

/* General purpose host R/O, fuser R/W registers */
#define REG_GP20_GP24           0x4b
#define REG_GP20_GP24_SZ        0x05

/* Parameter page selection for parameter read / write */
#define REG_PRM_PAGE_SEL        0x54
#define REG_PRM_PAGE_SEL_SZ     0x01
#define PRM_PAGE_SEL_MSK        0x0F // LSN is page number
#define PRM_PAGE_SEL_SZ_MSK     0xF0 // MSN is transfer size in bytes

/* Host interface control */
#define REG_HOST_INF_CTRL       0x55
#define REG_HOST_INF_CTRL_SZ    0x01
#define HOST_INF_CTRL_ALG_STBY  0x01 // Algorithm standby (low power suspend)
#define HOST_INF_CTRL_ABORT     0x02 // Abort current transfer, drops pending data, resets int
#define HOST_INF_CTRL_UPD_CNT   0x04 // Update FIFO transfer count
#define HOST_INF_CTRL_WKUP_DS   0x08 // Wakeup FIFO interrupt disable (master switch)
#define HOST_INF_CTRL_NED       0x10 // North East Down, vs. East North Up (default) coordinates
#define HOST_INF_CTRL_SUSPENDED 0x20 // host suspended, only allow wakeup sensor events
#define HOST_INF_CTRL_SELFTEST  0x40 // Request sensor self test when leaving standby
#define HOST_INF_CTRL_NWK_DS    0x80 // ? Non-wakeup interrupt disable

/* General purpose host R/W, fuser R/O registers */
#define REG_GP31_GP36           0x56
#define REG_GP31_GP36_SZ        0x06

/* Parameter write buffer */
#define REG_PRM_WRITE           0x5c
#define REG_PRM_WRITE_SZ        0x08

/* Parameter request */
#define REG_PRM_REQ             0x64
#define REG_PRM_REQ_SZ          0x01
#define PRM_REQ_READ            0x00
#define PRM_REQ_WRITE           0x80
#define PRM_REQ_MSK             0x7F // parameter page select

/* General purpose host R/W, fuser R/O registers */
#define REG_GP46_GP52           0x65
#define REG_GP46_GP52_SZ        0x07

/* ROM version */
#define REG_ROM_VER             0x70
#define REG_ROM_VER_SZ          0x02 // LE
#define ROM_VER_C1              0x1e8b
#define ROM_VER_C2              0x2112

/* RAM version, shows up after RAM patch is loaded. */
#define REG_RAM_VER             0x72
#define REG_RAM_VER_SZ          0x02 // LE

/* Product ID */
#define REG_PROD_ID             0x90
#define REG_PROD_ID_SZ          0x01
#define PROD_ID_C1              0x81
#define PROD_ID_C2              0x83

/* Revision ID */
#define REG_REV_ID              0x91
#define REG_REV_ID_SZ           0x01
#define REV_ID_C1               0x03
#define REV_ID_C2               0x01

/* Upload address - self incrementing address pointer for RAM patch upload */
#define REG_UPLD_ADDR           0x94
#define REG_UPLD_ADDR_SZ        0x02 // BE (exception)

/* Upload data
 * After setting address and CHIP_CTRL_UPLD_EN, burst data to this register.
 * 16-bit header in file must be skipped, then file uploaded byte swapped. */
#define REG_UPLD_DATA           0x96
#define REG_UPLD_DATA_SZ        0x01

/* Upload CRC
 * 32 bit CRC of uploaded data, for comparison at end.
 * unset CHIP_CTRL_UPLD_EN and set CHIP_CTRL_CPU_RUN to execute */
#define REG_UPLD_CRC            0x97
#define REG_UPLD_CRC_SZ         0x04

/* Reset request */
#define REG_RESET_REQ           0x9b
#define REG_RESET_REQ_SZ        0x01
#define RESET_REQ               0x01 // auto clears

/*
 * Parameter pages
 */

/* parameter page definitions */
#define P_PAGE_0                0x00 // No-op, data owned by BHI160
#define P_PAGE_SYSTEM           0x01 // System global parameters
#define P_PAGE_ALGORITHM        0x02 // Algoritm coefficients and knobs
#define P_PAGE_SENSORS          0x03 // Sensor (real or virtual) parameters
#define P_PAGE_CUST12           0x0c // Custom pages, any purpose
#define P_PAGE_CUST13           0x0d
#define P_PAGE_CUST14           0x0e

/* Parameter page 1, SYSTEM, request numbers */
#define P1_META_EVT_CTRL       0 // meta event control, 8bytes
/* 2 bits per event, MSB event enable, LSB int enable. Big Endian, event 1 is bits 0,1 of byte 0. */

#define P1_FIFO_CTRL           2 // Fifo control, 8 bytes, all entries Little Endian
struct __attribute__((__packed__)) p1_fifo_ctrl {
	uint16_t wkp_watermark; // R/W
	uint16_t wkp_fifo_size; // R/O
	uint16_t nwk_watermark; // R/W
	uint16_t nwk_fifo_size; // R/O
}; /**/

#define P1_SENS_STAT_0         3 // Sensors 1-16 status bytes
#define P1_SENS_STAT_1         4 // Sensors 17-32 status bytes
#define P1_SENS_STAT_2         5 // Sensors 33-48 status bytes
#define P1_SENS_STAT_3         6 // Sensors 49-64 status bytes
/* Status byte per sensor */
#define SENS_STAT_DATA         0x01 // Data available, samples in output buffer
#define SENS_STAT_NACK         0x02 // Sensor didn't ACK I2C
#define SENS_STAT_ID_ERR       0x04 // sensor ID register mismatch (different device on address)
#define SENS_STAT_TRANS_ERR    0x08 // Transient error, a.g. magnetic glitch
#define SENS_STAT_DTA_LOSS     0x10 // FIFO overflow
#define SENS_STAT_PWR_MSK      0xe0 // Sensor power mode mask
#define SENS_STAT_PWR_NA       (0x00 << 5) // Sensor not present
#define SENS_STAT_PWR_PD       (0x01 << 5) // Power down
#define SENS_STAT_PWR_SUS      (0x02 << 5) // suspend
#define SENS_STAT_PWR_ST       (0x03 << 5) // Self-test
#define SENS_STAT_PWR_IM       (0x04 << 5) // Interrupt motion
#define SENS_STAT_PWR_OS       (0x05 << 5) // One shot
#define SENS_STAT_PWR_LPA      (0x06 << 5) // Low power active
#define SENS_STAT_PWR_ACT      (0x07 << 5) // Active

static inline const char *bhi_160_power_mode_string(uint8_t status)
{
	const char *pm[] = {" NA", " PD", "SUS", " ST", " IM", " OS", "LPA", "ACT" };
	return pm[status >> 5];
}

#define P1_META_WKUP_EVT_CTRL  29 // meta event control, 8bytes, 2 bits per event, MSB enable, LSB int

#define P1_IRQ_TS              30 // Host IRQ timestamp 1/32000 seconds, Big endian
struct __attribute__((__packed__)) p1_irq_time {
	uint32_t irq_ts; // timestamp of last IRQ (can be also read from reg 0x6c)
	uint32_t ts;     // Current time
};

#define P1_PHYS_SENS_STAT      31 // Physical sensor status, debug info
struct __attribute__((__packed__)) p1_phys_sens_status {
	uint16_t acc_sample_rate; // [Hz] accelerometer sampling rate
	uint16_t acc_dynamic_range; // [gs] accelerometer dynamic range
	uint8_t  acc_flags; // 0 int en, 5-7 sensor power mode
	uint16_t gyr_sample_rate; // [Hz] gyro sample rate
	uint16_t gyr_dynamic_range; // [?] gyro dynamic range
	uint8_t  gyr_flags; // same as above
	uint16_t mag_sample_rate; // [Hz] magetic sample rate
	uint16_t mag_dynamic_range; // [?] magnetic dynamic range
	uint8_t  mag_flags; // same as above
};

#define P1_PHYS_SENS_PRESENT   32 // 64-bit bitmap of physical sensors
#define P1_PHYS_SENS_INFO(n)   (32 + (n)) // Physical sensor info
struct __attribute__((__packed__)) p1_phys_sens_info {
	uint8_t id; // Same as param number - 32
	uint8_t driver_id;
	uint8_t driver_ver;
	uint8_t current; // [0.1mA]
	uint16_t range; // Current dynamic range of sensor in SI units
	uint8_t flags;  // bit 0 IRQ enabled, Bits 5-7 power mode
	uint8_t res; // Reserved
	uint16_t samplerate; // Current sample rate [Hz]
	uint8_t axes; // Number of axes
	uint8_t m[5]; // Orientation matrix
	#define SIGN_EXTEND4(v) (((v) & 0xf) | (((v) & 0x8)?(-1 & ~0xf):0))
	#define SIGN_EXTEND_HIGH4(v) SIGN_EXTEND4((v) >> 4)
	#define SIGN_EXTEND_LOW4(v)  SIGN_EXTEND4((v) & 0xf)
	#define MATRIX_C(m,n) ((n & 1)?SIGN_EXTEND_HIGH4((m)[(n)/2]):SIGN_EXTEND_LOW4((m)[(n)/2]))
	/*
	 * One signed nibble per matrix element, C1 << 4 + C0, etc.
	 * MSB of last byte is unused and = 0.
	 * Sensor reports axes as:
	 * |X|              |C0 C1 C2|
	 * |Y| = [Xs,Ys,Zs]*|C3 C4 C5|
	 * |Z|              |C6 C7 C8|
	 */
};


/* Parameter page 3, SENSORS, config numbers for non-wakeup sensor info */
#define P3_ACC                  1 // Accelerometer
#define P3_MAG                  2 // Geomagnetic field
#define P3_ORIENT               3 // Orientation
#define P3_GYR                  4 // Gyroscope
#define P3_LIGHT                5 // Light sensor
#define P3_PRESSURE             6 // Pressure sensor
#define P3_TEMP                 7 // Temperature
#define P3_PROX                 8 // Proximity
#define P3_GRAV                 9 // Gravity
#define P3_LACC                10 // Linear Acceleration
#define P3_ROT                 11 // Rotation vector
#define P3_HUM                 12 // Humidity
#define P3_ATEMP               13 // Ambient temperature
#define P3_UMAG                14 // Uncalibrated magnetic field
#define P3_GROTV               15 // Game rotation vector
#define P3_UGYR                16 // Uncalibrated gyro
#define P3_SIGMO               17 // Significant motion
#define P3_STEP                19 // Step detector
#define P3_STEPCOUNT           19 // Step counter
#define P3_MAGROT              20 // Geomagnetic rotation vector
#define P3_HEART               21 // Heart rate
#define P3_TILT                22 // Tilt detector
#define P3_WAKE                23 // Wake gesture
#define P3_GLANCE              24 // Glance gesture
#define P3_PICKUP              25 // Pick up gesture
#define P3_ACTIVITY            31 // Activity recognition

#define P3_TYPE_WKP            31 // Add to config numbers above for wakeup sensor
#define P3_TYPE_NWK             0 // Add to config numbers above for non-wakeup sensor

#define P3_CONFIG_FLAG         64 // Add to access sensor config

// Add 64 for sensor configuration (writeable)
struct __attribute__((__packed__)) p3_sensor_info {
	uint8_t sensor_type; // Repeats the config number
	uint8_t driver_id;   // Unique per driver / vendor / part_no
	uint8_t driver_ver;  // 
	uint8_t power;       // consumption [0.1mA]
	uint16_t max_range;  // Maximum sensor range in SI units (fractional?)
	uint16_t resolution; // number of bits
	uint16_t max_rate;   // [Hz]
	uint16_t fifo_res;   // packet entries in fifo reserved for this sensor. 0 for shared.
	uint16_t fifo_max;   // total fifo size / packet size
	uint8_t  event_size; // bytes per packet, including type header
	uint8_t  min_rate;   // [Hz] minimum sample rate
};
struct __attribute__((__packed__)) p3_sensor_config {
	uint16_t sample_rate; // requested rate, reads back actual
	uint16_t max_latency; // Maximum report latency. [ms], reads back actual
	uint16_t sensitivity; // Scaled the same as sensor data value
	uint16_t dyn_range;   // dynamic range [g, deg/sec, uT]. 0 default
};

/* Enumeration of event data types, in order of struct event declaration */
#define EVT_NOPYLD              0 // No payload, just id
#define EVT_QUATERNION          1 // X,Y,Z,W +Acc
#define EVT_VECTOR              2 // X,Y,Z +Status
#define EVT_SCALAR_U16          3 // 16 bit unsigned
#define EVT_SCALAR_S16          4 // 16 bit signed
#define EVT_SCALAR_U24          5 // 24 bit unsigned
#define EVT_EVENT               6 // single byte event value
#define EVT_VECTOR_UNCAL        7 // 16 bit signed XYZ, signed bias XYZ, status
#define EVT_SCALAR_u8           8 // single byte scalar
#define EVT_BIN16               9 // bit field
#define EVT_DEBUG              10 // Debug event
#define EVT_TIME               11 // Time stamp
#define EVT_RAW                12 // Raw acc, mag and gyr
#define EVT_META               13 // Meta event

/* Event packet format
 * FIFO may be padded with 0 bytes (NOP, treat as empty packet)
 * size is struct + 1
 */
struct __attribute__((__packed__)) event {
	uint8_t  id; // Sensor ID ( & 0x1F - sensor, 0x20 - non-wkup / wkup)
	// Events without payload
	// 0  = NOP
	// 17 = Significant motion
	// 18 = Step detect
	// 22 = Tilt detector
	// 23 = Wake gesture
	// 24 = Glance gesture
	// 25 = Pick up gesture
	// For other sensor events use below structures
	union {
		struct __attribute__((__packed__)) {
			// Quaternion, scale 2^14[rad], signed BE, Sensors 11, 15, 20 (&0x1f)
			int16_t x; // rotation axis X
			int16_t y; // rotation axis Y
			int16_t z; // rotation axis Z
			int16_t w; // rotation value
			int16_t acc; // estimated accuracy
		} rotv;
		struct __attribute__((__packed__)) {
			// Vector & status, scale dynamic (except sensor 3). Sensors 1,2,3,4,9,10
			// Sensor 3 uses 360/2^15. X=azimuth=[0,360], Y=pitch=[-180,180], Z=roll=[-90,90]
			int16_t x; // X or azimuth
			int16_t y; // Y or pitch
			int16_t z; // Z or roll
			uint8_t status; // accuracy 0 (unrel), 1 (low), 2 (medium), 3 (high)
			#define STATUS_STR(s) (s==0?"unrel":s==1?"low":s==2?"med":s==3?"high":"???")
		} vector;
		struct __attribute__((__packed__)) {
			// Absolute scalar, sensors 5,8,12,19,21
			// Sensor 5  (Light): 10000Lux/2^16
			// Sensor 8  (Proximity): 100cm/2^16
			// Sensor 12 (rel humidity): 1%RH
			// Sensor 19 (steps): stepcount
			// Sensor 21 
			uint16_t v;
		} abs_scalar;
		struct __attribute__((__packed__)) {
			// Signed scalar, sensors 7,13
			// Sensors 7,13 (temp, ambient t) 500LSB/1C, centered at 24C
			int16_t v;
		} scalar16;
		struct __attribute__((__packed__)) {
			// unsigned 24 bit scalar, sensor 6 (baro), scale 1/128Pa
			uint8_t lsb;  // Least significant byte
			uint16_t msw; // most significant 16bit (2Pa)
		} abs_scalar24;
		struct __attribute__((__packed__)) {
			// uncalibrates 16bit signed + bias info, scale dynamic, Sensors  14,16
			int16_t x;
			int16_t y;
			int16_t z;
			int16_t xbias;
			int16_t ybias;
			int16_t zbias;
			uint8_t status; // accuracy 0 (unrel), 1 (low), 2 (medium), 3 (high)
		} uncal_vector;
		struct __attribute__((__packed__)) {
			// Printf(text)/fwrite(bin) from SDK, Sensor 245
			uint8_t info; // bits 0-5 = bytes; bit 6 = ASCII/BIN(1)
			uint8_t payload[12]; // payload, length from info.
		} debug;
		struct __attribute__((__packed__)) {
			uint16_t v;
		} time;
		/* TODO: Raw */
		struct __attribute__((__packed__)) {
			// Meta events, sensor 254, 248
#define META_FLUSH 1      // FIFO flush complete (sensor)
#define META_SAMPLERATE 2 // Sample rate changed (sensor)
#define META_POWER      3 // Power mode changed (sensor,power_mode)
#define META_ERROR      4 // Error (error_reg, debug_state)
#define META_SERROR    11 // Sensor Error (sensor, status_bits)
#define META_OVERFLOW  12 // FIFO overflow (loss_count_lsb, _msb)
#define META_DRANGE    13 // dynamic range changed (sensor)
#define META_WATERMARK 14 // FIFO Watermark (bytes_remaining_lsb,_msb)
#define META_SELFTEST  15 // Selftest results (sensor, result)
#define META_INIT      16 // Initialised (ram_ver_lsb, _msb)
			uint8_t ev;     // Event number
			uint8_t sensor; // sensor type
			uint8_t v;      // event specific value
		} meta;
	};
};


/* Default decode functions, print info. */

struct event_info;
struct bhi160_state;

void bhi160_decode_vector(struct bhi160_state *S, struct event_info *EI);
void bhi160_decode_quaternion(struct bhi160_state *S, struct event_info *EI);
void bhi160_decode_scalar(struct bhi160_state *S, struct event_info *EI);
void bhi160_decode_vector_uncal(struct bhi160_state *S, struct event_info *EI);
void bhi160_decode_raw(struct bhi160_state *S, struct event_info *EI);
void bhi160_decode_event(struct bhi160_state *S, struct event_info *EI);
void bhi160_decode_bin(struct bhi160_state *S, struct event_info *EI);
void bhi160_decode_debug(struct bhi160_state *S, struct event_info *EI);
void bhi160_decode_time(struct bhi160_state *S, struct event_info *EI);
void bhi160_decode_meta(struct bhi160_state *S, struct event_info *EI);

/* Event information type */
struct event_info {
	int id;           // Event id (stripped of wakeup bit)
	int wkup;         // 1 Wakeup event, 0 non-wakeup
	#define WKUP_D -1 // Dynamic wkup flag
	int size;         // Event size in bytes
	int type;         // EVT_ above
	float scale;     // Base scaling factor
	float ascale;    // Actual scale factor, adjusted by current sensor range
	#define SCALE_N (0.0) // No scale
	const char *unit; // Physical unit
	const char *sensor; // Sensor name
	void (*cb)(struct bhi160_state *S, struct event_info *EI); // Decode function
	struct event *ev; // Pointer to event data
};

// returns 2^n as a float
#define POW2(n) ((float)(1UL << (n)))

// Array index matches id up to 25, then continues with 31 and then from id=245 onwards
// The constant array gets copied into state variable at runtime
// callbacks point to debug functions to start with, can be redirected.
// Scaling factor can change during operation, for dynamic scaling
// TODO: only copy dynamic values into a striped down runtime structure
#ifndef __cplusplus // g++ refuses to compile this. We only need it in C code, so removing for C++
#define bhi160_Gev_SIZE (sizeof(bhi160_GEv)/sizeof(struct event_info))
static const struct event_info bhi160_GEv[] = {
	{ .id = 0, .wkup = WKUP_D, .size = 1, .type = EVT_NOPYLD,
	  .scale = SCALE_N, .unit="", .sensor="pad", .cb = 0, .ev = 0},
	{ .id = 1, .wkup = WKUP_D, .size = 8, .type = EVT_VECTOR,
	  .scale = 1.0 / POW2(15), .unit="g", .sensor="acc", .cb = bhi160_decode_vector, .ev = 0},
	{ .id = 2, .wkup = WKUP_D, .size = 8, .type = EVT_VECTOR,
	  .scale = 1.0 / POW2(15), .unit="uT", .sensor="mag", .cb = bhi160_decode_vector, .ev = 0},
	{ .id = 3, .wkup = WKUP_D, .size = 8, .type = EVT_VECTOR,
	  .scale = 360.0 / POW2(15), .unit="deg", .sensor="orient", .cb = bhi160_decode_vector, .ev = 0},
	{ .id = 4, .wkup = WKUP_D, .size = 8, .type = EVT_VECTOR,
	  .scale = 1.0 / POW2(15), .unit="d/s", .sensor="gyr", .cb = bhi160_decode_vector, .ev = 0},
	{ .id = 5, .wkup = WKUP_D, .size = 3, .type = EVT_SCALAR_U16,
	  .scale = 1.0 / POW2(16), .unit="lux", .sensor="light", .cb = bhi160_decode_scalar, .ev = 0},
	{ .id = 6, .wkup = WKUP_D, .size = 4, .type = EVT_SCALAR_U24,
	  .scale = 1.0 / POW2(7), .unit="Pa", .sensor="baro", .cb = bhi160_decode_scalar, .ev = 0},
	{ .id = 7, .wkup = WKUP_D, .size = 3, .type = EVT_SCALAR_S16,
	  .scale = 1.0 / 500.0, .unit="C", .sensor="temp", .cb = bhi160_decode_scalar, .ev = 0}, // Centered at 24C
	{ .id = 8, .wkup = WKUP_D, .size = 3, .type = EVT_SCALAR_U16,
	  .scale = 100.0 / POW2(16), .unit="cm", .sensor="prox", .cb = bhi160_decode_scalar, .ev = 0},
	{ .id = 9, .wkup = WKUP_D, .size = 8, .type = EVT_VECTOR,
	  .scale = 1.0 / POW2(15), .unit="g", .sensor="grav", .cb = bhi160_decode_vector, .ev = 0},
	{ .id = 10, .wkup = WKUP_D, .size = 8, .type = EVT_VECTOR,
	  .scale = 1.0 / POW2(15), .unit="g", .sensor="linac", .cb = bhi160_decode_vector, .ev = 0},
	{ .id = 11, .wkup = WKUP_D, .size = 11, .type = EVT_QUATERNION,
	  .scale = 1.0 / POW2(14), .unit="rad", .sensor="rotv", .cb = bhi160_decode_quaternion, .ev = 0},
	{ .id = 12, .wkup = WKUP_D, .size = 3, .type = EVT_SCALAR_U16,
	  .scale = 1.0, .unit="%rl", .sensor="humid", .cb = bhi160_decode_scalar, .ev = 0},
	{ .id = 13, .wkup = WKUP_D, .size = 3, .type = EVT_SCALAR_S16,
	  .scale = 1.0 / 500.0, .unit="C", .sensor="atemp", .cb = bhi160_decode_scalar, .ev = 0}, // Centered at 24C
	{ .id = 14, .wkup = WKUP_D, .size = 14, .type = EVT_VECTOR_UNCAL,
	  .scale = 1.0 / POW2(15), .unit="uT", .sensor="mag unc", .cb = bhi160_decode_vector_uncal, .ev = 0},
	{ .id = 15, .wkup = WKUP_D, .size = 11, .type = EVT_QUATERNION,
	  .scale = 1.0 / POW2(14), .unit="rad", .sensor="rotg", .cb = bhi160_decode_quaternion, .ev = 0},
	{ .id = 16, .wkup = WKUP_D, .size = 14, .type = EVT_VECTOR_UNCAL,
	  .scale = 1.0 / POW2(15), .unit="d/s", .sensor="gyr unc", .cb = bhi160_decode_vector_uncal, .ev = 0},
	{ .id = 17, .wkup = WKUP_D, .size = 2, .type = EVT_EVENT,
	  .scale = SCALE_N, .unit="", .sensor="sigmo", .cb = bhi160_decode_event, .ev = 0},
	{ .id = 18, .wkup = WKUP_D, .size = 2, .type = EVT_EVENT,
	  .scale = SCALE_N, .unit="", .sensor="step", .cb = bhi160_decode_event, .ev = 0},
	{ .id = 19, .wkup = WKUP_D, .size = 3, .type = EVT_SCALAR_U16,
	  .scale = 1.0, .unit="sts", .sensor="step", .cb = bhi160_decode_scalar, .ev = 0},
	{ .id = 20, .wkup = WKUP_D, .size = 11, .type = EVT_QUATERNION,
	  .scale = 1.0 / POW2(14), .unit="rad", .sensor="magrotv", .cb = bhi160_decode_quaternion, .ev = 0},
	{ .id = 21, .wkup = WKUP_D, .size = 2, .type = EVT_SCALAR_u8,
	  .scale = 1.0, .unit="bpm", .sensor="heart", .cb = bhi160_decode_scalar, .ev = 0},
	{ .id = 22, .wkup = WKUP_D, .size = 2, .type = EVT_EVENT,
	  .scale = SCALE_N, .unit="", .sensor="tilt", .cb = bhi160_decode_event, .ev = 0},
	{ .id = 23, .wkup = WKUP_D, .size = 2, .type = EVT_EVENT,
	  .scale = SCALE_N, .unit="", .sensor="wake", .cb = bhi160_decode_event, .ev = 0},
	{ .id = 24, .wkup = WKUP_D, .size = 2, .type = EVT_EVENT,
	  .scale = SCALE_N, .unit="", .sensor="glance", .cb = bhi160_decode_event, .ev = 0},
	{ .id = 25, .wkup = WKUP_D, .size = 2, .type = EVT_EVENT,
	  .scale = SCALE_N, .unit="", .sensor="pickup", .cb = bhi160_decode_event, .ev = 0},
/* Skip */
	{ .id = 31, .wkup = WKUP_D, .size = 3, .type = EVT_BIN16,
	  .scale = SCALE_N, .unit="", .sensor="activity", .cb = bhi160_decode_bin, .ev = 0},
/* Skip */
	{ .id = 245, .wkup = WKUP_D, .size = 14, .type = EVT_DEBUG,
	  .scale = SCALE_N, .unit="", .sensor="debug", .cb = bhi160_decode_debug, .ev = 0},
	{ .id = 246, .wkup = 1, .size = 3, .type = EVT_TIME,
	  .scale = 1.0 / 32000.0, .unit="", .sensor="TS LSW", .cb = bhi160_decode_time, .ev = 0},
	{ .id = 247, .wkup = 1, .size = 3, .type = EVT_TIME,
	  .scale = 1.0 / 32000.0, .unit="", .sensor="TS MSW", .cb = bhi160_decode_time, .ev = 0},
	{ .id = 248, .wkup = 1, .size = 4, .type = EVT_META,
	  .scale = SCALE_N, .unit="", .sensor="meta", .cb = bhi160_decode_meta, .ev = 0},
	{ .id = 249, .wkup = 0, .size = 17, .type = EVT_RAW,
	  .scale = 1.0 / POW2(15), .unit="d/s", .sensor="raw gyr", .cb = bhi160_decode_raw, .ev = 0},
	{ .id = 250, .wkup = 0, .size = 17, .type = EVT_RAW,
	  .scale = 1.0 / POW2(15), .unit="uT", .sensor="raw mag", .cb = bhi160_decode_raw, .ev = 0},
	{ .id = 251, .wkup = 0, .size = 17, .type = EVT_RAW,
	  .scale = 1.0 / POW2(15), .unit="g", .sensor="raw acc", .cb = bhi160_decode_raw, .ev = 0},
	{ .id = 252, .wkup = 0, .size = 3, .type = EVT_TIME,
	  .scale = 1.0 / 32000.0, .unit="", .sensor="TS LSW", .cb = bhi160_decode_time, .ev = 0},
	{ .id = 253, .wkup = 0, .size = 3, .type = EVT_TIME,
	  .scale = 1.0 / 32000.0, .unit="", .sensor="TS MSW", .cb = bhi160_decode_time, .ev = 0},
	{ .id = 254, .wkup = 0, .size = 4, .type = EVT_META,
	  .scale = SCALE_N, .unit="", .sensor="meta", .cb = bhi160_decode_meta, .ev = 0},
};
#else
// We need just the size for C++. WARNING: Make sure this matches the number of elements in the initialiser above
#define bhi160_Gev_SIZE (37)
#endif // __cplusplus

/* API */

/* Returns pointer to info structure for given event id.
   NULL for unknown. Info structure pointer is only valid
   until next call to this or poll. */
struct event_info * event_info(struct bhi160_state *S, uint8_t id);

// The length of a time tick in seconds
#define TIME_SCALE (1.0/32000.0)

struct bhi160_state {
	uint32_t Time; // Current sample time in ticks [1/32000 s].
	uint16_t wkp_fifo_size; // Wakeup FIFO size reported by firmware
	uint16_t nwk_fifo_size; // Non-wakeup FIFO size reported by firmware
	struct i2c_handle i2c; // I2C library stuff
	struct event_info GEv[bhi160_Gev_SIZE]; // Per sensor state and info
};

int bhi160_open(struct bhi160_state *S);
void bhi160_close(struct bhi160_state *S);

/* Resets device and uploads specified firmware into fuser core RAM. */
int bhi160_upload_firmware(struct bhi160_state *S, const uint8_t *fw, int size);


void bhi160_dump_sensor_info(struct bhi160_state *S, uint8_t id);

// Dumps parameter page3 sensor configuration
void bhi160_dump_sensor_config(struct bhi160_state *S, uint8_t id);

// Reads physically present sensors and dumps their information
void bhi160_dump_phys_sensors_present(struct bhi160_state *S);

/* Sets a sensor's rotational mapping.
 * 
 * The mapping is a matrix that can rotate and/or mirror sensor axes
 * to the real world. Matrix elements must be 1, 0 or -1, representing
 * 90deg rotations and mirror operations only.
 * 
 * Only the physical sensors can have a mapping set. Call
 * bhi160_dump_phys_sensors_present to see the available
 * physical sensors and their current mapping.
 *
 * | X |   | M00 M01 M02 |   | Xs |
 * | Y | = | M10 M11 M12 | * | Ys |
 * | Z |   | M20 M21 M22 |   | Zs |
 *
 * To calculate a new mapping, determine which axes need rotating, then
 * multiply that rotation matrix with the original one. Make sure that
 * axes are mapped into a right hand coordinate system.
 */
int bhi160_set_mapping(struct bhi160_state *S, uint8_t id, const int8_t mapping[3][3]);

int bhi160_set_sensor_config(
		struct bhi160_state *S,
		uint8_t id, // Sensor ID, including wakeup/non-wakeup
		uint16_t *samplerate,  // Sample rate [Hz], 0 to disable
		uint16_t *latency,     // Max allowed latency [ms]. Set to 0 for non-batch mode
		uint16_t *sensitivity, // ? Sensitivity, scaled to sensor scaling value
		uint16_t *dynrange);   // Dynamic range in SI units [uT], [g], [deg/s]

/* Checks the event FIFO and reads one event if available.
   Returns negative error, 0 if no-op, 1 for event and 2 for event & more pending. */
int bhi160_poll_fifo(struct bhi160_state *S, struct event_info **ev_info);

#ifdef __cplusplus
} // extern "C"
#endif

# endif // !_BHI_160_H
