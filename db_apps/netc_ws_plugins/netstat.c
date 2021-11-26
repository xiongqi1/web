#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/time.h>
#include "ds_store.h"
#include "netstat.h"

ssize_t add_relative_timestamp(ds_store *p)
{
	char time_string[100];

	static struct timeval tval_before;
	struct timeval tval_after, tval_result;

	if ((long int)tval_before.tv_sec == 0) {
		gettimeofday(&tval_before, NULL);
	}
	gettimeofday(&tval_after, NULL);
	timersub(&tval_after, &tval_before, &tval_result);

	snprintf(time_string, sizeof (time_string), "\"relative_timestamp\": %ld.%06ld\n", (long int)tval_result.tv_sec, (long int)tval_result.tv_usec);
	return (ds_strcat(p, time_string));
}

ssize_t dir_entries(ds_store *p, const char *path)
{
	DIR *dp;
	struct dirent *entry;
	int any = 0;

	if (ds_strcat(p, "[ ") < 0)
		return -1;

	if ((dp = opendir(path)) == NULL) {
		if (ds_strcat(p, "]") < 0)
			return -1;
		return ds_size_data(p);
	}
	while ((entry = readdir(dp)) != NULL) {
		if (entry->d_name[0] == '.' &&
			(entry->d_name[1] == '\0' ||
			(entry->d_name[1] == '.' && entry->d_name[2] == '\0')))
			continue;
		if (!any) {
			any = 1;
		} else {
			if (ds_strcat(p, ",\n") < 0)
				return -1;
		}
		if (ds_catstrs(p, "\"", entry->d_name, "\"" , NULL) < 0) {
			return -1;
		}
	}
	closedir(dp);
	if (ds_strcat(p, "\n]") < 0)
		return -1;
	return ds_size_data(p);
}

typedef struct {
	int newstring;
	char * cur, *prev;
} escaper;

ssize_t flush_saved(ds_store *p, escaper *escp, char *buf)
{
	if (escp->newstring) {
		if (ds_strcat(p, "\"<p>") < 0)
			return -1;
		escp->newstring = 0;
	}
	if (escp->cur != escp->prev && escp->cur != escp->prev+1) {
		if (ds_cat(p, escp->prev, escp->cur - escp->prev - 1) < 0)
			return -1;
		escp->prev = escp->cur;
	}
	if (buf[0] != '\0') {
		if (ds_strcat(p, buf) < 0)
			return -1;
	}
	return ds_size_data(p);
}

ssize_t reply_escape(ds_store *p, escaper *esc, char *reply)
{
	char buf[10];
	for (esc->prev = esc->cur = reply; *esc->cur != '\0'; esc->cur++) {
		if (*esc->cur == '\\' || *esc->cur == '"') {
			buf[0] = '\\';
			buf[1] = '\0';
			if (flush_saved(p, esc, buf) < 0)
				return -1;
		} else if (*esc->cur == '\n') {
			buf[0] = '\\';
			buf[1] = 'n';
			buf[2] = '\0';
			if (flush_saved(p, esc, "<br>") < 0)
				return -1;
		} else if (*esc->cur < 0x20) {
			sprintf(buf, "\\u%4.4u", *esc->cur);
			if (flush_saved(p, esc, buf) < 0)
				return -1;
		}
	}
	return ds_size_data(p);
}

ssize_t exec_wait(ds_store * p, const char *cmd)
{
	char buf[1024];
	FILE *fd;
	escaper esc;

	esc.newstring = 1;
	fd = popen(cmd, "r");
	if (!fd) {
		if (ds_strcat(p, "\"Can't open command for processing\"") < 0)
			return -1;
		return ds_size_data(p);
	}
	while (fgets(buf, sizeof(buf), fd)) {
		if (reply_escape(p, &esc, buf) < 0)
			return -1;
	}
	pclose(fd);
	if (flush_saved(p, &esc, "</p>\"") < 0)
		return -1;
	return ds_size_data(p);
}

