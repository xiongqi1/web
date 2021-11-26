#ifndef __CFGEDITOR_H__
#define __CFGEDITOR_H__

#include <stdio.h>

#include "list.h"
#include "growingmem.h"

#define TEXTEDIT_LINE_LENGTH		64 /*!< maximum length per line in a configuration file */

#define RDBMANAGER_DATABASE_VARIABLE_ENCODED_LENGTH RDBMANAGER_DATABASE_VARIABLE_LENGTH*3 /* RDBMANAGER_DATABASE_VARIABLE_LENGTH comes from Makefile.
                                                                                            The worst case is every byte needed to encode.
                                                                                            If there is encoded data longer than this, the remaining data
                                                                                            will be discarded and an error log will be displayed.
                                                                                            */
#define TEXTEDIT_MAX_LINE_LENGTH	(RDBMANAGER_DATABASE_VARIABLE_ENCODED_LENGTH + 256) /* line length is longer since it contains encoded data and other properties.
                                                                                   256 is long enough for other RDB properties.
                                                                                */

//! configuration editor line
/*! This structure represents a line in a configuration file */
typedef struct
{
	struct list_head list;									/*!< structure sustaining linked list */
	int fComment;
	
	pagedmem* g;

} texteditline;

typedef int (*textedit_save_cb_t)(int nLine,texteditline* pLine);


texteditline* textedit_findNext();
texteditline* textedit_findFirst(void);
void textedit_unload(void);
BOOL textedit_save(const char* szCfgFName,textedit_save_cb_t cb);
int textedit_load(const char* szCfgFName);

texteditline* textedit_addTail(void);
texteditline* textedit_addHead(void);
texteditline* textedit_addNext(texteditline* pLineTo);
void textedit_deleteLine(texteditline* pLine);


int writeCfgFormatF(FILE* pFOut, const char* szName, int nUser, int nGroup, int nPerm, int nFlags, const char* szValue);
int writeCfgFormat(texteditline* pLine, const char* szName, int nUser, int nGroup, int nPerm, int nFlags, const char* szValue);
int readCfgFormat(texteditline* pLine, char* szName, int* pUser, int* pGroup, int* pPerm, int* pFlags, char* pValue,int len);

int readRawExtCfgFormat(texteditline* pLine, char* szName, char* pValue,int len);
int writeRawExtCfgformat(char* pBuf, char* szName,char* pValue);
int writeRawExtCfgformatF(FILE* pFOut, char* szName,char* pValue);

char* textedit_get_line_ptr(texteditline* pLine);
int textedit_sprintf_line(texteditline* pLine,const char *format,...);

#endif
