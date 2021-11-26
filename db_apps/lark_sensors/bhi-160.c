/*
 * Bosch BHI-160 sensor access library
 *
 * Iwo.Mergler@netcommwireless.com
 */

#define _XOPEN_SOURCE // For usleep in unistd.h

#include <bhi-160.h>
#include "i2c_linux.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h> //malloc/free
#include "log.h"

// Set to 0, 1, 2 or 3
// 0 = Nothing
// 1 = Errors and mode change
// 2 = + Minimal data events
// 3 = + All other events and config
// 4 = + Everything, will affect operation
#ifdef CLI_TEST
#define DLEVEL 2
#endif

#ifdef BHI_TEST
#define DLEVEL 4
#endif

#ifndef DLEVEL
#define DLEVEL 1
#endif

// TODO: split constant sensor data from debug strings
void hdump(const void *buf, int len)
{
	int i=0,n=0;
	const unsigned char *b=buf;
	unsigned char c;
	char Can[17];
	while (len > 0)
	{
		if (!n) {
			printf("[%04X] ",i);
			i+=16;
		}
		c = *b++;
		printf("%02X ",c);
		Can[n]=(c<' ' || c>'~')?'.':c; Can[n+1]='\0';
		n++; len--;
		if (!len) {
			int cd = 16-n;
			while (cd--) printf("   ");
		}
		if (n >= 16) n=0;
		if(!n || !len) printf("|%s|\n",Can);
	}
}

void hdump_syslog(const void *buf, int len)
{
    char* log;
    const unsigned char *b=buf;

    if(!buf || !len) {
        return;
    }

    log = (char*) malloc(len*3 + 1);
    if(!log) {
        return;
    }

    for(int i=0; i<len; i++) {
        sprintf(log + 3*i, "%02X ", b[i]);
    }

    LS_DEBUG("%s", log);
    free(log);
}

#define NOPRINT(fmt, ...) do { (void)fmt; } while(0)

#if defined(BHI_TEST) || defined(CLI_TEST)
#define PRINTF  printf
#define HDUMP   hdump
#else
#define PRINTF  LS_DEBUG
#define HDUMP   hdump_syslog
#endif

#if DLEVEL >= 1
#define DBG1 PRINTF
#define HEX1 HDUMP
#else
#define DBG1 NOPRINT
#define HEX1 NOPRINT
#endif

#if DLEVEL >= 2
#define DBG2 PRINTF
#define HEX2 HDUMP
#else
#define DBG2 NOPRINT
#define HEX2 NOPRINT
#endif

#if DLEVEL >= 3
#define DBG3 PRINTF
#define HEX3 HDUMP
#else
#define DBG3 NOPRINT
#define HEX3 NOPRINT
#endif

#if DLEVEL >= 4
#define DBG4 PRINTF
#define HEX4 HDUMP
#else
#define DBG4 NOPRINT
#define HEX4 NOPRINT
#endif

// Check integer value against 0
#define CHECK_EXIT(rval) { int r; r = (rval); if (r) { DBG1("%d: CHECK_EXIT(%s)\n", __LINE__, r); exit(-1); }}

#include <stdlib.h>
#define ASSERT(cond) if (!(cond)) {DBG1("%d: Assert(%s)\n", __LINE__, #cond); exit(-1);}


int bhi160_open(struct bhi160_state *S)
{
	int r;
	unsigned i;

	r = i2c_open(&S->i2c, BHI160_I2C_BUS, BHI160_I2C_ADDR);
	if (r) return r;

	S->Time = 0.0;
	S->wkp_fifo_size = 0;
	S->nwk_fifo_size = 0;
	memcpy(S->GEv, bhi160_GEv, sizeof(bhi160_GEv));

	// Set actual scaling factor to be the same as base factor
	// System adjusts these later based on scale factor events from sensors
	for (i = 0; i < bhi160_Gev_SIZE; i++) {
		S->GEv[i].ascale = S->GEv[i].scale;
	}

	return 0;
}

void bhi160_close(struct bhi160_state *S)
{
	i2c_close(&S->i2c);
	S->Time = 0.0;
}


#include <i2c_linux.h>

static int sensor_write(struct bhi160_state *S, uint8_t reg, uint8_t *data, int bytes)
{
	return i2c_writeblock(&S->i2c, reg, data, bytes);
}

static int sensor_read(struct bhi160_state *S, uint8_t reg, uint8_t *data, int bytes)
{
	return i2c_readblock(&S->i2c, reg, data, bytes);
}


/* Operation wrappers for specific types, including endianness conversion */
static int sensor_write_8(struct bhi160_state *S, uint8_t reg, uint8_t data)
{
	return sensor_write(S, reg, &data, 1);
}

static int sensor_write_be16(struct bhi160_state *S, uint8_t reg, uint16_t data_be)
{
	uint8_t d[2];
	d[0] = data_be >> 8;
	d[1] = data_be & 0xff;
	return sensor_write(S, reg, d, sizeof(d));
}

