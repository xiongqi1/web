/* ----------------------------------------------------------------------------
RDB interface program

Lee Huang<leeh@netcomm.com.au>

*/

#ifndef __RDB_COMMS_H
#define __RDB_COMMS_H

#include "rdb_ops.h"
#include "rdb_lbm.h"
#include "rdb_lmp.h"
#include "rdb_ltm.h"
#include "rdb_ltr.h"
#include "rdb_mda.h"
#include "rdb_rmp.h"
#include "rdb_tlv.h"
#include "rdb_y1731_mda.h"
#include "rdb_y1731_lmp.h"
#include "rdb_y1731_lmm.h"
#include "rdb_y1731_dmm.h"
#include "rdb_y1731_slm.h"


#include "utils.h"

extern struct rdb_session *g_rdb_session;

#define RDB_DEVNAME		"/dev/cdcs_DD"

#define RDB_VAR_NONE		0
#define RDB_VAR_SET_IF		0x001 // used by IF interface

#define RDB_VAR_SUBCRIBE	0x080 // subscribed
#define RDB_STATIC			0x100
#define RBD_FLAG_PERSIST	0x040
#define RDB_NO_VAR			0x4000 // the rdb name has novariable
#define RDB_NO_REMOVE		0x8000

typedef struct TRdbNameList
{
    char* szName;
    int bcreate; // create if it does not exit
    int attribute;	//RDB_VAR_XXXX
    char*szValue; // default value
} TRdbNameList;

extern const TRdbNameList g_rdbNameList[];

////////////////////////////////////////////////////////////////////////////////
// open rdb, initilize global rdb variables and subscrible

extern int rdb_init_global();
// close rdb, delete global rdb variables
extern void rdb_end_global();
////////////////////////////////////////////////////////////////////////////////
///initilize rdb variables inside of session
/// id -1, 0, >0
/// -1		-- create/delete rdb with RDB_NO_VAR flags
//   0, >0 	-- create/delete rdb without RDB_NO_VAR flags
extern int create_rdb_variables(int id);
extern void delete_rdb_variables(int id);

extern int create_rmp_rdb_variables(int id1, int id2);
extern void delete_rmp_rdb_variables(int id1, int id2);



////////////////////////////////////////////////////////////////////////////////
/// id -1, 0, >0
/// -1		-- subscrible rdb with RDB_NO_VAR flags
//   0, >0 	-- subscrible rdb without RDB_NO_VAR flags
extern int subscribe(int id);

/// basic rdb function
////////////////////////
// check whether RDB exist or not
// 0: -- rdb not exist
// 1  --- rdb exist
extern int rdb_exist(const char *rdb_name);

extern int rdb_get_boolean(const char* rdbname, int *value);
extern int rdb_set_boolean(const char* rdbname, int value);
//////////////////////////////////////////
extern int rdb_get_sint(const char* rdbname, int *value);
extern int rdb_set_int(const char* rdbname, int value);

//////////////////////////////////////////
extern int rdb_get_uint(const char* rdbname, unsigned int *value);
extern int rdb_set_uint(const char* rdbname, unsigned int value);

//////////////////////////////////////////
static inline int rdb_get_str(const char* rdbname, char* value, int len)
{
	return rdb_get(g_rdb_session, rdbname, value, &len);
}
static inline int rdb_set_str(const char* rdbname, const char* value)
{
	return rdb_set_string(g_rdb_session, rdbname, value);
}


//////////////////////////////////////////////
extern int rdb_get_mac(const char* rdbname, unsigned char *mac);
extern int rdb_set_mac(const char* rdbname, const unsigned char* mac);



#define MAX_RDB_NAME_SIZE 128
#define MAX_RDB_VALUE_SIZE 256

/// advanced RDB function with prefix in first '.'
/// example: dot1ag.mda.peermode =>dot1ag.1.mda.peermode, dot1ag.2.mda.peermode,
////////////////////////
/// dot1ag.1.mda.peermode => dot1ag.mda.peermode, return 1, or 0 not(found)
extern int strip_p1(char* buf, const char *orig_name, int max_session);

