#ifndef _LUAEVENT_H
#define _LUAEVENT_H

int li_event_bootstrap_set();
int li_event_bootstrap_unset();
int li_event_bootstrap_get();

int li_event_deleteAll();
int li_event_add(const char *event);
int li_event_getAll(int (*callback)(const char *event));

int li_event_informComplete(int result);

int li_event_reboot();
int li_event_factoryReset();

int li_event_tick();

#endif
