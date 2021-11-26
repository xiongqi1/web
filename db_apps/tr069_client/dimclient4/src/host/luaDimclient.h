#ifndef _LUADIMCLIENT_H
#define _LUADIMCLIENT_H

#include <lua.h>

#define DIMCLIENT_GLOBAL "dimclient"
#define DIMCLIENT_THREADS "_threads"
#define DIMCLIENT_CALLBACKS "callbacks"
#define DIMCLIENT_EVENT "event"
#define DIMCLIENT_TRANSFER "transfer"
#define DIMCLIENT_PARAMETER "parameter"

int li_dimclient_init(lua_State *thread);
void li_dimclient_find_function(lua_State *thread, char *containerName, char *functionName);

#endif
