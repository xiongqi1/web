
#ifndef __HWCLS_H__
#define __HWCLS_H__

#include <stdio.h>
#include "list.h"

#include "hwdev.h"

#define HWCLS_RDB_PREFIX	"sys.hw.class."

enum hwcls_stat_t {
	hwcls_stat_removed,
	hwcls_stat_inserted,
};

enum hwcls_type_t {
	hwcls_type_unknown,
	hwcls_type_serial,
	hwcls_type_eth,
	hwcls_type_storage,
	hwcls_type_pm,
	
	hwcls_type_max,
};

enum hwcls_pmdev_type {
	hwcls_pmdev_type_at,
	hwcls_pmdev_type_gps,
	hwcls_pmdev_type_diag,
	hwcls_pmdev_type_eth,
	hwcls_pmdev_type_ctl,
	hwcls_pmdev_type_data0,
	hwcls_pmdev_type_data1,
	hwcls_pmdev_type_data2,
	hwcls_pmdev_type_data3,
	hwcls_pmdev_type_data4,
};


/* hardware class */
struct hwcls_t {
	int idx;
	
	enum hwcls_type_t type; /* device type */
	
	struct hwdev_t* hwdev;
	
	char id[HWDEV_ID_LEN];
	char loc[HWDEV_LOC_LEN];
	
	enum hwcls_stat_t stat;
	
	struct list_head list;
	
	int enable;

	int legacy;
	char legacy_name[HWDEV_RDB_DEV_NAME];
};


int hwcls_init();
void hwcls_fini();

int hwcls_search_by_id_and_loc(const struct hwdev_t* key,struct hwcls_t** hc);
struct hwcls_t* hwcls_get();
void hwcls_put(struct hwcls_t* hc);

int hwcls_write_to_rdb(struct hwcls_t* hc,int removed);
const char* hwcls_get_class_name(struct hwcls_t* hc);
const char* hwcls_get_class_stat(struct hwcls_t* hc);
const char* hwcls_get_class_type(struct hwcls_t* hc);

int hwcls_fixup(struct hwcls_t* hc,int valid_idx);

int hwcls_read_from_rdb(struct hwcls_t* hc,const char* rdb);

int hwcls_get_next(struct hwcls_t** hc);
int hwcls_get_first(struct hwcls_t** hc);

#endif
