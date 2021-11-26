#ifndef __INDEXED_RDB_RCP__
#define __INDEXED_RDB_RCP__

#include "qmirdbctrl.h"
#include "rwpipe.h"
#include "dbhash.h"

struct indexed_rdb_t {
	struct dbhash_t* hash_cmds;
	const char* cmds[vc_last];
};

struct indexed_rdb_t* indexed_rdb_create(struct dbhash_element_t* cmds, int cmd_cnt);
const char* indexed_rdb_get_cmd_str(struct indexed_rdb_t* ir, int cmd_idx);
int indexed_rdb_get_cmd_idx(struct indexed_rdb_t* ir, const char* cmd_str);
void indexed_rdb_destroy(struct indexed_rdb_t* ir);

#endif
