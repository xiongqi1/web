#include <iostream>
#include <chrono>
#include <thread>
#include <Eigen/Geometry>
#include <time.h>
#include <signal.h>

#include "bhi-160.h"
#include "bhi-160_fw.h"
#include "i2c_linux.h"
#include "log.h"

#ifdef HAS_RDB_SUPPORT
#include <sys/select.h>
#include "rdblib.h"

#include "process.h"
#include "gridvar.h"
#endif

//#############################################################################
// Constants
//#############################################################################
#define RDB_UPDATE_RATE_HZ              2       /* Hz */
#define SENSOR_SAMPLE_RATE_HZ           25      /* Hz */
#define SENSOR_SAMPLES_PER_RDB_UPDATE	(SENSOR_SAMPLE_RATE_HZ/RDB_UPDATE_RATE_HZ)
#define RE_CALIBRATION_TIME_SEC         10*60   /* used for counting before going to re-calibration */
#define CALIBRATION_STEADY_TIME_SEC     3       /* used for counting before going to normal mode */
#define CALIBRATION_ACC_TOLERANCE       0.1     /* unit g, used to be tolerance of position detection*/
#define ORI_IS_RELIABLE                 3       /* the most reliable estimation from sensor report */
#define POSITION_X_P 0x00000001
#define POSITION_X_N 0x00000002
#define POSITION_Y_P 0x00000004
#define POSITION_Y_N 0x00000008
#define POSITION_Z_P 0x00000010
#define POSITION_Z_N 0x00000020

#ifdef CLI_TEST
#define FIFO_POLL_DELAY_MS                     10      /* ms */
#else
#define FIFO_POLL_DELAY_MS                     100     /* ms */
#endif

#define CALIB_ACC_TOLERANCE_ACCEPTABLE(acc) (fabs(1.0 - (acc)) <= CALIBRATION_ACC_TOLERANCE)

typedef enum {
	SENSOR_CALIBRATING,     // Sensor start to calibrate
	SENSOR_ENTERING_NORMAL, // Sensor detected status in 3 and entering into normal
	SENSOR_NORMAL,          // Sensor is working on normal mode after calibration
	SENSOR_PRE_CALIB        // Sensor reported measurement not reliable
} sensor_mode_t;


static time_t sensor_state_timer_start;
static unsigned int calibration_positions = 0x0; // bit 0-5 maps to x+,x-, y+,y-,z+,z-


#ifdef HAS_RDB_SUPPORT

#define RPC_SERVICE_NAME  "azimuth_down_tilt"
#define RPC_CMD    "get"

static struct rdb_session * rdb_s = nullptr;
static rdb_rpc_server_session_t * rpc_s = nullptr;
static int rdbfd = 0;

#endif // HAS_RDB_SUPPORT

volatile static sig_atomic_t terminate = 0;

/* signal handler for SIGINT, SIGQUIT and SIGTERM */
static void sig_handler(int sig)
{
    (void)sig;
    terminate = 1;
}

struct bhi160_state S;

static const float  PI=3.14159265358979f;

typedef Eigen::Vector3f vec3f;          // 3D vector type
typedef Eigen::Vector3f ori3f;          // 3D orientation type (Azimuth(magnetic), pitch(grav), roll(grav)

typedef Eigen::Quaternionf quaternion;

/* The Eigen Quaternion type is missing the addition and accumulation operators.
 * Adding them here. */
class Quaternion: public Eigen::Quaternionf {
	public:
		Quaternion & operator+=(const Quaternion &rhs)
		{
			this->coeffs() += rhs.coeffs();
			return *this;
		}
		friend Quaternion operator+(Quaternion lhs, const Quaternion &rhs)
		{
			lhs.coeffs() += rhs.coeffs();
			return lhs;
		}
};

