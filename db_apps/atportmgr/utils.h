#ifndef __UTILS_H__
#define __UTILS_H__

#include <sys/types.h>
#include <regex.h>

#define ATPORTMGR_ATANS_LEN						4096

#define CMDCHANGER_BRACKET_BEGIN		"(["
#define CMDCHANGER_BRACKET_END			"])"


const char* __convCtrlToCStyle(const char* szRaw);

int __getRegPatternEx(const char* szPattern, const char* szStr, regmatch_t* pRegMatch);
int __getRegTokenEx(const char* szRegTokenEx, const char* szStr, char* pOut, int cbOut);

#endif
