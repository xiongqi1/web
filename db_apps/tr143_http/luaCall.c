#define _GNU_SOURCE

#include <stdlib.h>
#include <stdarg.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include <arpa/inet.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "log.h"

#include "rdb_comms.h"
#include "luaCall.h"
#include "utils.h"

#define MINSTACK 16


#define LUA_IF_PARAMS		"if_params"
#define LUA_PROTOCOL_NAME 	"protocolname"
#define LUA_ACTION_NAME		"action"

#define LUA_ACTION_START	"start"
#define LUA_ACTION_STOP		"stop"
#define LUA_ACTION_CLEAR	"clear"

#define LUA_IF_NAME			"interfacename"
#define LUA_SERVER_ADDRESS	"serveraddress"
#define LUA_DEBUG			"debug"
#define LUA_IF_BUILD_METHOD	"ifbuildmethod"

extern void install_signal_handler(void);

void li_stackDump (lua_State *LUA) {
	int i;
	int top = lua_gettop(LUA);
	NTCLOG_DEBUG("Stack %p (%d): begin\n", LUA, top);
	for (i = 1; i <= top; i++) {  /* repeat for each level */
		int t = lua_type(LUA, i);
		switch (t) {
			case LUA_TSTRING:  /* strings */
				NTCLOG_DEBUG("\t%d string:'%s'\n", i, lua_tostring(LUA, i));
				break;
			case LUA_TBOOLEAN:  /* booleans */
				NTCLOG_DEBUG("\t%d bool:%s\n", i, (lua_toboolean(LUA, i))?("true"):("false") );
				break;
			case LUA_TNUMBER:  /* numbers */
				NTCLOG_DEBUG("\t%d number:%g\n", i, lua_tonumber(LUA, i));
				break;
			default:  /* other values */
				NTCLOG_DEBUG("\t%d %s\n", i, lua_typename(LUA, t));
				break;
		}
	}
	NTCLOG_DEBUG("Stack %p (%d): end\n", LUA, top);
}