ssize_t do_ping(ds_store *p, const char *path)
{
	const char *cp;
	char buffer[1024];

	if (ds_strcat(p, "[ ") < 0)
		return -1;

	for (cp = path; *cp != '\0'; cp++) {
		if (!(isalnum(*cp) || *cp == '.' || *cp == ':')) {
			if (ds_strcat(p, "\"Illegal host address\" ]") < 0)
				return -1;
			return ds_size_data(p);
		}
	}
	if (cp - path > sizeof(buffer) - 20) {
		if (ds_strcat(p, "\"Host address too long\" ]") < 0)
			return -1;
		return ds_size_data(p);
	}
	sprintf(buffer, "ping -c 1 -W 1 %s",  path);
	if (exec_wait(p, buffer) < 0)
		return -1;
	if (ds_strcat(p, "\n]") < 0)
		return -1;
	return ds_size_data(p);
}

#define WS_SS_PATH "share/lwsws/netc_netstat/ss.cgi"

ssize_t do_traffic(ds_store *p, const char *path)
{
	if (ds_strcat(p, "[ ") < 0)
		return -1;

	setenv("SSCGI_NO_FULL", "YES", 1);
	setenv("REQUEST_METHOD", "GET", 1);
	setenv("QUERY_STRING", path, 1);
	if (exec_wait(p, WS_SS_PATH) < 0)
		return -1;
	if (ds_strcat(p, "\n]") < 0)
		return -1;
	return ds_size_data(p);
}

ssize_t filestats(ds_store *p, const char *dirname)
{
	DIR *dp;
	struct dirent *entry;
	struct stat statbuf;
	FILE *fp;
	char filename[1000];
	char buffer[1000];
	char *cp;
	
	if (ds_strcat(p, "{\n") < 0)
		return -1;
	if ((dp = opendir(dirname)) == NULL) {
		goto finito;
	}
	while ((entry = readdir(dp)) != NULL) {
		if (snprintf(filename, sizeof(filename), "%s/%s", dirname, entry->d_name) >= sizeof(filename))
			continue;

		lstat(filename, &statbuf);
		if (S_ISDIR(statbuf.st_mode))
			continue;
		fp = fopen(filename, "r");
		if (fp == NULL)
			continue;
		if (fgets(buffer, sizeof(buffer) - 1, fp) != NULL) {
			if ((cp = strchr(buffer, '\n')) != NULL) {
				*cp = '\0';
			}
			if (ds_catstrs(p, "\"", entry->d_name, "\" : \"", buffer, "\",\n", NULL) < 0) {
				break;
			}
		}
		fclose(fp);
	}
	closedir(dp);
finito:
	if (add_relative_timestamp(p) < 0)
		return -1;
	if (ds_strcat(p, "\n}") < 0)
		return -1;
	return ds_size_data(p);
}

static char *SYSDIR = "/sys/class/net";

ssize_t show_interfaces(ds_store *p)
{
	return dir_entries(p, SYSDIR);
}

ssize_t netstats(ds_store *p, const char *iface)
{
	char dirname[1000];
	
	if (snprintf(dirname, sizeof(dirname), "%s/%s/statistics", SYSDIR, iface) >= sizeof(dirname))
		return ds_size_data(p);
	return filestats(p, dirname);
}

ssize_t dostats(ds_store *p, const char *in_paths, stats_cb cb, void *arg)
{
char *search, *search_holder, *search_next, *name;
	
	if (!in_paths || *in_paths == '\0')
		return ds_size_data(p);

	if ((search = strdup(in_paths)) == NULL)
		return -1;

	search_next = search;
	search_holder = NULL;
	while((name = strtok_r(search_next, "&", &search_holder))) {
		search_next = NULL;
		if (cb(p, name, arg) < 0) {
			free(search);
			return -1;
		}
	}
	free(search);
	return ds_size_data(p);
}

ssize_t ldir_cb(ds_store *p, const char *path, void *arg)
{
int *any = (int*)arg;
	if (!*any) {
		*any = 1;
	} else {
		if (ds_strcat(p, ", ") < 0)
			return -1;
	}
	if (ds_catstrs(p, "\"", path, "\" : ", NULL) < 0) {
		return -1;
	}
	return dir_entries(p, path);
}

