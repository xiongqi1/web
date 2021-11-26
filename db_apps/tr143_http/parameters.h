#ifndef __PARAMETERS_H
#define __PARAMETERS_H
//#define CMD_RUN_NONE		0
//#define CMD_RUN_DOWNLOAD	1
//#define CMD_RUN_UPLOAD		2

#define IF_BUILD_CREATE_NEW			0 //	"\t   	0 -- create new interface(delete all conflicted interface)\n"
#define IF_BUILD_SAME_IF_NAME		1 //	"\t   	1 -- use same name of interface\n"
#define IF_BUILD_SIMILAR_IF_NAME	2 //	"\t   	2 -- use similar name of interface\n"
#define IF_BUILD_EXIST				3 //	"\t   	3 -- use same address of interface\n"
#define IF_BUILD_EXIST_DEL			4 //	"\t   	4 -- use same address of interface, delete at exit\n"
#define IF_BUILD_SAME_IF_NAME_KEEP 	5 //	"(default)use same name of interface and keep created one\n"

#define IF_BUILD_MAX				6


typedef struct TParameters
{
	int 			m_running;

	// from command line parameters
	//int 			m_verbosity;	// verbostiy level
	int 			m_script_debug;	// enable script debug
	const char*		m_ifup_script;	// the ifup script for ping client
	const char*		m_ifdown_script;// the ifdown script for ping client
	unsigned int	m_if_build_method; //IF_BUILD_XXX

	int 			m_cmd_line_mode;		// whether it runs once,

	unsigned int 	m_session_id;


	int 			m_remove_rdb;

	int 			m_connect_timeout_ms; //ms
	int 			m_session_timeout_ms; //ms
	int				m_sample_interval;	//second
	int 			m_enable_sample_window;

	int 			m_console_test_mode; // repeat test and update the min max, throughput
	//int 			m_httpmovingaveragewindowsize;

	int 			m_enabled_function; // 1 download 2-updload
	int 			m_if_ops;

} TParameters;

#endif