/// call lua script to setup if
///$ >=0 --if index
/// <0 	--error code
int lua_ifup(const char* ifup_script, TConnectSession* pSession,  unsigned int rdb_var_if_flag)
{
	int retValue;
    lua_State *LUA;
	const char *p;
    const TRdbNameList* pNameList;

    /*
     * All Lua contexts are held in this structure. We work with it almost
     * all the time.
     */
    LUA = luaL_newstate();

    luaL_openlibs(LUA); /* Load Lua libraries */

    /* Load the file containing the script we are going to run */
    retValue= luaL_loadfile(LUA, ifup_script);
    if (retValue) {
        /* If something went wrong, error message is at the top of */
        /* the stack */
        NTCLOG_ERR("Couldn't load file: %s\n", lua_tostring(LUA, -1));
        return -1;
    }

    /*
     * Ok, now here we go: We pass data to the lua script on the stack.
     * That is, we first have to prepare Lua's virtual stack the way we
     * want the script to receive it, then ask Lua to run it.
     */
    lua_newtable(LUA);    /* We will pass a table */

    /*
     * To put values into the table, we first push the index, then the
     * value, and then call lua_rawset() with the index of the table in the
     * stack. Let's see why it's -3: In Lua, the value -1 always refers to
     * the top of the stack. When you create the table with lua_newtable(),
     * the table gets pushed into the top of the stack. When you push the
     * index and then the cell value, the stack looks like:
     *
     * <- [stack bottom] -- table, index, value [top]
     *
     * So the -1 will refer to the cell value, thus -3 is used to refer to
     * the table itself. Note that lua_rawset() pops the two last elements
     * of the stack, so that after it has been called, the table is at the
     * top of the stack.
     */
     // build table from RDB variable
	pNameList= g_rdbNameList;
    while(pNameList->szName)
    {
        if((pNameList->attribute & rdb_var_if_flag))
        {
			char buf[120];
			retValue  = rdb_get_i_str(pNameList->szName, pSession->m_parameters->m_session_id, buf, 119);
            //NTCLOG_DEBUG("%s = %s",pNameList->szName, buf);
			if(retValue ==0)
			{
				// use last field as vaiable name
				p = strrchr(pNameList->szName, '.');
				if(p ==NULL)
				{
					p= pNameList->szName;
				}

				lua_pushstring(LUA, p+1);
				lua_pushstring(LUA, buf);
				lua_rawset(LUA, -3);      /* Stores the pair in the table */
			}


        }
//        else if(pNameList->attribute & rdb_var_prefix)
//        {
//			lua_pushstring(LUA, LUA_RDB_PREFIX);
//			lua_pushstring(LUA, pNameList->szName);
//			lua_rawset(LUA, -3);      /* Stores the pair in the table */
//
//        }

        pNameList++;
    }


	lua_pushstring(LUA, LUA_PROTOCOL_NAME);
	lua_pushstring(LUA, pSession->m_protocol_name);
	lua_rawset(LUA, -3);      /* Stores the pair in the table */

	if(pSession->m_parameters->m_if_build_method ==5)
	{
		sprintf(pSession->m_if_name, "%s_%d", pSession->m_protocol_name, pSession->m_if_cos);
	}
	else
	{
		sprintf(pSession->m_if_name, "%s_%d", pSession->m_protocol_name, pSession->m_parameters->m_session_id);
	}
	lua_pushstring(LUA, LUA_IF_NAME);
	lua_pushstring(LUA, pSession->m_if_name);
	lua_rawset(LUA, -3);      /* Stores the pair in the table */

	lua_pushstring(LUA, LUA_IF_BUILD_METHOD);
	lua_pushinteger(LUA, pSession->m_parameters->m_if_build_method);
	lua_rawset(LUA, -3);      /* Stores the pair in the table */



	lua_pushstring(LUA, LUA_ACTION_NAME);
	lua_pushstring(LUA, LUA_ACTION_START);
	lua_rawset(LUA, -3);      /* Stores the pair in the table */


	lua_pushstring(LUA, LUA_SERVER_ADDRESS);
	lua_pushstring(LUA, inet_ntoa(*((struct in_addr*)&pSession->m_ServerAddress)));
	lua_rawset(LUA, -3);      /* Stores the pair in the table */

	lua_pushstring(LUA, LUA_DEBUG);
	lua_pushinteger(LUA, pSession->m_parameters->m_script_debug);
	lua_rawset(LUA, -3);      /* Stores the pair in the table */

    /* By what name is the script going to reference our table? */
    lua_setglobal(LUA, LUA_IF_PARAMS);

    /* Ask Lua to run our little script */
    retValue= lua_pcall(LUA, 0, LUA_MULTRET, 0);
    if (retValue) {
        NTCLOG_ERR("Failed to run script: %s\n", lua_tostring(LUA, -1));
        retValue = -1;
        goto end;

    }

    /* Get the returned value at the top of the stack (index -1) */
    //-5: is tmp:, -4: smartedget_ifname: -3: ifname, -2:rx, -1:tx
    p = lua_tostring(LUA, -3);
	//NTCLOG_DEBUG("Script returned: %s", p);
    retValue =-1;
    if(p)
    {
#if 0
    	pSession->m_if_tx  = lua_tointeger(LUA, -1);
		pSession->m_if_rx  = lua_tointeger(LUA, -2);
#endif
    	pSession->m_if_istemp = lua_tointeger(LUA, -5);
		strncpy(pSession->m_if_name, p, MAX_IFNAME_LEN);
		pSession->m_if_name[MAX_IFNAME_LEN] =0;
		p = lua_tostring(LUA, -4);
		if(p)
		{
			strncpy(pSession->m_sedge_if_name, p, MAX_IFNAME_LEN);
		}
		
		pSession->m_sedge_if_name[MAX_IFNAME_LEN] =0;
	
		get_if_rx_tx(pSession->m_if_name, &pSession->m_if_rx, &pSession->m_if_tx,&pSession->m_if_rx_pkts, &pSession->m_if_tx_pkts);

		get_if_rx_tx(pSession->m_sedge_if_name, &pSession->m_sedge_if_rx, &pSession->m_sedge_if_tx,&pSession->m_sedge_if_rx_pkts, &pSession->m_sedge_if_tx_pkts);

	    NTCLOG_DEBUG("Script returned: isTmp=%d, sedge_if_name=%s,if_name=%s, rx=%lld, tx=%lld",pSession->m_if_istemp,
					pSession->m_if_name,  pSession->m_sedge_if_name, pSession->m_if_rx, pSession->m_if_tx );

		if (pSession->m_parameters->m_if_build_method == IF_BUILD_EXIST_DEL)
		{
			// always delete this interface when exit
			pSession->m_if_istemp =1;
		}
		retValue =0;
    }

    lua_pop(LUA, 5);  /* Take the returned value out of the stack */
end:
    lua_close(LUA);   /* Cya, Lua */
    install_signal_handler(); //signal handler is overwritten by lua callback, re-install it

    return retValue ;
}