/* Returns >= 0 value or < 0 for error */
static int sensor_read_8(struct bhi160_state *S, uint8_t reg)
{
	uint8_t v;
	int r;
	r = sensor_read(S, reg, &v, 1);
	if (r) return -1;
	return (int)v;
}

/* Returns >= 0 value or < 0 for error */
static int sensor_read_le16(struct bhi160_state *S, uint8_t reg)
{
	int r;
	uint16_t v = 0;
	uint8_t d[2];
	r = sensor_read(S, reg, d, 2);
	if (r) return r;
	v <<= 8; v |= d[1];
	v <<= 8; v |= d[0];
	return (int)v;
}

static int sensor_read_le32(struct bhi160_state *S, uint8_t reg, uint32_t *vp)
{
	int r;
	uint32_t v = 0;
	uint8_t d[4];
	r = sensor_read(S, reg, d, 4);
	if (r) return r;
	v <<= 8; v |= d[3];
	v <<= 8; v |= d[2];
	v <<= 8; v |= d[1];
	v <<= 8; v |= d[0];
	*vp = v;
	return r;
}

#if DLEVEL >= 1
static void bhi160_dump_reg(struct bhi160_state *S, const char *name, int reg, int bytes)
{
	int i,s;
	auto uint8_t v[bytes];

	DBG1("%20s [%02x] ", name, reg);

	s = sensor_read(S, reg, v, bytes);
	ASSERT(s == 0);

	for (i = 0; i < bytes; i++) {
		DBG1(" %02x", v[i]);
	}
	DBG1("  ");

	switch (reg) {
		case REG_CHIP_CTRL:
			if (*v & CHIP_CTRL_CPU_RUN) DBG1("CPU_RUN,");
			if (*v & CHIP_CTRL_UPLD_EN) DBG1("UPLOAD,");
			break;
		case REG_HOST_STS:
			if (*v & HOST_STS_RESET) DBG1("RESET,");
			if (*v & HOST_STS_ALG_STBY) DBG1("STBY,");
			if (*v & HOST_STS_IF_ID_MSK) DBG1("AndroidL,");
			break;
		case REG_INT_STS:
			if (*v & INT_STS_HOST) DBG1("HOST,");
			if (*v & INT_STS_WKUP_WMRK) DBG1("WKUPWM,");
			if (*v & INT_STS_WKUP_LAT) DBG1("WKUPLA,");
			if (*v & INT_STS_WKUP_IMM) DBG1("WKUPIM,");
			if (*v & INT_STS_NWK_WMRK) DBG1("NWKWM,");
			if (*v & INT_STS_NWK_LAT) DBG1("NWKLA,");
			if (*v & INT_STS_NWK_IMM) DBG1("NWKIM,");
			break;
		case REG_CHIP_STS:
			if (*v & CHIP_STS_EE_DET) DBG1("EEPROM ");
			if (*v & CHIP_STS_EE_UPLD_DONE) DBG1("UPLOK ");
			if (*v & CHIP_STS_EE_UPLD_ERR) DBG1("UPLER ");
			if (*v & CHIP_STS_FW_IDLE) DBG1("IDLE ");
			if (*v & CHIP_STS_NO_EE) DBG1("NOEE ");
			break;
		
		default:
			break;
	}

	DBG1("\n");
}
#define DUMP_REG(S,R) bhi160_dump_reg(S,#R, R, R##_SZ)
#else
#define DUMP_REG(S,R) do { (void)R; } while(0)
#endif

void bhi160_dump_all(struct bhi160_state *S)
{
	DUMP_REG(S,REG_FIFO_FLUSH);
	DUMP_REG(S,REG_CHIP_CTRL);
	DUMP_REG(S,REG_HOST_STS);
	DUMP_REG(S,REG_INT_STS);
	DUMP_REG(S,REG_CHIP_STS);
	DUMP_REG(S,REG_BYTES_REM);
	DUMP_REG(S,REG_PRM_ACK);
	DUMP_REG(S,REG_PRM_READ);
	DUMP_REG(S,REG_PRM_PAGE_SEL);
	DUMP_REG(S,REG_HOST_INF_CTRL);
	DUMP_REG(S,REG_PRM_REQ);
	DUMP_REG(S,REG_ROM_VER);
	DUMP_REG(S,REG_RAM_VER);
	DUMP_REG(S,REG_PROD_ID);
	DUMP_REG(S,REG_REV_ID);
	DUMP_REG(S,REG_UPLD_ADDR);
	DUMP_REG(S,REG_UPLD_CRC);
	DUMP_REG(S,REG_RESET_REQ);
}


void bhi160_decode_vector(struct bhi160_state *S, struct event_info *EI)
{
	struct event *E = EI->ev;
	const char *coo = "XYZ";
	if (EI->id == P3_ORIENT) {
		coo = "APR"; // Azimuth, pitch, roll
	}
	DBG2("%7.3f %7s [%-3s] %c=%10.5f, %c=%10.5f, %c=%10.5f, s=%d(%s)\n",
		(double)S->Time * TIME_SCALE, EI->sensor, EI->unit,
		coo[0], (float)E->vector.x * EI->ascale,
		coo[1], (float)E->vector.y * EI->ascale,
		coo[2], (float)E->vector.z * EI->ascale,
		E->vector.status, STATUS_STR(E->vector.status));
}

