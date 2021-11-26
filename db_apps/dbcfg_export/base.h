#ifndef __BASE_H__
#define __BASE_H__

#include <sys/times.h>

#ifndef NULL
#define NULL				0
#endif

#define FALSE				0
#define TRUE				1
//
//typedef u_int32_t u32;
//typedef u_int16_t u16;
//typedef u_int8_t u8;

typedef int BOOL;


///* Verison Information */
//#define VER_MJ		0
//#define VER_MN		1
//#define VER_BLD	1
//
////
//#define DAEMON_NAME "rdb-manager"
//
///* The user under which to run */
//#define RUN_AS_USER "rdbdaemon"
//
//
//extern int g_fSigTerm;
//
//#include <unistd.h>
//#include <sys/times.h>
//
//clock_t __getTicksPerSecond(void);
//clock_t __getTickCount(void);

#define __packedStruct							__attribute__((packed))

#endif