/// call lua script to drop if
///$ >=0 --if index
/// <0 	--error code
int lua_ifdown(const char* ifdown_script, TConnectSession*pSession, int if_op)
{
	int retValue;
    lua_State *LUA;

	if(if_op ==IF_DOWN_NONE) return 0;
    /*
     * All Lua contexts are held in this structure. We work with it almost
     * all the time.
     */
    LUA = luaL_newstate();

    luaL_openlibs(LUA); /* Load Lua libraries */

    /* Load the file containing the script we are going to run */
    retValue= luaL_loadfile(LUA, ifdown_script);
    if (retValue) {
        /* If something went wrong, error message is at the top of */
        /* the stack */
        NTCLOG_ERR("Couldn't load file: %s\n", lua_tostring(LUA, -1));
        return -1;
    }

    /*
     * Ok, now here we go: We pass data to the lua script on the stack.
     * That is, we first have to prepare Lua's virtual stack the way we
     * want the script to receive it, then ask Lua to run it.
     */
    lua_newtable(LUA);    /* We will pass a table */

    /*
     * To put values into the table, we first push the index, then the
     * value, and then call lua_rawset() with the index of the table in the
     * stack. Let's see why it's -3: In Lua, the value -1 always refers to
     * the top of the stack. When you create the table with lua_newtable(),
     * the table gets pushed into the top of the stack. When you push the
     * index and then the cell value, the stack looks like:
     *
     * <- [stack bottom] -- table, index, value [top]
     *
     * So the -1 will refer to the cell value, thus -3 is used to refer to
     * the table itself. Note that lua_rawset() pops the two last elements
     * of the stack, so that after it has been called, the table is at the
     * top of the stack.
     */

	lua_pushstring(LUA, LUA_IF_NAME);
	lua_pushstring(LUA, pSession->m_if_name);
	lua_rawset(LUA, -3);      /* Stores the pair in the table */


	lua_pushstring(LUA, LUA_PROTOCOL_NAME);
	lua_pushstring(LUA, pSession->m_protocol_name);
	lua_rawset(LUA, -3);      /* Stores the pair in the table */

	// if it is not a temp interface , do not stop it
	
	if(if_op ==IF_DOWN_CLEAR)
	{
		lua_pushstring(LUA, LUA_ACTION_NAME);
		lua_pushstring(LUA, LUA_ACTION_CLEAR);
		lua_rawset(LUA, -3);      /* Stores the pair in the table */
	}
	else if( if_op ==IF_DOWN_STOP)
	{
		lua_pushstring(LUA, LUA_ACTION_NAME);
		lua_pushstring(LUA, LUA_ACTION_STOP);
		lua_rawset(LUA, -3);      /* Stores the pair in the table */
	}

	lua_pushstring(LUA, LUA_DEBUG);
	lua_pushinteger(LUA, pSession->m_parameters->m_script_debug);
	lua_rawset(LUA, -3);      /* Stores the pair in the table */

    /* By what name is the script going to reference our table? */
    lua_setglobal(LUA, LUA_IF_PARAMS);


    /* Ask Lua to run our little script */
    retValue= lua_pcall(LUA, 0, LUA_MULTRET, 0);
    if (retValue) {
        NTCLOG_ERR("Failed to run script: %s\n", lua_tostring(LUA, -1));

        retValue =-1;
        goto end;
    }

    /* Get the returned value at the top of the stack (index -1) */
    /* Get the returned value at the top of the stack (index -1) */
    // -2:rx, -1:tx

   // NTCLOG_DEBUG("Script returned: rx=%lld, tx=%lld", pSession->m_if_rx,pSession->m_if_tx);
    lua_pop(LUA, 2);  /* Take the returned value out of the stack */

end:
    lua_close(LUA);   /* Cya, Lua */

    install_signal_handler(); //signal handler is overwritten by lua callback, re-install it

    return retValue;
}
