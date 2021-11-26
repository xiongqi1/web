
#include <string.h>
#include "../model/model.h"
#include "../at/at.h"
#include "model_default.h"
#include "cdcs_syslog.h"

extern const struct command_t model_default_commands[];
extern const struct notification_t model_default_notifications[];

///////////////////////////////////////////////////////////////////////////////
static int model_longsung_init(void)
{
	//update_ccid();

	return 0;
}

///////////////////////////////////////////////////////////////////////////////
static int model_longsung_detect(const char* manufacture, const char* model_name)
{

	if(strstr(manufacture,"Manufacturer") && strstr(model_name,"HSPA USB MODEM"))
		return 1;

	return 0;
}
///////////////////////////////////////////////////////////////////////////////
int model_longsung_set_status(const struct model_status_info_t* chg_status,const struct model_status_info_t* new_status,const struct model_status_info_t* err_status)
{

	if(!at_is_open())
		return 0;

	// update ccid
	update_ccid();

	update_sim_status();

	update_signal_strength();

	update_imsi();

	update_network_name();
	update_service_type();
	
	//update_pdp_status();

	if(new_status->status[model_status_registered])
		update_call_forwarding();

	return 0;
}
///////////////////////////////////////////////////////////////////////////////
struct model_t model_longsung =
{
	.name = "Longsung",

	.init = model_longsung_init,
	.detect = model_longsung_detect,

	.get_status = model_default_get_status,
	.set_status = model_longsung_set_status,

	.commands = model_default_commands,
	.notifications = model_default_notifications
}; 
