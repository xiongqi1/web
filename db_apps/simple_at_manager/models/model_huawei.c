
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include "../at/at.h"
#include "../model/model.h"
#include "../util/rdb_util.h"
#include "../rdb_names.h"
#include "cdcs_syslog.h"
#include "rdb_ops.h"
#include "model_default.h"
#include "../util/at_util.h"

#include "../featurehash.h"


///////////////////////////////////////////////////////////////////////////////
static int model_huawei_init(void)
{
	return 0;
}

///////////////////////////////////////////////////////////////////////////////
static int model_huawei_detect(const char* manufacture, const char* model_name)
{
	const char* model_names[]={
		"EM820U",
	};

	// search for Ericsson in manufacture string
	if(strstr(manufacture,"huawei"))
		return 1;

	// compare model name
	int i;
	for (i=0;i<sizeof(model_names)/sizeof(const char*);i++)
	{
		if(!strcmp(model_names[i],model_name))
			return 1;
	}

	return 0;
}

///////////////////////////////////////////////////////////////////////////////
static int model_huawei_set_status(const struct model_status_info_t* chg_status,const struct model_status_info_t* new_status,const struct model_status_info_t* err_status)
{
	if(!at_is_open())
		return 0;

	update_sim_hint();
	update_roaming_status();
	
	/* skip if the variable is updated by other port manager such as cnsmgr or
	 * process SIM operation if module supports +CPINC command */
	if(is_enabled_feature(FEATUREHASH_CMD_ALLCNS) || support_pin_counter_at_cmd) {
		update_ccid();
		update_sim_status();
		/* Latest Sierra modems like MC8704, MC8801 etcs returns error for +CSQ command
		 * when SIM card is locked so update signal strength with cnsmgr. */
		if(is_enabled_feature(FEATUREHASH_CMD_ALLCNS)) {
			update_signal_strength();
		}
		update_network_name();
		update_service_type();
		update_pdp_status();
	}
	update_imsi();
	return 0;
}

///////////////////////////////////////////////////////////////////////////////
struct model_t model_huiwei =
{
	.name = "Huawei",

	.init = model_huawei_init,
	.detect = model_huawei_detect,

	.get_status = model_default_get_status,
	.set_status = model_huawei_set_status,

	.commands = NULL,
	.notifications = NULL
};
