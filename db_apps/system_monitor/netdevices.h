#ifndef _NETDEVICES_H
#define _NETDEVICES_H

/* Call once at start */
int init_netdevices(void);

/*
Call on network subsystem hotplug events.
*/
int poll_netdevices(const char *interface, int add);

#endif /* _NETDEVICES_H */
