#include <stdio.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "global.h"
#include "lrt.h"
#include "luaprocman.h"

char *scriptFile = NULL;

static void doIt() {
	lua_State *L;
	
	/* create lua context */
	L = lua_open();
	luaL_openlibs(L);
	lrt_init(L);

	/* register process manager library */
	luaprocman_init(L);

	/* run the script */
	if(luaL_dofile(L, scriptFile)) {
		lrt_die(L, "Lua error: %s\n", lua_tostring(L, -1));
	}

	/* clean-up */
	lua_close(L);
}

int main(int argc, char **argv) {
	if(argc == 1) {
		scriptFile = DEFAULT_SCRIPT;
	} else if(argc == 2) {
		scriptFile = argv[1];
	} else {
		fprintf(stderr, "usage: %s <script>\n", argv[0]);
		return 1;
	}

	doIt();

	return 0;
}