void bhi160_decode_quaternion(struct bhi160_state *S, struct event_info *EI)
{
	(void)S;
	(void)EI;
	DBG2("TODO: decode quaternion\n");
}

void bhi160_decode_scalar(struct bhi160_state *S, struct event_info *EI)
{
	(void)S;
	(void)EI;
	DBG2("TODO: decode scalar\n");
}

void bhi160_decode_vector_uncal(struct bhi160_state *S, struct event_info *EI)
{
	(void)S;
	(void)EI;
	DBG2("TODO: decode vector uncal\n");
}

void bhi160_decode_raw(struct bhi160_state *S, struct event_info *EI)
{
	(void)S;
	(void)EI;
	DBG2("TODO: decode raw\n");
}

void bhi160_decode_event(struct bhi160_state *S, struct event_info *EI)
{
	(void)S;
	(void)EI;
	DBG2("TODO: decode event\n");
}

void bhi160_decode_bin(struct bhi160_state *S, struct event_info *EI)
{
	(void)S;
	(void)EI;
	DBG2("TODO: decode bin\n");
}

void bhi160_decode_debug(struct bhi160_state *S, struct event_info *EI)
{
	(void)S;
	(void)EI;
	DBG2("TODO: decode debug\n");
}

void bhi160_decode_time(struct bhi160_state *S, struct event_info *EI)
{
	static uint16_t ts_msw = 0; // Global MSW of timestamp
	struct event *EV = EI->ev;
	if (EI->id == 253 || EI->id == 247) {
		ts_msw = EV->time.v;
		DBG3("Timestamp MSW %04x\n", ts_msw);
	} else {
		uint32_t ts = (uint32_t)ts_msw << 16;
		ts += EV->time.v;
		S->Time = ts;
		DBG3("Timestamp [%08x] %.3f\n", ts, (double)S->Time * EI->scale);
	}
}

void bhi160_decode_meta(struct bhi160_state *S, struct event_info *EI)
{
	struct event *EV = EI->ev;
	uint8_t mev = EV->meta.ev;
	uint8_t sensor = EV->meta.sensor;
	uint8_t v = EV->meta.v;
	uint16_t u16 = ((uint16_t)v << 8) + sensor; // Some events encode a 16 bit number this way
	switch (mev) {
		case META_FLUSH:
			DBG2("Meta flush, sensor %d\n", sensor);
			break;
		case META_SAMPLERATE:
			DBG2("Meta samplerate, sensor %d\n", sensor);
			break;
		case META_POWER:
			DBG2("Meta powermode, sensor %d, mode=%d\n", sensor, v);
			break;
		case META_ERROR:
			DBG2("Meta error, reg %02x, dstate %d\n", sensor, v);
			break;
		case META_SERROR:
			DBG2("Meta sensor error, sensor %d, status %02x\n", sensor, v);
			break;
		case META_OVERFLOW:
			DBG2("Meta FIFO overflow, %d bytes\n", u16);
			break;
		case META_DRANGE:
			DBG2("Meta dynamic range change, sensor %d\n", sensor);
			bhi160_dump_sensor_config(S, sensor); // TODO: Move this into poll itself
			break;
		case META_WATERMARK:
			DBG2("Meta FIFO watermark, %d bytes remaining\n", u16);
			break;
		case META_SELFTEST:
			DBG2("Meta selftest, sensor %d, result %02x\n", sensor, v);
			break;
		case META_INIT:
			DBG2("Meta init, RAM ver %04x\n", u16);
			break;
		default :
			DBG1("Meta unknown event, %d\n", mev);
			break;
	}
}

/* Fetch specified parameter. Returns buffer pointer or NULL for error.
 * WARNING: returned pointer is only valid until next call to this function. */
void * bh160_fetch_parameter(struct bhi160_state *S, uint8_t page, uint8_t parameter, int dump)
{
	static uint8_t buf[REG_PRM_READ_SZ] = {0}; // Maximum parameter size (16 bytes)
	int r;

	DBG3("Accessing parameter page %d, param %d\n", page, parameter);
	
	r = sensor_write_8(S, REG_PRM_PAGE_SEL, page); // Size to 0, for max.
	ASSERT(r == 0);

	r = sensor_write_8(S, REG_PRM_REQ, PRM_REQ_READ | parameter);
	if (r) {
		DBG1("Failed to write param req\n");
		return NULL;
	}

	do {
		r = sensor_read_8(S, REG_PRM_ACK);
		if (r < 0) { DBG1("read failed\n"); return NULL; }
		if (r & PRM_ACK_ERR) { DBG1("PARAM error %02x\n", r); return NULL; }
	} while (r != parameter);

	r = sensor_read(S, REG_PRM_READ, buf, REG_PRM_READ_SZ);

	if (dump) {
		DBG3("PAR(%d,%d) ", page, parameter);
		HEX3(buf, REG_PRM_READ_SZ);
	}

	// Reset to page 0
	r = sensor_write_8(S, REG_PRM_PAGE_SEL, 0);
	ASSERT(r == 0);

	return buf;
}

