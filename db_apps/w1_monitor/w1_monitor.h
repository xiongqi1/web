#ifndef __W1_MONITOR_H__29112016
#define __W1_MONITOR_H__29112016

/*
 * Copyright Notice:
 * Copyright (C) 2016 NetComm Wireless limited.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Ltd.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETCOMM
 * WIRELESS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * W1_monitor reads detected 1-wire devices information via netlink/connector and
 * save to RDB variables.
 */

#include <asm/types.h>
#include <sys/types.h>

/* All below definitions are directly imported from header files in kernel source folder,
 * connector.h, w1.h, w1_netlink.h so keep the type unchanged for reference.
 */

/* well-known 1-wire slave families list */
#define W1_FAMILY_DEFAULT	"00"
#define W1_FAMILY_SMEM_01	"01"
#define W1_FAMILY_SMEM_81	"81"
#define W1_THERM_DS18S20	"10"
#define W1_FAMILY_DS28E04	"1C"
#define W1_COUNTER_DS2423	"1D"
#define W1_THERM_DS1822		"22"
#define W1_EEPROM_DS2433	"23"
#define W1_THERM_DS18B20	"28"
#define W1_FAMILY_DS2408	"29"
#define W1_EEPROM_DS2431	"2D"
#define W1_FAMILY_DS2760	"30"
#define W1_FAMILY_DS2780	"32"
#define W1_THERM_DS1825		"3B"
#define W1_FAMILY_DS2781	"3D"
#define W1_THERM_DS28EA00	"42"

#define MAX_FID_LEN         2

/* ID string : 15 bytes = 8 bits FID + '-' + 48 bits serial number, ex) 01-000016b3b0fa
 * Actuial device id string for RAB variable is 15 but defined its buffer length to 16.
 */
#define MAX_DID_LEN         16

#define MAX_NAME_LEN        128
#define MAX_DATA_LEN        1024

/* The number of maximum slave device is depending on the hardware capacity of
 * master device and the current consumption of each slave devices. In most cases,
 * 1-wire slave devices consume less than few miliamps in parasite mode so
 * usually even more than 200 slave devices are possible but that is not realistic.
 * In this implementation the maximum number of slave devices are limited to 10.
 */
#define MAX_SLAVE_DEVICES   10

#define APPLICATION_NAME "w1_monitor"
#define LOCK_FILE "/var/lock/subsys/"APPLICATION_NAME

#define W1_RBD_ROOT "sensors.w1"

//#define DEBUG
#ifdef DEBUG
#define D(f, a...) do {syslog(LOG_INFO, f, ##a);} while (0)
#else
#define D(f, a...)
#endif

int process_w1_sl_ds18xxx(int dbIdx, int idx);

typedef struct
{
	const char *fid;                       /* string form of 8 bits unique family id of the device */
	const char *sname;                     /* short family name */
	const char *name;                      /* family name */
	int (*p_sl_process)(int, int);         /* function to process slave device data */
} w1_sl_list_type;

#define	NAME_BIT        0x10
#define	SNAME_BIT       0x08
#define	DID_BIT         0x04
#define	PATH_BIT        0x02
#define	DATA_BIT        0x01
#define	MASKALL         0x1F

typedef struct
{
	char name[MAX_NAME_LEN];     /* device name */
	char sName[MAX_NAME_LEN];    /* short device name */
	char id[MAX_DID_LEN];        /* device id */
	char path[MAX_NAME_LEN];     /* device path */
	char data[MAX_DATA_LEN];     /* device data */
	int  flag;                   /* changed field bitmap */
} w1_dev_type;

#define W1_SYSDEV_PATH	"/sys/bus/w1/devices"

#define UPDATE_FIELD(f1, f2, b, m)  do { if (strcmp(f1, f2)) {strcpy(f1,f2); b|=m;}} while (0)

#endif /* __W1_MONITOR_H__29112016 */