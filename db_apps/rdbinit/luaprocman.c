#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MODNAME "procman"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <nc/nctype.h>
#include <nc/ncvector.h>

#include "global.h"
#include "lrt.h"
#include "luaprocman.h"

struct nctype *strType = NULL;

// pid = luaprocman.fork(args[], env[], stdin, stdout, stderr)
static int lpm_fork(lua_State *L) {
	int i, ret, argc;
	const char *file;
	char **argv, **envp;
	struct ncvec *args, *env;
	sigset_t newMask, oldMask;
	int fdout = -1, fdin = -1, fderr = -1;

	DEBUG(lrt_stackDump(L, stderr, "start of fork()"));

	luaL_checktype(L, 1, LUA_TTABLE); // args
	luaL_checktype(L, 2, LUA_TTABLE); // env

	// stdin
	if(!lua_isnoneornil(L, 3)) {
		file = luaL_checkstring(L, 3);
		if((fdin = open(file, O_RDONLY)) < 0) {
			return luaL_error(L, "open(%s): %d %d %s", file, fdin, errno, strerror(errno));
		}
	}
	// stdout
	if(!lua_isnoneornil(L, 4)) {
		file = luaL_checkstring(L, 4);
		if((fdout = open(file, O_WRONLY | O_CREAT | O_TRUNC)) < 0) {
			return luaL_error(L, "open(%s): %d %d %s", file, fdout, errno, strerror(errno));
		}
	}
	// stderr
	if(!lua_isnoneornil(L, 5)) {
		file = luaL_checkstring(L, 5);
		if((fdout = open(file, O_WRONLY | O_CREAT | O_TRUNC | O_SYNC)) < 0) {
			return luaL_error(L, "open(%s): %d %d %s", file, fderr, errno, strerror(errno));
		}
	}

	
	// generate argv
	args = NULL;
	if(ncvector_new(&args, strType)) {
		return luaL_error(L, "Unable to allocate args vector.");
	}
	argc = lua_objlen(L, 1) + 1; // grr lua 1s-based arrays
	for(i = 0; i < argc; i++) {
		char *arg;
		lua_rawgeti(L, 1, i);
		arg = (char *)lua_tostring(L, -1);
		if(ncvector_push(args, arg)) {
			ncvector_del(&args, NCFREE_INSTANCE);
			return luaL_error(L, "Unable to push argument into args vector.");
		}
		lua_pop(L, 1);
	}
	ncvector_data(args, &argv);

	// generate envp
	env = NULL;
	if(ncvector_new(&env, strType)) {
		ncvector_del(&args, NCFREE_INSTANCE);
		return luaL_error(L, "Unable to allocate env vector.");
	}
	lua_pushnil(L);
	DEBUG(lrt_stackDump(L, stderr, "start of env loop"));
	while(lua_next(L, -2) != 0) {
		lua_pushvalue(L, -2);
		const char *val = lua_tostring(L, -2);
		const char *key = lua_tostring(L, -1);
		int len = strlen(key) + strlen(val) + 2;
		char *buf = malloc(len);
		DEBUG(lrt_stackDump(L, stderr, "top of env loop"));
		if(!buf) {
			ncvector_del(&args, NCFREE_INSTANCE);
			ncvector_del(&env, NCFREE_RECURSIVE);
			return luaL_error(L, "Unable to allocate %d bytes.", len);
		}
		if(snprintf(buf, len, "%s=%s", key, val) >= len) {
			free(buf);
			ncvector_del(&args, NCFREE_INSTANCE);
			ncvector_del(&env, NCFREE_RECURSIVE);
			return luaL_error(L, "Unexpected string truncation!");
		}
		if(ncvector_push(env, buf)) {
			free(buf);
			ncvector_del(&args, NCFREE_INSTANCE);
			ncvector_del(&env, NCFREE_RECURSIVE);
			return luaL_error(L, "Unable to push environment variable into env vector.");
		}
		lua_pop(L, 2);
		DEBUG(lrt_stackDump(L, stderr, "bottom of env loop"));
	}
	ncvector_data(env, &envp);

	DEBUG(fprintf(stderr, MODNAME ".fork({'%s', ...}, {'%s', ...})\n", argv[0], envp[0]));
	DEBUG(ncvector_dump(args, stderr));
	DEBUG(ncvector_dump(env, stderr));

	// block all signals for a moment - not sure we need to actually do this
	sigfillset(&newMask);
	if((ret = sigprocmask(SIG_BLOCK, &newMask, &oldMask))) {
		ncvector_del(&args, NCFREE_INSTANCE);
		ncvector_del(&env, NCFREE_RECURSIVE);
		return luaL_error(L, "sigprocmask(): %d: %s.", ret, strerror(errno));
	}

	// fork() & exec()!
	ret = FORK();
	if(ret < 0) {
		// error
		ncvector_del(&args, NCFREE_INSTANCE);
		ncvector_del(&env, NCFREE_RECURSIVE);
		if(fdin > -1) close(fdin);
		if(fdout > -1) close(fdout);
		if(fderr > -1) close(fderr);
		if(sigprocmask(SIG_SETMASK, &oldMask, NULL)) {
			return luaL_error(L, "FORK(): %d: %s and sigprocmask() failed to restore signal mask!", ret, strerror(errno));
		}
		return luaL_error(L, "fork(): %d: %s.", ret, strerror(errno));
	} else if(ret == 0) {
		// child
		//DEBUG(fprintf(stderr, "in child, execve()ing %p %p %p\n", argv, envp, envp[0]));
		if(fdin > -1) {
			if(dup2(fdin, STDIN_FILENO) < 0) {
				perror("dup2(stdin)");
				_exit(EXIT_FAILURE);
			}
		}
		if(fdout > -1) {
			if(dup2(fdout, STDOUT_FILENO) < 0) {
				perror("dup2(stdout)");
				_exit(EXIT_FAILURE);
			}
		}
		if(fderr > -1) {
			if(dup2(fderr, STDERR_FILENO) < 0) {
				perror("dup2(stderr)");
				_exit(EXIT_FAILURE);
			}
		}
		ret = execve(argv[0], argv, envp);
		perror("execve()");
		_exit(EXIT_FAILURE);
	} else {
		// parent
		if(fdin > -1) close(fdin);
		if(fdout > -1) close(fdout);
		if(fderr > -1) close(fderr);
		if(sigprocmask(SIG_SETMASK, &oldMask, NULL)) {
			return luaL_error(L, "sigprocmask(): %d: %s - failed to restore signal mask!", ret, strerror(errno));
		}
		DEBUG(fprintf(stderr, MODNAME ".fork(): pid = %d\n", ret));
		ncvector_del(&args, NCFREE_INSTANCE);
		ncvector_del(&env, NCFREE_RECURSIVE);
		lua_pushinteger(L, ret);
	}

	DEBUG(lrt_stackDump(L, stderr, "end of fork()"));

	return 1;
}

