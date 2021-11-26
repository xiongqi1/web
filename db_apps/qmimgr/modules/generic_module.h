#ifndef __GENERIC_MODULE_H__
#define __GENERIC_MODULE_H__

#include "../abstractcmd.h"

int generic_module_info(struct abstractcmd_t* abstract,struct abstractcmd_param_t* param);
int generic_module_detect(struct abstractcmd_t* abstract,struct abstractcmd_param_t* param);

#endif
