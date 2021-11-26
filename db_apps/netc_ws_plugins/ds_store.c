#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <sys/types.h>
#include "ds_store.h"

void ds_clear(ds_store * dsptr, ssize_t save_amount)
{
ssize_t residual;
	if (save_amount > 0 && save_amount < dsptr->end_index) {
		residual = dsptr->end_index - save_amount;
		memmove(dsptr->data, dsptr->data + save_amount, residual);
		dsptr->end_index = residual;
	} else
		dsptr->end_index = 0;
}

void ds_free(ds_store * dsptr)
{
	free(dsptr->data);
	dsptr->data = NULL;
	dsptr->end_index = 0;
	dsptr->max_size = 0;
}

ssize_t ds_resize(ds_store *p, ssize_t need)
{
	if (need == 0) {
		ds_free(p);
		return 0;
	}
	if (need > p->max_size) {
		unsigned char *rall = realloc(p->data, need);
		if (!rall) {
			ds_free(p);
			return -1;
		} else {
			p->data = rall;
			p->max_size = need;
		}
		if (p->end_index > p->max_size)
			p->end_index = p->max_size;
	}
	return p->max_size;
}

/* prepare the buffer for at least this amount. Be generous. There is a s use case for
 * getting a buffer and holding it.
 * Returns -1 for error, or the amount that can be added (can be larger than requested)
 */

ssize_t ds_prepare(ds_store *p, ssize_t extra)
{
ssize_t newsize;
	if (p->end_index < p->max_size && p->end_index + extra <= p->max_size)
		return p->max_size - p->end_index;
	newsize = p->max_size * 2;
	if (newsize < p->max_size + extra) {
		newsize = p->max_size + extra;
	} else {
		if (!extra)
			extra = 256;
		newsize = extra * 2;
	}
	if (ds_resize(p, newsize) < 0)
		return -1;
	return p->max_size - p->end_index;
}

ssize_t ds_move_ptr(ds_store *p, ssize_t amount)
{
	if (amount <= 0)
		return 0;
	if (ds_resize(p, p->end_index + amount) < 0)
		return -1;
	p->end_index += amount;
	return amount;
}

ssize_t ds_memset(ds_store *p, ssize_t amount, int c)
{
	if (amount <= 0)
		return 0;
	if (ds_resize(p, p->end_index + amount) < 0)
		return -1;
	memset(&p->data[p->end_index], c, amount);
	p->end_index += amount;
	return amount;
}

ssize_t ds_memcpy(ds_store * p, void *data, ssize_t amount)
{
	if (amount <= 0)
		return 0;
	if (ds_resize(p, p->end_index + amount) < 0)
		return -1;
	assert(p);
	assert(p->max_size >= amount);
	assert(p->max_size - p->end_index >= amount);
	memcpy(&p->data[p->end_index], data, amount);
	p->end_index += amount;
	return amount;
}

ssize_t ds_cat(ds_store * p, void *data, ssize_t amount)
{
	if (amount > 0) {
		if (p->end_index && p->data && p->data[p->end_index-1] == '\0')
			p->end_index--;
		return ds_memcpy(p, data, amount + 1);
	}
	return 0;
}

ssize_t ds_strcat(ds_store * p, char *str)
{
	return ds_cat(p, str, strlen(str));
}

ssize_t ds_catstrs(ds_store *p, ...)
{
va_list ap;
char *s;
ssize_t len = 0;
ssize_t cur = 0;
	va_start(ap, p);
	while(1) {
		s = va_arg(ap, char *);
		if (s) {
			if ((cur = ds_strcat(p, s)) < 0) {
				return -1;
			}
			len += cur;
		} else
			break;
	}
	va_end(ap);
	return len;
}