////////////////////////
extern int rdb_get_p1_boolean(const char* rdbname, int i, int *value);
extern int rdb_set_p1_boolean(const char* rdbname, int i, int value);

//////////////////////////////////////////
extern int rdb_get_p1_int(const char* rdbname, int i, int *value);
extern int rdb_set_p1_int(const char* rdbname, int i, int value);

//////////////////////////////////////////
extern int rdb_get_p1_uint(const char* rdbname, int i, unsigned int *value);
extern int rdb_set_p1_uint(const char* rdbname, int i, unsigned int value);

//////////////////////////////////////////
extern int rdb_get_p1_str(const char* rdbname, int i, char* str, int len);
extern int rdb_set_p1_str(const char* rdbname, int i, const char* str);

extern int rdb_set_p1_2str(const char* rdbname, int i, const char* str1, const char* str2);

//////////////////////////////////////////
extern  int rdb_get_p1_mac(const char* rdbname, int i, unsigned char* mac);
extern  int rdb_set_p1_mac(const char* rdbname, int i, const unsigned char* mac);


/* macro to hide session*/
#define RDB_GET_P1_BOOLEAN(name, var) rdb_get_p1_boolean(name, pSession->m_node, var);


#define RDB_SET_P1_BOOLEAN(name,  var) rdb_set_p1_boolean(name, pSession->m_node, var);

#define RDB_GET_P1_INT(name,  var) rdb_get_p1_int(name, pSession->m_node, var)

#define RDB_SET_P1_INT(name,  var) rdb_set_p1_int(name, pSession->m_node, var);

#define RDB_GET_P1_UINT(name,  var) rdb_get_p1_uint(name, pSession->m_node, var);

#define RDB_SET_P1_UINT(name,  var) rdb_set_p1_uint(name, pSession->m_node, var);

#define RDB_GET_P1_STR(name,  var, len) rdb_get_p1_str(name, pSession->m_node, var, len);

#define RDB_SET_P1_STR(name,  var)  rdb_set_p1_str(name, pSession->m_node, var);

#define RDB_SET_P1_2STR(name,  str1, str2) 	rdb_set_p1_2str( name, pSession->m_node, str1, str2);

#define RDB_GET_P1_MAC(name,  var)  rdb_get_p1_mac(name, pSession->m_node, var);

#define RDB_SET_P1_MAC(name,  var) rdb_set_p1_mac(name, pSession->m_node, var);




/* Macro with error check*/

#define TRY_RDB_GET_P1_BOOLEAN(name, var) err= rdb_get_p1_boolean(name, pSession->m_node, var);\
							if(err) goto lab_err;


#define TRY_RDB_SET_P1_BOOLEAN(name,  var) err= rdb_set_p1_boolean(name, pSession->m_node, var);\
							if(err) goto lab_err;

#define TRY_RDB_GET_P1_INT(name,  var) err= rdb_get_p1_int(name, pSession->m_node, var);\
							if(err) goto lab_err;

#define TRY_RDB_SET_P1_INT(name,  var) err= rdb_set_p1_int(name, pSession->m_node, var);\
							if(err) goto lab_err;


#define TRY_RDB_GET_P1_UINT(name,  var) err= rdb_get_p1_uint(name, pSession->m_node, var);\
							if(err) goto lab_err;


#define TRY_RDB_SET_P1_UINT(name,  var) err= rdb_set_p1_uint(name, pSession->m_node, var);\
							if(err) goto lab_err;

#define TRY_RDB_GET_P1_STR(name,  var, len) err= rdb_get_p1_str(name, pSession->m_node, var, len);\
							if(err) goto lab_err;

#define TRY_RDB_SET_P1_STR(name,  var) if (var) {\
										err= rdb_set_p1_str(name, pSession->m_node, var);\
										if(err) goto lab_err;}


#define TRY_RDB_GET_P1_MAC(name,  var) err= rdb_get_p1_mac(name, pSession->m_node, var);\
							if(err) goto lab_err;


#define TRY_RDB_SET_P1_MAC(name,  var) err= rdb_set_p1_mac(name, pSession->m_node, var);\
							if(err) goto lab_err;


