#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <time.h>
#include "ds_store.h"
#include "netstat.h"

ssize_t rdbcloseoff(ds_store *p)
{
	if (add_relative_timestamp(p) < 0)
		return -1;
	if (ds_strcat(p, "\n}\n") < 0)
		return -1;
	return ds_size_data(p);
}

#ifdef HAVE_RDB

#include <rdb_ops.h>
static struct rdb_session *rdb_sess = NULL;

static int check_rdb_session(void)
{
int retval;
	if (rdb_sess)
		return 0;
	retval = rdb_open(NULL, &rdb_sess);
	if (retval < 0 || !rdb_sess) {
		return -1;
	}
	return 0;
}


static int rdbvalues(ds_store *p, char *vars)
{
char *buf = NULL, *sp, *vn, *vns;
int size = 1024, len, ret;
	vns = vars;
	sp = NULL;
	buf = malloc(size);
	if (!buf)
		return -1;

	while((vn = strtok_r(vns, "&", &sp))) {
		vns = NULL;
		while(1) {
			buf[0] = '\0';
			len = size - 1;
			errno = 0;
			ret = rdb_get(rdb_sess, vn, buf, &len);
			if (!ret || errno != EOVERFLOW) {
				buf[len] = 0;
				if (ds_catstrs(p, "\"", vn, "\" : \"", buf, "\",\n", NULL) < 0) {
					free(buf);
					return -1;
				}
				break;
			}
			size <<= 1;
			if(!(buf = realloc(buf, size))) {
				return -1;
			}
		}
	}
	free(buf);
	return 0;
}

/*
	if(rdb_lock(rdb_sess, 0)) {
	if(rdb_unlock(rdb_sess)) {
*/

ssize_t rdbstats(ds_store *p, const char *in_vars)
{
char *search, *search_holder, *search_next;
char *buf = NULL, *name;
int size = 1024, len, ret;
	
	if (ds_strcat(p, "{\n") < 0)
		return -1;
	if (check_rdb_session() < 0)
		return -1;
	if (!in_vars || *in_vars == '\0')
		return rdbcloseoff(p);

	buf = malloc(size);
	if (!buf)
		return -1;

	if ((search = strdup(in_vars)) == NULL)
		return -1;

	search_next = search;
	search_holder = NULL;
	while((name = strtok_r(search_next, "&", &search_holder))) {
		search_next = NULL;
		while(1) {
			buf[0] = '\0';
			len = size - 1;
			errno = 0;
			ret = rdb_getnames(rdb_sess, name, buf, &len, 0);
			if (!ret || errno != EOVERFLOW) {
				buf[len] = 0;
				ret = rdbvalues(p, buf);
				break;
			}
			size <<= 1;
			if(!(buf = realloc(buf, size))) {
				free(search);
				return -1;
			}
		}
	}
	free(buf);
	free(search);
	return rdbcloseoff(p);
}
#else
ssize_t rdbstats(ds_store *p, const char *in_vars)
{
	if (ds_strcat(p, "{\n") < 0)
		return -1;
	if (ds_catstrs(p, "\"rdb_not_present\" : true,\n", NULL) < 0) {
		return -1;
	}
	return rdbcloseoff(p);
}

#endif