/* Writes parameter page, max 8 bytes */
int bh160_set_parameter(struct bhi160_state *S, uint8_t page, uint8_t parameter, void *payld, int size)
{
	int i,r;
	uint8_t buf[REG_PRM_WRITE_SZ] = {0}; // Copy of buffer, for 0-padding

	ASSERT(size <= REG_PRM_WRITE_SZ);

	/* Copy into write buffer */
	for (i = 0; i < size; i++) {
		buf[i] = ((uint8_t *)payld)[i];
	}

	r = sensor_write(S, REG_PRM_WRITE, buf, REG_PRM_WRITE_SZ);
	ASSERT(r == 0);

	r = sensor_write_8(S, REG_PRM_PAGE_SEL, page);
	ASSERT(r == 0);

	r = sensor_write_8(S, REG_PRM_REQ, PRM_REQ_WRITE | parameter);
	ASSERT(r == 0);

	do {
		r = sensor_read_8(S, REG_PRM_ACK);
		ASSERT(r >= 0);
		if (r == PRM_ACK_ERR) { DBG1("PARAM error %02x\n", r); return -1; }
	} while (r != (PRM_REQ_WRITE | parameter));

	// Reset to page 0
	r = sensor_write_8(S, REG_PRM_PAGE_SEL, 0);
	ASSERT(r == 0);

	return 0;
}

/* Resets and uploads specified firmware into fuser core RAM. */
int bhi160_upload_firmware(struct bhi160_state *S, const uint8_t *fw, int size)
{
	uint8_t v  = 0;
	int i,j,r;
	uint32_t hcrc, crc = 0;

	// Checking firmware signature magic
	DBG2("Firmware size: %d\n", size);
	if (fw[0] != 0x2A || fw[1] != 0x65) {
		DBG1("Firmware signature failed\n");
		return -1;
	}
	hcrc = ((uint32_t)fw[7] << 24) | ((uint32_t)fw[6] << 16) |
	       ((uint32_t)fw[5] << 8) | (uint32_t)fw[4];

	// Strip header
	fw+=16; size-=16;

	/* Reset the sensor */
	DBG1("Reset\n");
	r = sensor_write_8(S, REG_RESET_REQ, RESET_REQ);
	ASSERT(r==0);

	/* Waiting for idle indication */
	do {
		r = sensor_read_8(S,REG_CHIP_STS);
		ASSERT(r>=0);
	} while ( (r & CHIP_STS_FW_IDLE) == 0);

	v = r;

	DBG1("Chip status: %02x, ", v);
	if (! (v & CHIP_STS_FW_IDLE)) { DBG1("Not idle\n"); return -1; }
	DBG1("ready to accept firmware\n");

	// As an exception, the upload address is BigEndian. 0 default though.
	DBG2("Setting address %d\n", 0);
	r =  sensor_write_be16(S, REG_UPLD_ADDR, 0);
	ASSERT(r==0);

	// Enable upload mode
	DBG2("Starting upload\n");
	r = sensor_write_8(S, REG_CHIP_CTRL, CHIP_CTRL_UPLD_EN);
	ASSERT(r == 0);

	/* Datasheet says this:
	 * Once the host has entered upload mode, it may send the firmware to REG_UPLD_DATA.
	 * Note that the RAM patch file format starts with a 16 byte header that must be stripped.
	 * Every 4 bytes of data after the header must be byte swapped before upload.
	 * 
	 * Do we need to upload in chunks? Yes, Bosch code seems to transfer 32 bytes at a time,
	 * but larger seems to work too, with a fuzzy boundary around 8000 bytes.
	 * 
	 * The CRC is a mystery. Running a standard CRC32, both with 0x0 and 0xffffffff starting
	 * values is not matching the one the sensor is using. But the sensor CRC matches the
	 * one in the firmware file header.
	 */

	#define CHUNK 32
	DBG1("Uploading firmware blocks, %d chunks of <=%d bytes\n", (size+CHUNK-1)/CHUNK, CHUNK);
	for (i = 0; i < size; i += CHUNK) {
		uint8_t chunk[CHUNK];
		int rem = size - i;
		int ts = rem > CHUNK ? CHUNK : rem;

		/* Endian swap all words within this chunk */
		for (j = 0; j < ts; j += 4) {
			chunk[j+0] = fw[i+j+3];
			chunk[j+1] = fw[i+j+2];
			chunk[j+2] = fw[i+j+1];
			chunk[j+3] = fw[i+j+0];
		}

		DBG3("	%d (%d) rem=%d\n", i, ts, rem);


		r = sensor_write(S, REG_UPLD_DATA, chunk, ts);
		ASSERT(r==0);
	}

	// Checking CRC
	r = sensor_read_le32(S, REG_UPLD_CRC, &crc);
	ASSERT(r==0);
	if (crc == hcrc) {
		DBG1("CRC match (%08x)\n", crc);
	} else {
		DBG1("CRC mismatch (h:%08x, s:%08x)\n", hcrc, crc);
		return -1;
	}

	DBG1("Starting Execution\n");
	r = sensor_write_8(S, REG_CHIP_CTRL, CHIP_CTRL_CPU_RUN);
	ASSERT(r==0);

	// Waiting for firmware patch to be operational, RAM version seems a suitable indicator
	do {
		// the delay seems have resolved the problem that BHI160 doesn't start working occasionally.
		// for more information please refer description in RB#10459
		usleep(5 * 1000); // 5ms
		r = sensor_read_le16(S, REG_RAM_VER);
	} while (r == 0);
	DBG2("Running\n");

	// Setting some global parameters from RAM patch
	{
		const struct p1_fifo_ctrl *P1ffc;
		P1ffc = bh160_fetch_parameter(S, P_PAGE_SYSTEM, P1_FIFO_CTRL, 0);
		S->wkp_fifo_size = P1ffc->wkp_fifo_size;
		S->nwk_fifo_size = P1ffc->nwk_fifo_size;
		DBG1("G wkp_fifo=%d, nwk_fifo=%d\n", S->wkp_fifo_size, S->nwk_fifo_size);
	}

	return 0;
}


