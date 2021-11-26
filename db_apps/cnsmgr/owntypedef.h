#ifndef __OWNTYPEDEF_H__
#define __OWNTYPEDEF_H__


#define TRUE		1
#define FALSE		0

typedef int BOOL;
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned int DWORD;
typedef long long unsigned int UINT64;
typedef long long int INT64;

typedef struct
{
	union
	{
		struct
		{
			DWORD dw32lo;
			DWORD dw32hi;
		};

		unsigned long long u64;
	};

} UINT64STRUC;


#endif

