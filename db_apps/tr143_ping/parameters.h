#ifndef __PARAMETERS_H
#define __PARAMETERS_H


#define IF_BUILD_CREATE_NEW			0 //	"\t   	0 -- create new interface(delete all conflicted interface)\n"
#define IF_BUILD_SAME_IF_NAME		1 //	"\t   	1 -- use same name of interface\n"
#define IF_BUILD_SIMILAR_IF_NAME	2 //	"\t   	2 -- use similar name of interface\n"
#define IF_BUILD_EXIST				3 //	"\t   	3 -- use same address of interface\n"
#define IF_BUILD_EXIST_DEL			4 //	"\t   	4 -- use same address of interface, delete at exit\n"
#define IF_BUILD_EXIST_CLEAR		5 //	"\t   	5 -- use same address of interface, clear at exit\n"

#define IF_BUILD_MAX				6

typedef struct TParameters
{
	int 			m_running;
	int             m_force_rdb_read;         // force read rdb
	// from command line parameters
	//int 			m_verbosity;	// verbostiy level
	int 			m_script_debug;	// enable script debug

	char			m_stats_name[120];

	const char*		m_ifup_script;	// the ifup script for ping client
	const char*		m_ifdown_script;// the ifdown script for ping client
	unsigned int	m_if_build_method; //IF_BUILD_XXX

	int 			m_cmd_line_mode;		// whether it runs once
	unsigned int 	m_session_id;
	int 			m_remove_rdb;
	int 			m_console_test_mode;
	int 			m_dump_raw_data;
	int 			m_if_ops;

} TParameters;

#endif
