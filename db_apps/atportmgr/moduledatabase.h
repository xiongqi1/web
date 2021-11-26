#ifndef __MODULEDATABASE_H__
#define __MODULEDATABASE_H__

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define STRCONVERT_TABLE_NAME(tableName)		_strConvTable_##tableName
#define STRCONVERT_FUNCTION_NAME(tableName) _strConvFunc_##tableName

#define __countOf(x)			(sizeof(x)/sizeof((x)[0]))

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define STRCONVERT_TABLE_BEGIN(tableName) \
	static const char* STRCONVERT_TABLE_NAME(tableName)[] =

#define STRCONVERT_TABLE_END(tableName) \
	;\
	int STRCONVERT_FUNCTION_NAME(tableName)(const char* szDbVarName, int iIdx, const char* szValue, char* pBuf, int cbBuf)\
	{\
		int iValue=atoi(szValue);\
		\
		if(!(iValue < __countOf(STRCONVERT_TABLE_NAME(tableName))))\
			return -1;\
		\
		strcpy(pBuf,STRCONVERT_TABLE_NAME(tableName)[iValue]);\
		return 0;\
	}

////////////////////////////////////////////////////////////////////////////////
typedef enum
{
	callback_entry_type_permanent,
	callback_entry_type_deleteonsuccess,
	callback_entry_type_deleteonfailure,
	callback_entry_type_deleteonfinish
} callback_entry_type;

////////////////////////////////////////////////////////////////////////////////
typedef struct
{
	const char* szDbVarName;
	int iIdx;
	callback_entry_type entType;
} dbquery;


////////////////////////////////////////////////////////////////////////////////
typedef struct
{
	const char* szModuleName;

	unsigned short wVendor;
	unsigned short wProdId;
	unsigned short wRevMj;
	unsigned short wRevMn;

	unsigned short wTotalLength;

	const char* szAtPort;
	const char* szPppPort;

	const char* szLastPort;

} devinfo;

////////////////////////////////////////////////////////////////////////////////

typedef int (attranstbl_callback)(const char* szDbVarName, int iIdx, const char* szValue, char* pBuf, int cbBuf);

////////////////////////////////////////////////////////////////////////////////
typedef struct
{
	const char* szDbVarName;										// database variable name

	const char* szGetAt;												// GET - at command to get the database varialbe from module
	const char* szGetAnsRegTokenEx;							// GET - token regular express to get the database variable from at command answer
	attranstbl_callback* lpfnGetPostProcess;		// GET - function pointer that processes the token before putting in database
	const char* szGetFormat;										// GET - database variable format in database

} attranstbl;

////////////////////////////////////////////////////////////////////////////////
typedef enum
{
	cmdchanger_module_type_MC8780,
	cmdchanger_module_type_Generic,
} cmdchanger_module_type;


////////////////////////////////////////////////////////////////////////////////
typedef struct
{
	const char* szModuleName;

	const attranstbl* pAtTransTbl;
	cmdchanger_module_type moduleType;

} moduleinfo;

////////////////////////////////////////////////////////////////////////////////

extern const dbquery dbQueries[];
extern const devinfo devInfoTbl[];
extern const devinfo genericDevInfo;
extern const attranstbl sierraAtCommandTable[];
extern const moduleinfo moduleInfoTbl[];

#endif