/* Returns pointer to info structure for given event id.
   NULL for unknown. Info structure is valid until next call to this or poll. */
struct event_info * event_info(struct bhi160_state *S, uint8_t id)
{
	int idx;
	/* Wakeup sensor numbers are [1:31],  non-wkup + 32 [32:63].
	 * Above that are the special events. */
	if (id < 64) {
		idx = id & 31; // Fold wakeup id's into non-wkup ones
		if (idx <= 25) return &S->GEv[idx]; // Ordinary sensor
		if (idx == 31) return &S->GEv[26];  // activity bitfield
		return (struct event_info *)NULL; // Reserved sensor, shouldn't exist
	} else {
		if (id < 245) return (struct event_info *)NULL; // Unknown event, shouldn't exist
		if (id > 254) return (struct event_info *)NULL; // Unknown event, shouldn't exist
		return &S->GEv[id - 245 + 27]; // Last table block
	}
}

//
const char * bhi160_sensor_status_string(uint8_t v)
{
	static char sstr[12] = "";
	char *p = sstr;

	p += sprintf(p, "%s ", bhi_160_power_mode_string(v));

	if (v & SENS_STAT_DATA)      *p++ = 'D'; else *p++ = '.';
	if (v & SENS_STAT_NACK)      *p++ = 'N'; else *p++ = '.';
	if (v & SENS_STAT_ID_ERR)    *p++ = 'E'; else *p++ = '.';
	if (v & SENS_STAT_TRANS_ERR) *p++ = 'T'; else *p++ = '.';
	if (v & SENS_STAT_DTA_LOSS)  *p++ = 'F'; else *p++ = '.';

	*p = '\0';
	return sstr;
}

// Dumps all 4 sensor status banks
void bhi160_dump_sensor_status(struct bhi160_state *S)
{
	uint8_t *stat; // 16 sensors per bank (REG_PRM_READ_SZ)
	int i,j;
	struct event_info *EI;
	for (i = 0; i < 4; i++) {
		stat = bh160_fetch_parameter(S, P_PAGE_SYSTEM, P1_SENS_STAT_0 + i, 0);
		ASSERT(stat);
		for (j = 0; j < REG_PRM_READ_SZ; j++) {
			if (stat[j]) {
				uint8_t id = i*REG_PRM_READ_SZ + j + 1;
				int wkp = 0;
				if (id & 32) { wkp = 1; id &= 31; }
				EI = event_info(S, id);
				DBG1("%02d (%s:%-10s) %s\n", id, wkp?"WKP":"NWK",
					EI->sensor, bhi160_sensor_status_string(stat[j]));
			}
		}
	}
}

// Underlying sensor debug info
void bhi160_dump_phys_sensor_status(struct bhi160_state *S)
{
	struct p1_phys_sens_status *PSS;

	PSS = bh160_fetch_parameter(S, P_PAGE_SYSTEM, P1_PHYS_SENS_STAT, 0);
	ASSERT(PSS);

	DBG1("Physical Sensor debug:\n");

	DBG1("ACC: sample[Hz]=%5u, dyn[g]=  %5u, int=%d, pwr=%s\n",
		PSS->acc_sample_rate, PSS->acc_dynamic_range, PSS->acc_flags & SENS_STAT_DATA,
		bhi_160_power_mode_string(PSS->acc_flags));

	DBG1("GYR: sample[Hz]=%5u, dyn[r/s]=%5u, int=%d, pwr=%s\n",
		PSS->gyr_sample_rate, PSS->gyr_dynamic_range, PSS->gyr_flags & SENS_STAT_DATA,
		bhi_160_power_mode_string(PSS->gyr_flags));

	DBG1("MAG: sample[Hz]=%5u, dyn[uT]= %5u, int=%d, pwr=%s\n",
		PSS->mag_sample_rate, PSS->mag_dynamic_range, PSS->mag_flags & SENS_STAT_DATA,
		bhi_160_power_mode_string(PSS->mag_flags));
}