ssize_t ldir(ds_store *p, const char *path)
{
int any = 0;
int ret;
	if (ds_strcat(p, "{\n") < 0)
		return -1;
	ret = dostats(p, path, ldir_cb, &any);
	if (ret < 0)
		return ret;
	if (ds_strcat(p, "\n}") < 0)
		return -1;
	return ds_size_data(p);
}

ssize_t pathstats_cb(ds_store *p, const char *path, void *arg)
{
int *any = (int*)arg;
	if (!*any) {
		*any = 1;
	} else {
		if (ds_strcat(p, ", ") < 0)
			return -1;
	}
	if (ds_catstrs(p, "\"", path, "\" : ", NULL) < 0) {
		return -1;
	}
	return filestats(p, path);
}

ssize_t pathstats(ds_store *p, const char *in_path)
{
int any = 0;
int ret;
	if (ds_strcat(p, "{\n") < 0)
		return -1;
	if (!in_path || *in_path == '\0') {
		if (ds_strcat(p, "\n}") < 0)
			return -1;
		return ds_size_data(p);
	}
	ret = dostats(p, in_path, pathstats_cb, &any);
	if (ret < 0)
		return ret;
	if (ds_strcat(p, "\n}") < 0)
		return -1;
	return ds_size_data(p);
}
#ifdef TEST_H

#define SEND_IFACE	0
#define SEND_LIST	1
#define SEND_RDB	2
#define SEND_LDIR	3
#define SEND_CONTENTS	4
#define SEND_LLPING	5
#define SEND_TRAFFIC	6

static int send_data(ds_store *ds, char *paths, int cmd)
{
	if (!ds->data)
		ds_init(ds,256);
	if (!ds->data) {
		printf("ERROR memory allocation ds_store\n");
		return -1;
	}
	ds_clear(ds, 0);
	switch(cmd) {
	case SEND_IFACE:
		if (paths[0]) {
			if (ds_strcat(ds, "{\"IFACE\": ") < 0)
				return -1;
			if (netstats(ds, paths) < 0) {
				printf("ERROR creating stats of %s\n", paths);
				return -1;
			}
			if (ds_strcat(ds, "}\n") < 0)
				return -1;
		} else {
			return 0;
		}
		break;
	case SEND_LIST:
		if (ds_strcat(ds, "{\"LIST\": ") < 0)
				return -1;
		if (show_interfaces(ds) < 0) {
			printf("ERROR creating list of interfaces\n");
			return -1;
		}
		if (ds_strcat(ds, "}\n") < 0)
			return -1;
		break;
	case SEND_RDB:
		if (paths[0]) {
			if (ds_strcat(ds, "{\"RDB\": ") < 0)
				return -1;
			if (rdbstats(ds, paths) < 0) {
				printf("ERROR creating stats of %s\n", paths);
				return -1;
			}
			if (ds_strcat(ds, "}\n") < 0)
				return -1;
		} else {
			return 0;
		}
		break;
	case SEND_CONTENTS:
		if (paths[0]) {
			if (ds_strcat(ds, "{\"CONTENTS\": ") < 0)
				return -1;
			if (pathstats(ds, paths) < 0) {
				printf("ERROR creating var list of %s\n", paths);
				return -1;
			}
			if (ds_strcat(ds, "}\n") < 0)
				return -1;
		} else {
			return 0;
		}
		break;
	case SEND_LDIR:
		if (paths[0]) {
			if (ds_strcat(ds, "{\"LDIR\": ") < 0)
				return -1;
			if (ldir(ds, paths) < 0) {
				printf("ERROR creating var list of %s\n", paths);
				return -1;
			}
			if (ds_strcat(ds, "}\n") < 0)
				return -1;
		} else {
			return 0;
		}
		break;
	case SEND_LLPING:
		if (paths[0]) {
			if (ds_strcat(ds, "{\"LLPING\": ") < 0)
				return -1;
			if (do_ping(ds, paths) < 0) {
				printf("ERROR creating ping of %s\n", paths);
				return -1;
			}
			if (ds_strcat(ds, "}\n") < 0)
				return -1;
		} else {
			return 0;
		}
		break;
	case SEND_TRAFFIC:
		if (paths[0]) {
			if (ds_strcat(ds, "{\"TRAFFIC\": ") < 0)
				return -1;
			if (do_traffic(ds, paths) < 0) {
				printf("ERROR creating traffic of %s\n", paths);
				return -1;
			}
			if (ds_strcat(ds, "}\n") < 0)
				return -1;
		} else {
			return 0;
		}
		break;
	default:
		return 0;
	}
	return 0;
}

