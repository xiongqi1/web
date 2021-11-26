#ifndef __SIERRA_MC8355_H__
#define __SIERRA_MC8355_H__

#include "../abstractcmd.h"

int sierra_mc8355_info(struct abstractcmd_t* abstract,struct abstractcmd_param_t* param);
int sierra_mc8355_detect(struct abstractcmd_t* abstract,struct abstractcmd_param_t* param);

#endif