// Reads physically present sensors and dumps their information
void bhi160_dump_phys_sensors_present(struct bhi160_state *S)
{
	uint64_t *bitmapp; // 64 bits, one for each physical sensor present
	uint64_t bitmap;   // Runtime copy of bitmap
	struct p1_phys_sens_info *PSI;
	struct event_info *EI;
	int i;

	bitmapp = bh160_fetch_parameter(S, P_PAGE_SYSTEM, P1_PHYS_SENS_PRESENT, 0);
	ASSERT(bitmapp);
	bitmap = *bitmapp; // Need to make a copy, to avoid overwriting the bitmap below.

	for (i = 0; i < 64; i++) {
		if (!(bitmap & (1ULL << i))) continue;
		// Sensor is present, dumping info

		PSI = bh160_fetch_parameter(S, P_PAGE_SYSTEM, P1_PHYS_SENS_INFO(i), 0);
		ASSERT(PSI);

		EI = event_info(S, i);
		ASSERT(EI);

		DBG1("Sensor %02d(%s), drv(%02x,%02x), I[mA]=%.1f, range[SI]=%5u, irq=%d, pwr=%s, sample[Hz]=%u,\n",
			PSI->id, EI->sensor, PSI->driver_id, PSI->driver_ver, 0.1 * PSI->current,
			PSI->range, PSI->flags & SENS_STAT_DATA, bhi_160_power_mode_string(PSI->flags),
			PSI->samplerate);
		DBG1("       axes=%d, matrix=[[%2d,%2d,%2d],[%2d,%2d,%2d],[%2d,%2d,%2d]]\n", PSI->axes,
			MATRIX_C(PSI->m,0), MATRIX_C(PSI->m,1), MATRIX_C(PSI->m,2),
			MATRIX_C(PSI->m,3), MATRIX_C(PSI->m,4), MATRIX_C(PSI->m,5),
			MATRIX_C(PSI->m,6), MATRIX_C(PSI->m,7), MATRIX_C(PSI->m,8));

	}

}

// Sets orientation matrix for specified physical sensor. Mapping is
// [line][column] of the rotation matrix. Must only contain (-1,0,1).
int bhi160_set_mapping(struct bhi160_state *S, uint8_t id, const int8_t mapping[3][3])
{
	struct p1_phys_sens_info *PSI;
	uint8_t packed_matrix[8] = {0};
	int rval;

	PSI = bh160_fetch_parameter(S, P_PAGE_SYSTEM, P1_PHYS_SENS_INFO(id), 0);
	ASSERT(PSI);
	DBG1("S %d, matrix=[[%2d,%2d,%2d],[%2d,%2d,%2d],[%2d,%2d,%2d]] => ", id,
		MATRIX_C(PSI->m,0), MATRIX_C(PSI->m,1), MATRIX_C(PSI->m,2),
		MATRIX_C(PSI->m,3), MATRIX_C(PSI->m,4), MATRIX_C(PSI->m,5),
		MATRIX_C(PSI->m,6), MATRIX_C(PSI->m,7), MATRIX_C(PSI->m,8));
	
	// Pack signed 4-bit integers into rotation matrix
	#define CHECKM(v) ASSERT(v == 0 || v == 1 || v == -1)
	#define PACKM(v1,v2) (((uint8_t)(v1) & 0xF) | ((uint8_t)(v2) << 4)); CHECKM(v1); CHECKM(v2)

	packed_matrix[0] = PACKM(mapping[0][0],mapping[0][1]);
	packed_matrix[1] = PACKM(mapping[0][2],mapping[1][0]);
	packed_matrix[2] = PACKM(mapping[1][1],mapping[1][2]);
	packed_matrix[3] = PACKM(mapping[2][0],mapping[2][1]);
	packed_matrix[4] = PACKM(mapping[2][2],0);

	rval = bh160_set_parameter(S, P_PAGE_SYSTEM, P1_PHYS_SENS_INFO(id), packed_matrix, sizeof(packed_matrix));
	ASSERT(rval==0);

	PSI = bh160_fetch_parameter(S, P_PAGE_SYSTEM, P1_PHYS_SENS_INFO(id), 0);
	ASSERT(PSI);
	DBG1("[[%2d,%2d,%2d],[%2d,%2d,%2d],[%2d,%2d,%2d]]\n",
		MATRIX_C(PSI->m,0), MATRIX_C(PSI->m,1), MATRIX_C(PSI->m,2),
		MATRIX_C(PSI->m,3), MATRIX_C(PSI->m,4), MATRIX_C(PSI->m,5),
		MATRIX_C(PSI->m,6), MATRIX_C(PSI->m,7), MATRIX_C(PSI->m,8));

	return 0;
}

