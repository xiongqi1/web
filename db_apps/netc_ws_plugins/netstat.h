#ifndef NETSTAT_H_DEFINED
#define NETSTAT_H_DEFINED

ssize_t show_interfaces(ds_store *p);

ssize_t add_relative_timestamp(ds_store *p);
ssize_t netstats(ds_store *p, const char *iface);
ssize_t rdbstats(ds_store *p, const char *vars);
ssize_t pathstats(ds_store *p, const char *paths);
ssize_t ldir(ds_store *p, const char *path);
typedef ssize_t  (*stats_cb) (ds_store *p, const char *in_path, void *arg);
ssize_t dostats(ds_store *p, const char *in_paths, stats_cb cb, void *arg);
ssize_t do_ping(ds_store *p, const char *path);
ssize_t do_traffic(ds_store *p, const char *path);

#endif
