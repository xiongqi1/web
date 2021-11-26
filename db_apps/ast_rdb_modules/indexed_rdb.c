#include "indexed_rdb.h"

#include <syslog.h>
#include <stdlib.h>


void indexed_rdb_destroy(struct indexed_rdb_t* ir)
{
	if(!ir)
		return;

	dbhash_destroy(ir->hash_cmds);

	free(ir);
}

int indexed_rdb_get_cmd_idx(struct indexed_rdb_t* ir, const char* cmd_str)
{
	return dbhash_lookup(ir->hash_cmds, cmd_str);
}

const char* indexed_rdb_get_cmd_str(struct indexed_rdb_t* ir, int cmd_idx)
{
	if(cmd_idx < 0 || cmd_idx >= vc_last) {
		syslog(LOG_ERR, "command index out of range in indexed_rdb_get_cmd_str()");
		goto err;
	}

	return ir->cmds[cmd_idx];
err:
	return NULL;
}

struct indexed_rdb_t* indexed_rdb_create(struct dbhash_element_t* cmds, int cmd_cnt)
{
	struct indexed_rdb_t* ir;

	ir = calloc(1, sizeof(*ir));
	if(!ir) {
		syslog(LOG_ERR, "calloc() failed in indexed_rdb_create()");
		goto err;
	}

	ir->hash_cmds = dbhash_create(cmds, cmd_cnt);
	if(!ir->hash_cmds) {
		syslog(LOG_ERR, "dbhash_create(cmds) failed in indexed_rdb_create()");
		goto err;
	}

	int i;
	for(i = 0; i < cmd_cnt; i++) {

		if(cmds[i].idx < 0 || cmds[i].idx >= vc_last) {
			syslog(LOG_ERR, "command index out of range in indexed_rdb_create()");
			goto err;
		}

		ir->cmds[i] = cmds[i].str;
	}

	return ir;
err:
	indexed_rdb_destroy(ir);
	return NULL;
}
