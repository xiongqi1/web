#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include "ds_store.h"
#include "netstat.h"

#define LWS_DLL
#define LWS_INTERNAL
#include "libwebsockets.h"

struct per_vhost_data__iface_stats {
	uv_timer_t timeout_watcher;
	struct lws_context *context;
	struct lws_vhost *vhost;
	const struct lws_protocols *protocol;
};

struct per_session_data__iface_stats {
	char	iface_name[64];
	char	rdb_paths[2049];
	char	contents_paths[2049];
	ds_store store;
	int	mode;
};

static void
uv_timeout_cb_iface_stats(uv_timer_t *w
#if UV_VERSION_MAJOR == 0
		, int status
#endif
)
{
	struct per_vhost_data__iface_stats *vhd = lws_container_of(w,
			struct per_vhost_data__iface_stats, timeout_watcher);
	lws_callback_on_writable_all_protocol_vhost(vhd->vhost, vhd->protocol);
}

#define SEND_IFACE	0
#define SEND_LIST	1
#define SEND_RDB	2
#define SEND_LDIR	3
#define SEND_CONTENTS	4
#define SEND_LLPING	5
#define SEND_TRAFFIC	6

#define MODE_IFACE	1
#define MODE_RDB	2
#define MODE_CONTENTS	4

static int send_data(struct lws *wsi, struct per_session_data__iface_stats *pss, int cmd, char *paths)
{
	int n, m;
	unsigned char *p;

	if (!pss->store.data)
		ds_init(&pss->store, 256);
	if (!pss->store.data) {
		lwsl_err("ERROR memory allocation ds_store\n");
		return -1;
	}
	ds_clear(&pss->store, 0);
	if (ds_memset(&pss->store, LWS_SEND_BUFFER_PRE_PADDING+1, 0) < 0) {
		lwsl_err("ERROR memory pre-allocation ds_store\n");
		return -1;
	}
	switch(cmd) {
	case SEND_IFACE:
		if (pss && paths && paths[0]) {
			if (ds_strcat(&pss->store, "{\"IFACE\": ") < 0)
				return -1;
			if (netstats(&pss->store, paths) < 0) {
				lwsl_err("ERROR creating stats of %s\n", paths);
				return -1;
			}
			if (ds_strcat(&pss->store, "}\n") < 0)
				return -1;
		} else {
			return 0;
		}
		break;
	case SEND_LIST:
		if (ds_strcat(&pss->store, "{\"LIST\": ") < 0)
				return -1;
		if (show_interfaces(&pss->store) < 0) {
			lwsl_err("ERROR creating list of interfaces\n");
			return -1;
		}
		if (ds_strcat(&pss->store, "}\n") < 0)
			return -1;
		break;
	case SEND_RDB:
		if (pss && paths && paths[0]) {
			if (ds_strcat(&pss->store, "{\"RDB\": ") < 0)
				return -1;
			if (rdbstats(&pss->store, paths) < 0) {
				lwsl_err("ERROR creating stats of %s\n", paths);
				return -1;
			}
			if (ds_strcat(&pss->store, "}\n") < 0)
				return -1;
		} else {
			return 0;
		}
		break;
	case SEND_CONTENTS:
		if (pss && paths && paths[0]) {
			if (ds_strcat(&pss->store, "{\"CONTENTS\": ") < 0)
				return -1;
			if (pathstats(&pss->store, paths) < 0) {
				lwsl_err("ERROR creating var list of %s\n", paths);
				return -1;
			}
			if (ds_strcat(&pss->store, "}\n") < 0)
				return -1;
		} else {
			return 0;
		}
		break;
	case SEND_LDIR:
		if (pss && paths && paths[0]) {
			if (ds_strcat(&pss->store, "{\"LDIR\": ") < 0)
				return -1;
			if (ldir(&pss->store, paths) < 0) {
				lwsl_err("ERROR creating var list of %s\n", paths);
				return -1;
			}
			if (ds_strcat(&pss->store, "}\n") < 0)
				return -1;
		} else {
			return 0;
		}
		break;
	case SEND_LLPING:
		if (pss && paths && paths[0]) {
			if (ds_strcat(&pss->store, "{\"LLPING\": ") < 0)
				return -1;
			if (do_ping(&pss->store, paths) < 0) {
				printf("ERROR creating ping of %s\n", paths);
				return -1;
			}
			if (ds_strcat(&pss->store, "}\n") < 0)
				return -1;
		} else {
			return 0;
		}
		break;
	case SEND_TRAFFIC:
		if (pss && paths && paths[0]) {
			if (ds_strcat(&pss->store, "{\"TRAFFIC\": ") < 0)
				return -1;
			if (do_traffic(&pss->store, paths) < 0) {
				printf("ERROR creating traffic of %s\n", paths);
				return -1;
			}
			if (ds_strcat(&pss->store, "}\n") < 0)
				return -1;
		} else {
			return 0;
		}
		break;
	default:
		return 0;
	}
	if (ds_memset(&pss->store, LWS_SEND_BUFFER_POST_PADDING, 0) < 0) {
		lwsl_err("ERROR memory post-allocation ds_store\n");
		return -1;
	}
	n = ds_size_data(&pss->store);
	if (n > LWS_SEND_BUFFER_PRE_PADDING+LWS_SEND_BUFFER_POST_PADDING+1) {
		p = (ds_data(&pss->store))+LWS_SEND_BUFFER_PRE_PADDING;
		n = strlen((char*)p);
		m = lws_write(wsi, p, n, LWS_WRITE_TEXT);
		if (m < n) {
			lwsl_err("ERROR %d writing to di socket\n", n);
			return -1;
		}
	}
	return 0;
}