/* Dumps parameter page3 sensor information for given sensor, wkup or nwk */
void bhi160_dump_sensor_info(struct bhi160_state *S, uint8_t id)
{
	struct event_info *EI;
	struct p3_sensor_info *SI;

	ASSERT(id <= P3_ACTIVITY + P3_TYPE_WKP);

	EI = event_info(S, id);
	ASSERT(EI);

	// Using id (1-63) for parameter address returns sensor info pages
	SI = bh160_fetch_parameter(S, P_PAGE_SENSORS, id, 0);
	ASSERT(SI);

	ASSERT(SI->sensor_type == id);

	DBG1("P3 Sensor %d(%s), ", id, EI->sensor);
	DBG1("drv(%02x,%02x), ", SI->driver_id, SI->driver_ver);
	DBG1("pwr[mA]=%.1f, maxr[%s]=%u, ", 0.1 * SI->power, EI->unit, SI->max_range);
	DBG1("res[bits]=%u\n          msam[Hz]=%u ", SI->resolution, SI->max_rate);
	DBG1("ff_res=%u, ff_max=%u, evsize=%u, min_sam[Hz]=%u", SI->fifo_res, SI->fifo_max, SI->event_size, SI->min_rate);
	DBG1("\n");
}

// Dumps parameter page3 sensor configuration
void bhi160_dump_sensor_config(struct bhi160_state *S, uint8_t id)
{
	struct event_info *EI;
	struct p3_sensor_config *SC;

	ASSERT(id <= P3_ACTIVITY + P3_TYPE_WKP);

	EI = event_info(S, id);
	ASSERT(EI);

	// Set bit to access config parameter pages
	SC = bh160_fetch_parameter(S, P_PAGE_SENSORS, id | P3_CONFIG_FLAG, 0);
	ASSERT(SC);

	// TODO: This needs to move to something other than a dump function
	// Update sensor scaling factor
	EI->ascale = EI->scale * (float)SC->dyn_range;

	DBG1("Sensor %d(%s), srate[Hz]=%u, latency[ms]=%u, sens[?]=%u, dynrange[%s]=%u\n",
		id, EI->sensor, SC->sample_rate, SC->max_latency, SC->sensitivity, EI->unit, SC->dyn_range);
}

// Sets requested sensor parameters, returns actual values
int bhi160_set_sensor_config(
		struct bhi160_state *S,
		uint8_t id, // Sensor ID, including wakeup/non-wakeup
		uint16_t *samplerate,  // Sample rate [Hz], 0 to disable
		uint16_t *latency,     // Max allowed latency [ms]. Set to 0 for non-batch mode
		uint16_t *sensitivity, // ? Sensitivity, scaled to sensor scaling value
		uint16_t *dynrange)    // Dynamic range in SI units [uT], [g], [deg/s]
{
	struct p3_sensor_config SCW;  // Write data
	struct p3_sensor_config *SCR; // Readback
	int r;

	ASSERT(id <= P3_ACTIVITY + P3_TYPE_WKP);

	SCW.sample_rate = *samplerate;
	SCW.max_latency = *latency;
	SCW.sensitivity = *sensitivity;
	SCW.dyn_range   = *dynrange;

	r = bh160_set_parameter(S, P_PAGE_SENSORS, id | P3_CONFIG_FLAG, &SCW, sizeof(SCW));
	ASSERT(r == 0);

	// Read back actual values
	// Set bit to access config parameter pages
	SCR = bh160_fetch_parameter(S, P_PAGE_SENSORS, id | P3_CONFIG_FLAG, 0);
	ASSERT(SCR);

	*samplerate = SCR->sample_rate;
	*latency = SCR->max_latency;
	*sensitivity = SCR->sensitivity;
	*dynrange = SCR->dyn_range;

	return 0;
}

/* Checks the event FIFO and reads one event if available.
   Returns negative error, 0 if no-op, 1 for event and 2 for event & more pending. */
int bhi160_poll_fifo(struct bhi160_state *S, struct event_info **ev_info)
{
	static uint8_t event_buffer[48*1024] = {0};  /* Can hold entire sensor FIFO */
	static uint8_t *bptr = NULL;                 /* start of next event in buffer */
	static int buffer_bytes = 0;                 /* remaining bytes to end of buffer */
	struct event_info *EI;
	struct event *EV;
	uint8_t id;
	int r;

	if (!buffer_bytes) {
		int bytes_rem;

		bytes_rem = sensor_read_le16(S, REG_BYTES_REM);
		if (!bytes_rem) return 0; // Nothing to do

		DBG4("bytes_rem: %d\n", bytes_rem);
		buffer_bytes = bytes_rem;

		/* reading entire FIFO in REG_BUF_OUT_SZ bytes chunks */
		bptr = event_buffer;
		while (bytes_rem > 0) {
			int to_read;
			to_read = bytes_rem > REG_BUF_OUT_SZ ? REG_BUF_OUT_SZ : bytes_rem;
			r = sensor_read(S, REG_BUF_OUT, bptr, to_read);
			if (r) return -1;
			bptr += to_read;
			bytes_rem -= to_read;
		}
		bptr = event_buffer;
	}

	/* There is at least one event in the buffer now */
	id = bptr[0];

	EI = event_info(S, id);

	if (EI == NULL) {
		DBG1("Unknown event id %d, flushing\n", id);
		HEX1(bptr, buffer_bytes);
		bptr = NULL;
		buffer_bytes = 0;
		*ev_info = (struct event_info *)NULL;
		return 0;
	}

	if (EI->size > buffer_bytes) {
		DBG1("Partial event, flushing\n");
		HEX1(bptr, buffer_bytes);
		bptr = NULL;
		buffer_bytes = 0;
		*ev_info = (struct event_info *)NULL;
		return 0;
	}

	/* Overlay event struture over current message in buffer. */
	EV = (struct event *)bptr;

	EI->ev = EV;

	/* Decoding sensor wakup flag */
	if (id < 64) EI->wkup = !!(id & 32);

	/* return info structure */
	*ev_info = EI;

	/* Adjust buffer for next event */
	buffer_bytes -= EI->size;
	bptr += EI->size;

	return buffer_bytes ? 2 : 1;
}


