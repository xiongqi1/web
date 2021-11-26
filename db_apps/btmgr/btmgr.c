
#include <syslog.h>

#include "btmgr_priv.h"
#include "bluez_support.h"
#include "daemon.h"

/*
 * bluez support ready call back. Invoked when bluez is up and ready.
 */
static void
bluez_ready (void *user_data)
{
    /*
     * The RPC is started only after bluez is ready as the server is
     * dependent on bluez being up.
     */
    int rval = btmgr_rpc_server_init();
    if (rval) {
        errp("Failed to init btmgr rpc server");
        bz_stop();
    }
}

int
main (int argc, char *argv[])
{
    int rval;
    bz_client_t bz_client = { NULL, bluez_ready, NULL, NULL };

    daemon_init("btmgr", NULL, 0, DAEMON_LOG_LEVEL);

    rval = bz_init(&bz_client);
    if (rval) {
        errp("Failed to init bluez support");
        daemon_fini();
    }

    bz_run();

    btmgr_rpc_server_destroy();
    daemon_fini();

    return 0;
}
