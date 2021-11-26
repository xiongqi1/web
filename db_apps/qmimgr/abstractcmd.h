#ifndef __ABSTRACTCMD_H__
#define __ABSTRACTCMD_H__

#include "generictree.h"

#define ABSTRACTCMD_CMDIO_MAX	100

#define ABSTRACTCMD_CMDIO_INFO		0x0001
#define ABSTRACTCMD_CMDIO_DETECT	0x0002


// command index
#define ABSTRACTCMD_CMD_IDX_INFO	0
#define ABSTRACTCMD_CMD_IDX_DETECT	1
#define ABSTRACTCMD_CMD_IDX_START	2

struct abstractcmd_t;
struct abstractcmd_param_t;

// command io function
typedef int (*abstractcmd_func)(struct abstractcmd_t* abstract,struct abstractcmd_param_t* param);

struct abstractcmd_param_t {
	void* in;
	void* out;

	int in_len;
	int out_len;
};

struct abstractcmd_param_in_detect_t {
	const char* model;
	const char* manufacture;
};

struct abstractcmd_param_out_info_t {
	const char* model;
	const char* manufacture;
};

struct abstractcmd_element_t {
	unsigned int idx;
	abstractcmd_func func;
};

// abstractcmd object
struct abstractcmd_t {
	struct generictree_t* cmdtree;
	struct abstractcmd_element_t* cmds;
};


#define __get_out_param(name,type)	((type)( (char*)(name)+sizeof(struct abstractcmd_param_t)))
#define __get_in_param(name,type)	((type)( (char*)(name)+sizeof(struct abstractcmd_param_t)+(name)->in_len))

#define __create_param(name,in_size,out_size) \
	struct abstractcmd_param_t* name=NULL; \
	do { \
		int len; \
		\
		len=sizeof(*name)+(in_size)+(out_size); \
		name=malloc(len); \
		if(name) { \
			memset(name,0,len); \
			\
			name->in_len=(in_size); \
			name->out_len=(out_size); \
		} \
	} while(0)

#define __destroy_param(name) _free(name)



#endif