struct sensor_readings {
	uint32_t ts;     // Timestamp of this sensor reading
	vec3f    acc;    // Current accelerometer data [g]
	int      accf;   // Current sensor reliability (0-3)
	vec3f    mag;    // Current magnetometer data [uT]
	int      magf;   // Current sensor reliability (0-3)
	vec3f    gyr;    // Current gyroscope data [deg/s]
	int      gyrf;   // Current sensor reliability (0-3)
	ori3f    ori;    // Current orientation data [deg] (Azimuth, Pitch, Roll)
	int      orif;   // Current sensor reliability (0-3)
};
/* Sensor readings. Values change during callbacks. Full set is only
 * valid during the fusecallback (the last one belonging to a time sample) */
static struct sensor_readings R;

/* Rotate by 90/180/270 deg to match carthesian coordinates to reality */
static void coord_adjust(vec3f &xyz)
{
	(void)xyz;
}

// Rotate orientation vector (Azimuth (0 - 360) / Pitch (-180 - 180) / Roll (-90 - 90))
static void orient_Adjust(ori3f &apr)
{
	// Note that when device is rolled through 90deg pitch, the azimuth
	// mirrors, but not exactly at the 90 point. It may be necessary to
	// adjust via the sensor orientation matrix instead of here.
	(void)apr;
}

// filter orientation status before it report to uplevel
void do_sensor_tasks();

/* 9 degree of freedom sensor fusion. Called once for every sample block, when all samples are collected.
 * deltat is elapsed time since previous sample. If it's 0, disregard. */
void fuse_9dof_sensors(uint32_t deltat)
{
	/*
	Coordinates: X,Y in PCB plane, Z down.

	The accelerometer sensor produces a vector pointing down. So the flat board
	is positive Z.

	Gyro produces rotation rates around x,y,z

	Magnetometer produces Magnetic magnitude x,y,z

	*/

#ifdef CLI_TEST
	// Raw data
	printf("%u a%u(%5.2f,%5.2f,%5.2f) m%u(%6.2f,%6.2f,%6.2f) g%u(%6.2f,%6.2f,%6.2f)\n",
		deltat, R.accf, R.acc[0], R.acc[1], R.acc[2],
		R.magf, R.mag[0], R.mag[1], R.mag[2], R.gyrf, R.gyr[0], R.gyr[1], R.gyr[2]);

	// Bosch sensor fusion
	printf("%7.3f orient[deg] A=%10.5f, P=%10.5f, R=%10.5f, s=%d(%s)\n",
		(double)S.Time * TIME_SCALE,
		R.ori[0], R.ori[1], R.ori[2], R.orif, STATUS_STR(R.orif));

	// ANSI sequence to move cursor up two lines and back to start
	printf("\r\x1b[2A"); fflush(stdout);
#endif
}

void decode_vector(struct bhi160_state *S, struct event_info *EI)
{
	struct event *E = EI->ev;

	switch (EI->id) {
		case P3_ACC:
			R.acc[0] = (float)E->vector.x * EI->ascale;
			R.acc[1] = (float)E->vector.y * EI->ascale;
			R.acc[2] = (float)E->vector.z * EI->ascale;
			R.accf = E->vector.status;
			coord_adjust(R.acc);
			break;
		case P3_MAG:
			R.mag[0] = (float)E->vector.x * EI->ascale;
			R.mag[1] = (float)E->vector.y * EI->ascale;
			R.mag[2] = (float)E->vector.z * EI->ascale;
			R.magf = E->vector.status;
			coord_adjust(R.mag);
			break;
		case P3_UMAG:
			R.mag[0] = (float)E->uncal_vector.x * EI->ascale;
			R.mag[1] = (float)E->uncal_vector.y * EI->ascale;
			R.mag[2] = (float)E->uncal_vector.z * EI->ascale;
			R.magf = E->uncal_vector.status;
// 			coord_adjust(R.mag);
			break;
		case P3_GYR:
			R.gyr[0] = (float)E->vector.x * EI->ascale;
			R.gyr[1] = (float)E->vector.y * EI->ascale;
			R.gyr[2] = (float)E->vector.z * EI->ascale;
			R.gyrf = E->vector.status;
			coord_adjust(R.gyr);
			break;
		case P3_ORIENT:
			R.ori[0] = (float)E->vector.x * EI->ascale;
			R.ori[1] = (float)E->vector.y * EI->ascale;
			R.ori[2] = (float)E->vector.z * EI->ascale;
			R.orif = E->vector.status;
			orient_Adjust(R.ori);
			break;
	}

	// This is assuming that all sensor sample rates are identical and we
	// can identify the end of the block by the last sensor's ID
	#define LAST_SENSOR P3_ORIENT

	if (EI->id == LAST_SENSOR) {
		int32_t Td;
		// Calculate time delta, allowing for wrap
		Td = (int32_t)S->Time - (int32_t)R.ts;
		R.ts = S->Time;
		if (Td < 0 || Td > 32000) Td = 0; // Bad timestep (at least 1Hz)
		fuse_9dof_sensors((uint32_t)Td);
		do_sensor_tasks();
	}
}