// luaprocman.kill(pid, signal)
static int lpm_kill(lua_State *L) {
	int pid = luaL_checkint(L, 1);
	int signal = luaL_checkint(L, 2);
	DEBUG(fprintf(stderr, MODNAME ".kill(%d, %d)\n", pid, signal));
	if(kill(pid, signal)) {
		return luaL_error(L, "kill(%d, %d): %s.", pid, signal, strerror(errno));
	}
	return 0;
}

// luaprocman.wait(pid)
static int lpm_wait(lua_State *L) {
	int ret, status, nonblock = 1, pid = luaL_checkint(L, 1);
	if(lua_gettop(L) > 1) {
		luaL_checktype(L, 2, LUA_TBOOLEAN);
		nonblock = lua_toboolean(L, 2);
	}

	DEBUG(fprintf(stderr, MODNAME ".wait(%d)\n", pid));
	DEBUG(lrt_stackDump(L, stderr, "start of wait()"));

	lua_newtable(L);

	ret = waitpid(pid, &status, WUNTRACED | WCONTINUED | ((nonblock)?(WNOHANG):(0)));
	if(ret < 0) {
		return luaL_error(L, "waitpid(%d): %s.", pid, strerror(errno));
	} else if(ret == 0) {
		return 0;
	}

	DEBUG(lrt_stackDump(L, stderr, "after wait().waitpid"));
	
	lua_pushinteger(L, ret);
	lua_setfield(L, -2, "pid");
	if(WIFEXITED(status)) {
		lua_pushstring(L, "exited");
		lua_setfield(L, -2, "status");
		lua_pushinteger(L, WEXITSTATUS(status));
		lua_setfield(L, -2, "exit");
	} else if(WIFSIGNALED(status)) {
		lua_pushstring(L, "signalled");
		lua_setfield(L, -2, "status");
		lua_pushinteger(L, WTERMSIG(status));
		lua_setfield(L, -2, "signal");
		lua_pushstring(L, strsignal(WTERMSIG(status)));
		lua_setfield(L, -2, "signalName");
		if(WCOREDUMP(status)) {
			lua_pushboolean(L, 1);
			lua_setfield(L, -2, "core");
		}
	} else if(WIFSTOPPED(status)) {
		lua_pushstring(L, "stopped");
		lua_setfield(L, -2, "status");
		lua_pushinteger(L, WSTOPSIG(status));
		lua_setfield(L, -2, "signal");
		lua_pushstring(L, strsignal(WSTOPSIG(status)));
		lua_setfield(L, -2, "signalName");
#ifdef WIFCONTINUED
	} else if(WIFCONTINUED(status)) {
		lua_pushstring(L, "continued");
		lua_setfield(L, -2, "status");
#endif
	}

	DEBUG(lrt_stackDump(L, stderr, "end of wait()"));

	return 1;
}

static const struct luaL_reg luaprocman_funcs[] = {
	{ "fork",		lpm_fork },
	{ "kill",		lpm_kill },
	{ "wait",		lpm_wait },
	{ NULL, NULL }
};

int luaprocman_init(lua_State *L) {
	nctype_new(&strType, NCTYPE_STRING);

	// register our module functions
	luaL_register(L, MODNAME, luaprocman_funcs);

	// leave stack clean
	lua_pop(L, 1);

	return 0;
}