static int
callback_iface_stats(struct lws *wsi, enum lws_callback_reasons reason,
			void *user, void *in, size_t len)
{
	struct per_session_data__iface_stats *pss =
			(struct per_session_data__iface_stats *)user;
	struct per_vhost_data__iface_stats *vhd =
			(struct per_vhost_data__iface_stats *)
			lws_protocol_vh_priv_get(lws_get_vhost(wsi),
					lws_get_protocol(wsi));
	const char *p;
	size_t n;
	char paths[2049];

	switch (reason) {
	case LWS_CALLBACK_PROTOCOL_INIT:
		vhd = lws_protocol_vh_priv_zalloc(lws_get_vhost(wsi),
				lws_get_protocol(wsi),
				sizeof(struct per_vhost_data__iface_stats));
		vhd->context = lws_get_context(wsi);
		vhd->protocol = lws_get_protocol(wsi);
		vhd->vhost = lws_get_vhost(wsi);
		uv_timer_init(lws_uv_getloop(vhd->context, 0),
			      &vhd->timeout_watcher);
		uv_timer_start(&vhd->timeout_watcher,
			       uv_timeout_cb_iface_stats, 1000, 1000);
		lwsl_notice("netc_netstat: protocol init\n");
		break;

	case LWS_CALLBACK_PROTOCOL_DESTROY:
		if (!vhd)
			break;
		uv_timer_stop(&vhd->timeout_watcher);
		lwsl_notice("netc_netstat: protocol destroyed\n");
		break;

	case LWS_CALLBACK_ESTABLISHED:
		lwsl_notice("netc_netstat: callback established\n");
		pss->iface_name[0] = '\0';
		pss->rdb_paths[0] = '\0';
		pss->contents_paths[0] = '\0';
		pss->mode = 0;
		break;

	case LWS_CALLBACK_CLOSED:
		if (pss) {
			lwsl_notice("netc_netstat: callback closed %s\n", pss->iface_name);
			pss->iface_name[0] = 0;
			ds_free(&pss->store);
		}
		break;

	case LWS_CALLBACK_SERVER_WRITEABLE:
		if (pss && pss->iface_name[0] && (pss->mode & MODE_IFACE)) {
			if (send_data(wsi, pss, SEND_IFACE, pss->iface_name) < 0)
				return -1;
		}
		else if (pss && pss->rdb_paths[0] && (pss->mode & MODE_RDB)) {
			if (send_data(wsi, pss, SEND_RDB, pss->rdb_paths) < 0)
				return -1;
		}
		else if (pss && pss->contents_paths[0] && (pss->mode & MODE_CONTENTS)) {
			if (send_data(wsi, pss, SEND_CONTENTS, pss->contents_paths) < 0)
				return -1;
		}
		break;

	case LWS_CALLBACK_RECEIVE:
		if (len < 3)
			break;
		p = (char *)in;
		if (strncmp(p, "CLOSE", 5) == 0) {
			lwsl_notice("netc_netstat: closing as requested\n");
			lws_close_reason(wsi, LWS_CLOSE_STATUS_GOINGAWAY,
					 (unsigned char *)"seeya", 5);
			return -1;
		} else if (strncmp(p, "LIST", 4) == 0) {
			if (send_data(wsi, pss, SEND_LIST, NULL) < 0)
				return -1;
		} else if (strncmp(p, "IFACE", 5) == 0) {
			for (n = 5; n < len; n++) {
				if (p[n] != ' ')
					break;
			}
			if (n < len && p[n] != '\0' && len - n < sizeof(pss->iface_name) ) {
				strncpy(pss->iface_name, p+n, len - n);
				pss->iface_name[len - n] = '\0';
				lwsl_notice("netc_netstat: interface %s stats requested\n", pss->iface_name);
				pss->mode |= MODE_IFACE;
			} else {
				pss->mode &= ~MODE_IFACE;
			}
		} else if (strncmp(p, "RDB", 3) == 0) {
			for (n = 4; n < len; n++) {
				if (p[n] != ' ')
					break;
			}
			if (n < len && p[n] != '\0' && len - n < sizeof(pss->rdb_paths) ) {
				strncpy(pss->rdb_paths, p+n, len - n);
				pss->rdb_paths[len - n] = '\0';
				lwsl_notice("netc_netstat: rdb variables %s requested\n", pss->rdb_paths);
				pss->mode |= MODE_RDB;
			} else {
				pss->mode &= ~MODE_RDB;
			}
		} else if (strncmp(p, "CONTENTS", 8) == 0) {
			for (n = 9; n < len; n++) {
				if (p[n] != ' ')
					break;
			}
			if (n < len && p[n] != '\0' && len - n < sizeof(pss->contents_paths) ) {
				strncpy(pss->contents_paths, p+n, len - n);
				pss->contents_paths[len - n] = '\0';
				lwsl_notice("netc_netstat: directory file contents as variables %s requested\n", pss->contents_paths);
				pss->mode |= MODE_CONTENTS;
			} else {
				pss->mode &= ~MODE_CONTENTS;
			}
		} else if (strncmp(p, "LDIR ", 5) == 0) {
			for (n = 5; n < len; n++) {
				if (p[n] != ' ')
					break;
			}
			if (n < len && p[n] != '\0' && len - n < sizeof(paths) ) {
				strncpy(paths, p+n, len - n);
				paths[len - n] = '\0';
				lwsl_notice("netc_netstat: directory file names %s requested\n", paths);
				if (send_data(wsi, pss, SEND_LDIR, paths) < 0)
					return -1;
			} else {
				lwsl_err("ERROR paths required\n");
				return -1;
			}
		} else if (strncmp(p, "LLPING ", 7) == 0) {
			for (n = 7; n < len; n++) {
				if (p[n] != ' ')
					break;
			}
			if (n < len && p[n] != '\0' && len - n < sizeof(paths) ) {
				strncpy(paths, p+n, len - n);
				paths[len - n] = '\0';
				lwsl_notice("netc_netstat: ping ip address %s requested\n", paths);
				if (send_data(wsi, pss, SEND_LLPING, paths) < 0)
					return -1;
			} else {
				lwsl_err("ERROR IP address required\n");
				return -1;
			}
		} else if (strncmp(p, "TRAFFIC ", 8) == 0) {
			for (n = 8; n < len; n++) {
				if (p[n] != ' ')
					break;
			}
			if (n < len && p[n] != '\0' && len - n < sizeof(paths) ) {
				strncpy(paths, p+n, len - n);
				paths[len - n] = '\0';
				lwsl_notice("netc_netstat: traffic pararameters %s requested\n", paths);
				if (send_data(wsi, pss, SEND_TRAFFIC, paths) < 0)
					return -1;
			} else {
				lwsl_err("ERROR traffic pararameters required\n");
				return -1;
			}
		}

		break;

	default:
		break;
	}

	return 0;
}

static const struct lws_protocols protocols[] = {
	{
		"netc_netstat",
		callback_iface_stats,
		sizeof(struct per_session_data__iface_stats),
		1024, /* rx buf size must be >= permessage-deflate rx size */
	},
};

LWS_VISIBLE int
init_netc_netstat(struct lws_context *context,
			     struct lws_plugin_capability *c)
{
	if (c->api_magic != LWS_PLUGIN_API_MAGIC) {
		lwsl_err("Plugin API %d, library API %d", LWS_PLUGIN_API_MAGIC,
			 c->api_magic);
		return 1;
	}

	lwsl_notice("netc_netstat: init_netc_netstat\n");
	c->protocols = protocols;
	c->count_protocols = ARRAY_SIZE(protocols);
	c->extensions = NULL;
	c->count_extensions = 0;

	return 0;
}

LWS_VISIBLE int
destroy_netc_netstat(struct lws_context *context)
{
	return 0;
}