/*
 * It will check if 6 directions are touched during calibration process
 * @return 1 if all points are touched, otherwise 0
 */
int check_positions_touched()
{
    if (R.acc[0]>0.0 && CALIB_ACC_TOLERANCE_ACCEPTABLE(R.acc[0]) &&
        !(calibration_positions & POSITION_X_P)) {
        calibration_positions |= POSITION_X_P;
        LS_INFO("X front side detected\n");
    }

    if (R.acc[0]<0.0 && CALIB_ACC_TOLERANCE_ACCEPTABLE(-R.acc[0]) &&
        !(calibration_positions & POSITION_X_N)) {
        calibration_positions |= POSITION_X_N;
        LS_INFO("X back side detected\n");
    }

    if (R.acc[1]>0.0 && CALIB_ACC_TOLERANCE_ACCEPTABLE(R.acc[1]) &&
        !(calibration_positions & POSITION_Y_P)) {
        calibration_positions |= POSITION_Y_P;
        LS_INFO("Y front side detected\n");
    }

    if (R.acc[1]<0.0 && CALIB_ACC_TOLERANCE_ACCEPTABLE(-R.acc[1]) &&
        !(calibration_positions & POSITION_Y_N)) {
        calibration_positions |= POSITION_Y_N;
        LS_INFO("Y back side detected\n");
    }

    if (R.acc[2]>0.0 && CALIB_ACC_TOLERANCE_ACCEPTABLE(R.acc[2]) &&
        !(calibration_positions & POSITION_Z_P)) {
        calibration_positions |= POSITION_Z_P;
        LS_INFO("Z front side detected\n");
    }

    if (R.acc[2]<0.0 && CALIB_ACC_TOLERANCE_ACCEPTABLE(-R.acc[2]) &&
        !(calibration_positions & POSITION_Z_N)) {
        calibration_positions |= POSITION_Z_N;
        LS_INFO("Z back side detected\n");
    }

    if (calibration_positions & 0x3f == 0x3f){
        LS_INFO("All positions detected\n");
        return 1;
    }
    else {
        return 0;
    }
}


/*
 * Sensor report main task , will check orientation status and current working mode
 * to do related operation, the working mode transition states are:
 *   1. by default it is in calibrating mode, once consecutive reliable status in 3 is recorded,move to normal mode,
 *   2. in normal mode,  sensor will report and update RDB periodical.
 *   3. once sensor status decline is detected, system moves to sensor lost lock enter mode
 *   4. if the status is not recovered till a period of time, it moves to lock count down mode, otherwise it will go back to normal mode.
 *   5. once entering to lost lock count down mode,if re-calibrate timer is expired, system moves to calibrating mode, but if found
 *      any device status recovered back to 3, it will reset the timer and goes back to normal mode and continue measurement task.
 *   note: in normal and lost_lock states, sensor need to report measurement result.
 *
 */