#define RDB_CMP_P1_INT(name,  tmp, var) rdb_name=name;\
							err= rdb_get_p1_int(name, pSession->m_node, &tmp);\
							if(err) goto lab_err;\
							if( tmp != var) goto lab_value_changed;

#define RDB_CMP_P1_UINT(name,  tmp, var) rdb_name=name;\
							err= rdb_get_p1_uint(name, pSession->m_node, &tmp);\
							if(err) goto lab_err;\
							if( tmp != var) goto lab_value_changed;

#define RDB_CMP_P1_STR(name,  tmp, var, len) rdb_name=name;\
							err= rdb_get_p1_str(name, pSession->m_node, tmp, len);\
							if(err) goto lab_err;\
							if( strncmp(tmp, var, len) != 0) goto lab_value_changed;

#define RDB_CMP_P1_MAC(name,  tmp, var) rdb_name=name;\
							err= rdb_get_p1_mac(name, pSession->m_node, tmp);\
							if(err) goto lab_err;\
							if( memcmp(tmp, var, 6) != 0) goto lab_value_changed;




// advanced RDB function with prefix in first '.' and second '.'
/// example: dot1ag.rmp.lastccmmacaddr=>dot1ag.1.rmp.2.lastccmmacaddr

//////////////////////////////////////////
extern int rdb_set_p1_2_boolean(const char* rdbname, int i, int j, int value);

//////////////////////////////////////////
extern int rdb_set_p1_2_str(const char* rdbname, int i, int j,  const char* str);

//////////////////////////////////////////
extern int rdb_get_p1_2_int(const char* rdbname, int i, int j, int *value);
extern int rdb_set_p1_2_int(const char* rdbname, int i, int j, int value);

//////////////////////////////////////////
extern int rdb_set_p1_2_uint(const char* rdbname, int i, int j, unsigned int value);


//////////////////////////////////////////
extern int rdb_set_p1_2_mac(const char* rdbname, int i, int j, const unsigned char* mac);

//////////////////////////////////////////
extern int rdb_get_p1_uint_array(const char* rdbname, int i, unsigned int *a, int maxlen);
extern int rdb_set_p1_uint_array(const char* rdbname, int i, const unsigned int* a, int len);


#define TRY_RDB_SET_P1_2_BOOLEAN(name, j, var) err= rdb_set_p1_2_boolean(name, pSession->m_node, j, var);\
							if(err) goto lab_err;

#define TRY_RDB_SET_P1_2_INT(name, j, var) err= rdb_set_p1_2_int(name, pSession->m_node, j, var);\
							if(err) goto lab_err;

#define TRY_RDB_SET_P1_2_UINT(name, j,  var) err= rdb_set_p1_2_uint(name, pSession->m_node, j, var);\
							if(err) goto lab_err;


#define TRY_RDB_SET_P1_2_MAC(name, j,  var) err= rdb_set_p1_2_mac(name, pSession->m_node, j, var);\
							if(err) goto lab_err;


#define RDB_GET_P1_2_INT(name, j, var) rdb_get_p1_2_int(name, pSession->m_node, j, var);


//////////////////////////////////////////
// advanced RDB function with read a number list
/// example: "1,2,3,4"=>[0]=1,[1]=2,[2]=3,[3]=4
///rdbname -- rdbname, may need a var
///i       -- variable in rdbname, if it is >0
// pIndex	-- index array 
// max_number -- maxium allown index number 
// -1 --- rdb does not exist
// >0 --- number of in pIndex
int rdb_get_index_list(const char* rdbname, int i, int *pIndex, int max_number);

/// example: [0]=1,[1]=2,[2]=3,[3]=4 => "1,2,3,4"
///rdbname -- rdbname, may need a var
///i       -- variable in rdbname, if it is >0
// pIndex	-- index array
// max_number -- maxium allown index number
// -1 --- rdb does not exist
// ==0 --- success
int rdb_set_index_list(const char* rdbname, int i, int *pIndex, int number);

/*
look for avc index for matched its avc.x.avcid
$param avcid -- to search for
$ -1 -- error
$ 0  -- not found
$>0   -- the matched index
*/
int get_avc_index(const char *avcid);
#endif
