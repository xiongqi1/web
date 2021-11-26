#include "generic_module.h"
#include "hauwei_em820u.h"
#include "sierra_mc8355.h"

#include "../abstractcmd.h"

// generic module
static struct abstractcmd_element_t generic_module[]={
	{ABSTRACTCMD_CMDIO_DETECT,generic_module_detect},
	{0,0},
};


// SIERRA MC8355
static struct abstractcmd_element_t sierra_mc8355[]={
	{ABSTRACTCMD_CMDIO_DETECT,sierra_mc8355_detect},
	{0,0},
};


// HUAWEI EM820U
static struct abstractcmd_element_t huawei_em820u[]={
	{ABSTRACTCMD_CMDIO_DETECT,huawei_em820u_detect},
	{0,0},
};


// total modules
struct abstractcmd_element_t* modules[]={
	huawei_em820u,
	sierra_mc8355,
	generic_module,
 	0,
};