void do_sensor_tasks(){
    static sensor_mode_t sensor_mode=SENSOR_CALIBRATING;
    static unsigned int filtered_status;
    time_t now;
    switch(sensor_mode) {

        case SENSOR_CALIBRATING:
            filtered_status = 0;
            if (check_positions_touched() && R.orif == ORI_IS_RELIABLE ){
                time(&sensor_state_timer_start);
                sensor_mode = SENSOR_ENTERING_NORMAL;
                LS_NOTICE("Going entering normal\n");
            }
            break;

        case SENSOR_ENTERING_NORMAL:
            if (R.orif != ORI_IS_RELIABLE) {
                sensor_mode = SENSOR_CALIBRATING;
                LS_NOTICE("Going back clibrating\n");
                break;
            }
            time(&now);
            if (difftime(now,sensor_state_timer_start) >= CALIBRATION_STEADY_TIME_SEC) {
                sensor_mode = SENSOR_NORMAL;
                LS_NOTICE("Going normal work\n");
            }
            break;

        case SENSOR_NORMAL:
            filtered_status = 3;
            if (R.orif != ORI_IS_RELIABLE) {
                time(&sensor_state_timer_start);
                sensor_mode = SENSOR_PRE_CALIB;
                LS_NOTICE("Sensor loosing reliability\n");
                break;
            }
            break;

        case SENSOR_PRE_CALIB:
            if (R.orif == ORI_IS_RELIABLE ) {
                sensor_mode = SENSOR_NORMAL;
                LS_NOTICE("Sensor recovered\n");
                break;
            }
            time(&now);
            if (difftime(now,sensor_state_timer_start) >= RE_CALIBRATION_TIME_SEC) {
                sensor_mode = SENSOR_CALIBRATING;
                LS_NOTICE("Sensor lost reliability\n");

            }
            break;

        default:
            sensor_mode = SENSOR_CALIBRATING;

    }
#ifdef HAS_RDB_SUPPORT
    static unsigned int samples_recieved = 0;
    if(0 == (samples_recieved++ % SENSOR_SAMPLES_PER_RDB_UPDATE))
    {
        (void)process_readings(rdb_s, R.acc, R.accf, R.ori, filtered_status);
        rdb_set_ori_inst_status(rdb_s, R.orif);
    }
#endif
}

#ifdef HAS_RDB_SUPPORT
/*
 * RDB RPC command handler
 *
 * @param cmd          The name of the command to be handled
 * @param params       An array of RDB RPC parameters
 * @param params_len   The number of parameters
 * @param result       A buffer for a command result
 * @param result_len   On input this gives the result buffer size. On output it
 * gives the length actually written into result (including terminating null if any)
 * @return 0 on success; a negative error code on failure
 */
static int cmd_handler(char *cmd, rdb_rpc_cmd_param_t params[], int params_len, char *result, int *result_len)
{
    int ret = 0;
    LS_DEBUG("Handling command %s\n", cmd);

    if(strcmp(cmd, RPC_CMD)) {
        *result_len = 0;
        return -1;
    }

    int azimuth = 0;
    int tilt = 0;
    get_precise_data(azimuth, tilt);
    ret = snprintf(result, *result_len, "%d;%d", azimuth, tilt);

    if(ret < 0) {
        *result_len = 0;
        return -2;
    }
    else if(ret >= *result_len) {
        *result_len = 0;
        return -3;
    }

    *result_len = ret + 1;
    return 0;
}

/**
 * Initialize RDB session and register RDB RPC service
 *
 * @return 0 on success; none zero on failure
 */
