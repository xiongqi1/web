#ifndef __HWDEV_H__
#define __HWDEV_H__

#include <stdio.h>
#include "list.h"

#define HWDEV_RDB_PREFIX	"sys.hw.dev."

#define HWDEV_MAX_DEVICE	99

#define HWDEV_RDB_DEV_NAME	32
#define HWDEV_NAME_LEN		128
#define HWDEV_ID_LEN		128
#define HWDEV_LOC_LEN		128
#define HWDEV_DEVPATH_LEN	128
#define HWDEV_DRIVER_LEN	128
#define HWDEV_MAC_LEN		(16*3)

enum hw_dev_type_t {
	hw_dev_type_unknown,
	hw_dev_type_serial,
	hw_dev_type_eth,
	hw_dev_type_storage,
};

struct hwcls_t;

struct hwdev_t {

	int idx; /* rdb device index */
	
	char* name; /* device driver filename */
	enum hw_dev_type_t type; /* device type */
	
	char* loc; /* device location */
	char* id; /* unique ID of device - vendor id, product id, serial and mac */
	char* devpath; /* sysfs devpath */
	char* driver;
	
	int inf; /* device interface */
	
	char* owner; /* owner info */
	
	struct list_head list;
	
	struct hwcls_t* hwcls;	
	
	/* block device info */
	long long size;
	int partition;

	/* network device info */
	char* mac;
};

struct hwdev_t* hwdev_search_by_devpath(const char* devpath);

struct hwdev_t* hwdev_get();
void hwdev_put(struct hwdev_t* hd,int tail);
int hwdev_fixup(struct hwdev_t* hd);

void hwdev_fini();
int hwdev_init();

const char* hwdev_get_dev_name(struct hwdev_t* hd);
const char* hwdev_get_hwdev_name(struct hwdev_t* hd);
int hwdev_write_to_rdb(struct hwdev_t* hd,int removed);
const char* hwdev_get_dev_type(struct hwdev_t* hd);


#endif