#ifdef BHI_TEST

// This is expected to define BHI160_FW_DATA & BHI160_FW_LEN
#include <bhi-160_fw.h>
#include <unistd.h>

struct bhi160_state GS;

int main(void)
{
	int rval;
	{
		uint8_t b[4] = {0x11, 0x22, 0x33, 0x44};
		unsigned *x = (unsigned *)b;
		printf("Native word length: %d bytes\n", sizeof(int));
		printf("Endian: %08x\n", *x);
	}

	rval = bhi160_open(&GS);
	ASSERT(rval == 0);

	//bhi160_dump_all(&GS);

	printf("Uploading firmware\n");
	bhi160_upload_firmware(&GS, BHI160_FW_DATA, BHI160_FW_LEN);

	printf("bhi160_dump_sensor_status\n");
	bhi160_dump_sensor_status(&GS);
	printf("bhi160_dump_phys_sensor_status\n");
	bhi160_dump_phys_sensor_status(&GS);
	printf("bhi160_dump_phys_sensors_present\n");
	bhi160_dump_phys_sensors_present(&GS);

	printf("bhi160_dump_sensor_info\n");
	bhi160_dump_sensor_info(&GS, P3_ACC);
	bhi160_dump_sensor_info(&GS, P3_MAG);
	bhi160_dump_sensor_info(&GS, P3_ORIENT);

	printf("bhi160_dump_sensor_config\n");
	bhi160_dump_sensor_config(&GS, P3_ACC);
	bhi160_dump_sensor_config(&GS, P3_MAG);
	bhi160_dump_sensor_config(&GS, P3_ORIENT);

	{
		uint16_t samplerate, latency, sensitivity, dynrange;
		samplerate = 10;
		latency = 0;
		sensitivity = 0;
		dynrange = 2;
		printf("bhi160_set_sensor_config(ACC)\n");
		bhi160_set_sensor_config(&GS, P3_ACC, &samplerate, &latency, &sensitivity, &dynrange);
		DBG1("Sensor readback parameters: sam=%u, lat=%u, sens=%u, dyn=%u\n",
			samplerate, latency, sensitivity, dynrange);
		samplerate = 10;
		latency = 0;
		sensitivity = 0;
		dynrange = 2000;
		printf("bhi160_set_sensor_config(MAG)\n");
		bhi160_set_sensor_config(&GS, P3_MAG, &samplerate, &latency, &sensitivity, &dynrange);
		DBG1("Sensor readback parameters: sam=%u, lat=%u, sens=%u, dyn=%u\n",
			samplerate, latency, sensitivity, dynrange);
		samplerate = 10;
		latency = 0;
		sensitivity = 0;
		dynrange = 1;
		printf("bhi160_set_sensor_config(GYR)\n");
		bhi160_set_sensor_config(&GS, P3_GYR, &samplerate, &latency, &sensitivity, &dynrange);
		DBG1("Sensor readback parameters: sam=%u, lat=%u, sens=%u, dyn=%u\n",
			samplerate, latency, sensitivity, dynrange);
		samplerate = 10;
		latency = 0;
		sensitivity = 0;
		dynrange = 1;
		printf("bhi160_set_sensor_config(ORIENT)\n");
		bhi160_set_sensor_config(&GS, P3_ORIENT, &samplerate, &latency, &sensitivity, &dynrange);
		DBG1("Sensor readback parameters: sam=%u, lat=%u, sens=%u, dyn=%u\n",
			samplerate, latency, sensitivity, dynrange);
	}

	printf("Starting poll loop\n");

	while (1) {
		int r;
		struct event_info *EI = NULL;

		r = bhi160_poll_fifo(&GS, &EI);
		if (r) {
			if (EI->cb) EI->cb(&GS, EI);
			else DBG1("Evpoll: r=%d, ev=%d, evtsize=%d\n", r, EI->id, EI->size);
			if (EI->id == 3) {
				// ANSI sequence to move cursor up 4 lines and back to start
				printf("\r\x1b[4A"); fflush(stdout);
			}
			continue;
		}
		usleep(10*1000);
	}
}

#endif