int init_rdb_service()
{
    int ret = 0;

    if ((ret = rdb_open(NULL, &rdb_s)) < 0 || !rdb_s) {
        LS_ERROR("Failed to open RDB\n");
        return ret;
    }

    rdbfd = rdb_fd(rdb_s);
    if (rdbfd < 0) {
        LS_ERROR("Failed to get RDB FD\n");

        rdb_close(&rdb_s);
        return rdbfd;
    }

    ret = rdb_rpc_server_init(RPC_SERVICE_NAME, &rpc_s);
    if(ret) {
        LS_ERROR("Failed to init RDB RPC (%d)\n", ret);

        rdb_close(&rdb_s);
        return ret;
    }

    ret = rdb_rpc_server_add_command(rpc_s, RPC_CMD, nullptr, 0, cmd_handler);
    if (ret) {
        LS_ERROR("Failed to add RDB command %s (%d)\n", RPC_CMD, ret);

        rdb_rpc_server_destroy(&rpc_s);
        rdb_close(&rdb_s);
        return ret;
    }

    ret = rdb_rpc_server_run(rpc_s, rdb_s);
    if (ret) {
        LS_ERROR("Failed to run rpc server (%d)\n", ret);

        rdb_rpc_server_destroy(&rpc_s);
        rdb_close(&rdb_s);
        return ret;
    }

    LS_DEBUG("RDB RPC registerd: %s - %s", RPC_SERVICE_NAME, RPC_CMD);
    return 0;
}

/**
 * Terminate RDB session and register RDB RPC service
 *
 */
void terminate_rdb_services()
{
    int ret = 0;

    ret = rdb_rpc_server_stop(rpc_s);
    if (ret) {
        LS_ERROR("Failed to stop rpc server\n");
    }

    ret = rdb_rpc_server_destroy(&rpc_s);
    if (ret) {
        LS_ERROR("Failed to destroy rpc server\n");
    }

    rdb_close(&rdb_s);
}

/*
 * Monitor RDB FD read activity with a timeout
 *
 * @return 0 on success; none zero on failure
 */
int monitor_rpc()
{
    int ret = 0;
    fd_set fdset;
    struct timeval tv;

    int name_len;
    char name_buf[MAX_NAME_LENGTH];

    tv.tv_sec = 0;
    tv.tv_usec = FIFO_POLL_DELAY_MS * 1000;

    FD_ZERO(&fdset);
    FD_SET(rdbfd, &fdset);

    ret = select(rdbfd + 1, &fdset, NULL, NULL, &tv);

    if (ret < 0) { // error
        int tmp = errno;
        LS_ERROR("Select returned %d, errno: %d\n", ret, tmp);
        return ret;
    }

    if (ret > 0) { // available
        name_len = sizeof(name_buf) - 1;

        ret = rdb_getnames(rdb_s, "", name_buf, &name_len, TRIGGERED);
        if (ret) {
            LS_ERROR("Failed to get triggered RDB names (%d)\n", ret);
            return ret;
        }

        LS_DEBUG("RPC call in: %s", name_buf);

        ret = rdb_rpc_server_process_commands(rpc_s, name_buf);
        if (ret) {
            LS_ERROR("Failed to process commands (%d)\n", ret);
            return ret;
        }
    }

    return 0;
}

#endif //HAS_RDB_SUPPORT