static char paths[4096];


static ssize_t cppath(const char *p, size_t n, size_t len)
{
	paths[0] = '\0';
	while (n < len) {
		if (p[n] != ' ')
			break;
		n++;
	}
	if (n < len && p[n] != '\0' && len - n < sizeof(paths) ) {
		strncpy(paths, p+n, len - n);
		paths[len - n] = '\0';
		return len;
	}
	return -1;
}

int main(int argc, char *argv[])
{
	ds_store ds;
	ds_init(&ds, 256);
	char buffer[256];
	char *p;
	char *d;
	int cmd;
	size_t len;

	if (argc < 2) {
		if (show_interfaces(&ds) > 0) {
			printf("%s\n", ds.data);
		}
	} else if (argc == 2) {
		if (snprintf(buffer, sizeof(buffer), "\"%s\": ", argv[1]) < sizeof(buffer)) {
			/* if (ds_strcat(&ds, buffer ) < 0)
				exit(1); */
			if (netstats(&ds, argv[1]) > 0) {
				printf("%s\n", ds.data);
			}
		}
	} else {
		d = argv[1];
		p = argv[2];
		len = strlen(p);
		paths[0] = '\0';
		if (strncasecmp(d, "LIST", 4) == 0) {
			cmd = SEND_LIST;
		} else if (strncasecmp(d, "IFACE", 5) == 0) {
			if(cppath(p, 0, len) < 0) {
				printf("ERROR interface name bad\n");
				return -1;
			}
			printf("netc_netstat: interface %s stats requested\n", paths);
			cmd = SEND_IFACE;
		} else if (strncasecmp(d, "RDB", 3) == 0) {
			if(cppath(p, 0, len) < 0) {
				printf("ERROR rdb variables required\n");
				return -1;
			}
			printf("netc_netstat: rdb variables %s requested\n", paths);
			cmd = SEND_RDB;
		} else if (strncasecmp(d, "CONTENTS", 8) == 0) {
			if(cppath(p, 0, len) < 0) {
				printf("ERROR paths required\n");
				return -1;
			}
			printf("netc_netstat: directory file contents as variables %s requested\n", paths);
			cmd = SEND_CONTENTS;
		} else if (strncasecmp(d, "LDIR", 4) == 0) {
			if(cppath(p, 0, len) < 0) {
				printf("ERROR paths required\n");
				return -1;
			}
			printf("netc_netstat: directory file names %s requested\n", paths);
			cmd = SEND_LDIR;
		} else if (strncasecmp(d, "LLPING", 6) == 0) {
			if(cppath(p, 0, len) < 0) {
				printf("ERROR ip address bad\n");
				return -1;
			}
			printf("netc_netstat: ping %s requested\n", paths);
			cmd = SEND_LLPING;
		} else if (strncasecmp(d, "TRAFFIC", 7) == 0) {
			if(cppath(p, 0, len) < 0) {
				printf("ERROR traffic command not present\n");
				return -1;
			}
			printf("netc_netstat: traffic %s requested\n", paths);
			cmd = SEND_TRAFFIC;
		} else {
			printf("netc_netstat: command  %s not recognised\n", d);
			return -1;
		}
		if (send_data(&ds, paths, cmd) < 0) {
			printf("Error\n");
		} else {
			printf("%s\n", ds.data);
		}
	}
	return 0;
}
#endif