int main(void)
{
	uint16_t samplerate, latency, sensitivity, dynrange;

	openlog(PROG_NAME, LOG_PID | LOG_CONS, LOG_USER);
	LS_NOTICE("Starting\n");

    if (signal(SIGINT, sig_handler) == SIG_ERR ||
        signal(SIGQUIT, sig_handler) == SIG_ERR ||
        signal(SIGTERM, sig_handler) == SIG_ERR) {
        int ret = -errno;
        LS_ERROR("Failed to register signal handler\n");

        closelog();
        return ret;
    }

#ifdef HAS_RDB_SUPPORT
    LS_NOTICE("WMM version date: %s", WMMVersion().c_str());

    if(init_rdb_service()) {
        LS_ERROR("RDB initialization has failed, exiting.");
        return 1;
    }
#endif

	CHECK(bhi160_open(&S),0,"Can't open\n");
	CHECK(bhi160_upload_firmware(&S, BHI160_FW_DATA, BHI160_FW_LEN), 0, "Failed to upload firmware\n");

	/* Read mapping matrices for all three base sensors (1=acc/14=mag unc/16=gyr unc) */
	bhi160_dump_phys_sensors_present(&S);
	LS_NOTICE("Started\n");

	/* Devboard axis info with the default orientation mapping. Devboard orientation
	 * is defined as front = 3-pin connector.
	 *
	 * Accel - Gravity provides an 'up' acceleration:
	 * 	Flat, upright	(0,0,1)
	 * 	Flat, upside dn	(0,0,-1)
	 * 	Pointing down	(0,-1,0)
	 * 	Pointing up	(0,1,0)
	 * 	Right side down	(-1,0,0)
	 * 	Left side down	(1,0,0)
	 * 
	 * Mag - note Sydney magnetic inclination is 64deg, so Z-component dominates:
	 * 	Flat, -> N	(0,24,47)
	 * 	Flat, -> S	(0,-24,47)
	 * 	Flat, -> E	(-24,0,47)
	 * 	Flat, -> W	(24,0,47)
	 * 	N -> nose up	(0,47,-24)
	 * 	N -> nose down	(0,-47,24)
	 *
	 * Gyro - right hand rule for axis rotation:
	 * 	Roll right	(0,1,0)
	 * 	Roll left	(0,-1,0)
	 * 	Pitch down	(-1,0,0)
	 * 	Pitch up	(1,0,0)
	 * 	Yaw left	(0,0,1)
	 * 	Yaw right	(0,0,-1)
	 *
	 * Default mapping matrices (both inertial sensors are same, one package)
	 * 
	 * INR  0, 1, 0
	 *     -1, 0, 0
	 *      0, 0, 1
	 * 
	 * MAG  0, 1, 0
	 *      1, 0, 0
	 *      0, 0,-1
	 * 
	 * Mapping matrix for Lark is calculated to replicate the same orientations.
	 * inertial and magnetic sensors are in same relative orientations, but
	 * the board is horizontal on edge.
	 *
	 * Accel - Gravity provides an 'up' acceleration:
	 * 	Flat, upright	(0,1,0)
	 * 	Flat, upside dn	(0,-1,0)
	 * 	Pointing down	(0,0,1)
	 * 	Pointing up	(0,0,-1)
	 * 	Right side down	(-1,0,0)
	 * 	Left side down	(1,0,0)
	 * 
	 * Mag - note Sydney magnetic inclination is 64deg, so Z-component dominates:
	 * 	Flat, -> N	(0,48,-20)
	 * 	Flat, -> S	(0,48,20)
	 * 	Flat, -> E	(-20,48,0)
	 * 	Flat, -> W	(20,48,0)
	 * 	N -> nose up	(0,-20,-48)
	 * 	N -> nose down	(0,20,48)
	 * 
	 * Gyro - right hand rule for axis rotation:
	 * 	Roll right	(0,0,-1)
	 * 	Roll left	(0,0,1)
	 * 	Pitch down	(-1,0,0)
	 * 	Pitch up	(1,0,0)
	 * 	Yaw left	(0,1,0)
	 * 	Yaw right	(0,-1,0)
	 *
	 * What is the axis rotation between devboard and lark? Determine
	 * coordinate transform (x on lark is what coordinate on devboard).
	 * Multiply [x,y,z] with new coordinates [x,z,-y]^T. In the
	 * resulting 3x3 matrix, replace the square terms with 1 and
	 * non-square ones with 0.
	 * 
	 * We can then determine the new rotation matrix by multiplying the
	 * default one by the relative devboard->lark one.
	 *
	 * Matrices are orthogonal, so inverse is the same as transpose.
	 *
	 * Inertial: (x,y,z) -> (x,z,-y)
	 * 	R=[[1,0,0],[0,0,-1],[0,1,0]]T=[[1,0,0],[0,0,1],[0,-1,0]]
	 * 	Ni=Di*R=[[0,1,0],[-1,0,0],[0,0,1]]*[[1,0,0],[0,0,1],[0,-1,0]]
	 * 	Ni=[[0,0,1],[-1,0,0],[0,-1,0]]
	 *
	 * Magnetic: (x,y,z) -> (x,z,-y)
	 * 	R=[[1,0,0],[0,0,-1],[0,1,0]]T=[[1,0,0],[0,0,1],[0,-1,0]]
	 * 	Nm=Dm*R=[[0,1,0],[1,0,0],[0,0,-1]]*[[1,0,0],[0,0,1],[0,-1,0]]
	 * 	Nm=[[0,0,1],[1,0,0],[0,1,0]]
	 */

	const int8_t mapping_inertial[3][3] = { { 0, 0,1 }, {-1, 0, 0 }, { 0, -1, 0 } };
	const int8_t mapping_magnetic[3][3] = { { 0, 0,1 }, { 1, 0, 0 }, { 0,1, 0 } };
	bhi160_set_mapping(&S, P3_ACC, mapping_inertial);
	bhi160_set_mapping(&S, P3_UMAG, mapping_magnetic);
	bhi160_set_mapping(&S, P3_UGYR, mapping_inertial);
	
	event_info(&S, P3_ACC)->cb = decode_vector;
	event_info(&S, P3_MAG)->cb = decode_vector;
	event_info(&S, P3_UMAG)->cb = decode_vector;
	event_info(&S, P3_GYR)->cb = decode_vector;
	event_info(&S, P3_ORIENT)->cb = decode_vector;

	/* Set up accelerometer, magnetic and gyro calibrated sensors. */
	samplerate = SENSOR_SAMPLE_RATE_HZ;
	latency = 0;
	sensitivity = 0;
	dynrange = 2;
	CHECK(bhi160_set_sensor_config(&S, P3_ACC, &samplerate, &latency, &sensitivity, &dynrange),0,"Sensor config\n");
#ifdef CLI_TEST
	samplerate = SENSOR_SAMPLE_RATE_HZ;
	latency = 0;
	sensitivity = 0;
	dynrange = 2000;
	CHECK(bhi160_set_sensor_config(&S, P3_MAG, &samplerate, &latency, &sensitivity, &dynrange),0,"Sensor config\n");

	samplerate = SENSOR_SAMPLE_RATE_HZ;
	latency = 0;
	sensitivity = 0;
	dynrange = 1;
	CHECK(bhi160_set_sensor_config(&S, P3_GYR, &samplerate, &latency, &sensitivity, &dynrange),0,"Sensor config\n");
#endif
	/* Set up fused orientation sensor */
	samplerate = SENSOR_SAMPLE_RATE_HZ;
	latency = 0;
	sensitivity = 0;
	dynrange = 1;
	CHECK(bhi160_set_sensor_config(&S, P3_ORIENT, &samplerate, &latency, &sensitivity, &dynrange),0,"Sensor config\n");

	LS_NOTICE("Polling sensor data\n");

	while (!terminate) {
		int r;
		struct event_info *EI = NULL;

		r = bhi160_poll_fifo(&S, &EI);
		if (r > 0) {
			if (EI->cb) EI->cb(&S, EI);
			else if (EI->id != 0)
				LS_ERROR("Evpoll: r=%d, ev=%d, evtsize=%d\n", r, EI->id, EI->size);
			continue;
		}
		if (r < 0) {
			LS_ERROR("Poll error %d\n", r);
		}
#ifdef HAS_RDB_SUPPORT
		monitor_rpc();
#else
		std::this_thread::sleep_for(std::chrono::milliseconds(FIFO_POLL_DELAY_MS));
#endif
	}

	bhi160_close(&S);

#ifdef HAS_RDB_SUPPORT
	terminate_rdb_services();
#endif

	closelog();
	return 0;
}
