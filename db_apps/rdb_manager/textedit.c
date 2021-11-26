
#include "textedit.h"

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <unistd.h> 
#include <stdarg.h> 
#include <errno.h>

#include "base.h"
#include "list.h"
#include "uricodec.h"
#include "rdb_ops.h"

static LIST_HEAD(_listHdr);

static struct list_head* _pFindCurPos;

static texteditline* textedit_alloc_line();
static void textedit_free_line(texteditline* pNewLine);
static BOOL textedit_grow_line(texteditline* pLine,int length);

static char encodedBuf[RDBMANAGER_DATABASE_VARIABLE_ENCODED_LENGTH];
static char lineBuf[TEXTEDIT_MAX_LINE_LENGTH];
static char outBuf[TEXTEDIT_MAX_LINE_LENGTH];

///////////////////////////////////////////////////////////////////////////////////////////////////
int writeCfgFormat(texteditline* pLine, const char* szName, int nUser, int nGroup, int nPerm, int nFlags, const char* szValue)
{
	// uri encode
	
	int ret = uriEncode(szValue, encodedBuf, sizeof(encodedBuf));
	if (ret < strlen(szValue)) {
		syslog(LOG_ERR, "sting [%.50s...]is too long to be encoded completely.", szValue);
	}

	nFlags&=PERSIST | CRYPT | STATISTICS;

	// printf
	const char* szFmtWriteLine = "%s;%d;%d;0x%x;0x%x;%s";		// line writer format - achName, nUser, nGroup, nPerm, nFlags, achValue;
	return textedit_sprintf_line(pLine,szFmtWriteLine, szName, nUser, nGroup, nPerm, nFlags, encodedBuf);
}
///////////////////////////////////////////////////////////////////////////////////////////////////
int writeCfgFormatF(FILE* pFOut, const char* szName, int nUser, int nGroup, int nPerm, int nFlags, const char* szValue)
{
	// uri encode
	int ret = uriEncode(szValue, encodedBuf, sizeof(encodedBuf));
	if (ret < strlen(szValue)) {
		syslog(LOG_ERR, "sting [%.50s...]is too long to be encoded completely.", szValue);
	}

	nFlags&=PERSIST | CRYPT | STATISTICS;

	// printf
	const char* szFmtWriteLine = "%s;%d;%d;0x%x;0x%x;%s\n";		// line writer format - achName, nUser, nGroup, nPerm, nFlags, achValue;
	return fprintf(pFOut, szFmtWriteLine, szName, nUser, nGroup, nPerm, nFlags, encodedBuf);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
static int getSepCnt(const char* szLine)
{
	int cSep = 0;
	
	while (*szLine)
	{
		if (*szLine++ == ';')
			cSep++;
	}

	return cSep;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int getLastItem(const char* achCfg,char* pValue,int len)
{
	const char* pPtr=achCfg;
	const char* pLastD=NULL;

	// get last delimitor
	while(*pPtr)
	{
		if(*pPtr==';')
			pLastD=pPtr;
		pPtr++;
	}

	// bypass if no delimitor
	if(!pLastD)
		return -1;

	const char* pLastStr=pLastD+1;

	// bypass if zero-length
	if(!(strlen(pLastStr)>0))
		return -1;

	strncpy(pValue,pLastStr,len);
	pValue[len-1]=0;

	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int writeRawExtCfgformatF(FILE* pFOut, char* szName,char* pValue)
{
	return fprintf(pFOut, "%s;%s\n", szName, pValue);
}
///////////////////////////////////////////////////////////////////////////////////////////////////
int writeRawExtCfgformat(char* pBuf, char* szName,char* pValue)
{
	return sprintf(pBuf, "%s;%s", szName, pValue);
}
///////////////////////////////////////////////////////////////////////////////////////////////////
int readRawExtCfgFormat(texteditline* pLine, char* szName, char* pValue,int len)
{
	const char* szFmtReadLine = "%[^;];%s"; // line reader format - achName, achValue;
	const char* pBuf;
	
	pBuf=textedit_get_line_ptr(pLine);
	
	int cGot = sscanf(pBuf, szFmtReadLine, szName, pValue);

	if(!(cGot>0))
		return -1;

	if (cGot == 1)
		*pValue=0;
	else
		getLastItem(pBuf,pValue,len);

	return 0;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
int readCfgFormat(texteditline* pLine, char* szName, int* pUser, int* pGroup, int* pPerm, int* pFlags, char* pValue,int len)
{
	const char* pBuf;
	
	// use dummy ptr if NULL
	int nDummy = 0;
	if (!pFlags)
		pFlags = &nDummy;

	// get delimiter
	int cSep;
	
	char* temp_buf;
	int temp_buf_len;
	
	pBuf=textedit_get_line_ptr(pLine);

	cSep = getSepCnt(pBuf);
	
	/* allocate temp. buf */
	temp_buf_len=strlen(pBuf)+1;
	temp_buf=alloca(temp_buf_len);
	
	switch (cSep)
	{
		default:
		{
			const char* szFmtReadLine = "%[^;];%i;%i;%i;%i;%s"; // line reader format - achName, nUser, nGroup, nPerm, nFlag, achValue;
			
			// scanf
			int cGot = sscanf(pBuf, szFmtReadLine, szName, pUser, pGroup, pPerm, pFlags, temp_buf);
			if (cGot < 4)
				break;

			// use zero-length string if only 5
			if (cGot == 5)
				*temp_buf = 0;
			else
				getLastItem(pBuf,temp_buf,temp_buf_len);
			
			// uri decode
			uriDecode(temp_buf, pValue, len);

			return 0;
		}

		case 4:
		{
			const char* szFmtReadLine = "%[^;];%i;%i;%i;%s"; // line reader format - achName, nUser, nGroup, nPerm, achValue;

			// scanf
			int cGot = sscanf(pBuf, szFmtReadLine, szName, pUser, pGroup, pPerm, temp_buf);
			if (cGot < 4)
				break;

			// use zero-length string if only 4
			if (cGot == 4)
				*pValue = 0;
			else
				getLastItem(pBuf,pValue,len);

			*pFlags = 0;

			return 0;
		}
	}

	return -1;
}


char* textedit_get_line_ptr(texteditline* pLine)
{
	return (char*)growingmem_getMem(pLine->g);
}
	
static BOOL textedit_grow_line(texteditline* pLine,int length)
{
	return growingmem_growIfNeeded(pLine->g,length);
}

int textedit_sprintf_line(texteditline* pLine,const char *format,...)
{
	int len;
	int memlen;
	char* mem;
	
	va_list ap;

	va_start(ap, format);

	/* printf */
	len=vsnprintf(lineBuf,sizeof(lineBuf),format,ap);
	/* increase memory */
	if(!textedit_grow_line(pLine,len+1)) {
		syslog(LOG_ERR,"failed in increasing line memory (len=%d)",len+1);
	}
	
	/* copy */
	memlen=pLine->g->cbMem;
	mem=textedit_get_line_ptr(pLine);
	strncpy(mem,lineBuf,memlen);
	mem[memlen-1]=0;
	
	/* get return */
	len=strlen(mem);
	
	va_end(ap);
	
	return len;
	
	
}

///////////////////////////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////////////////////////
void textedit_deleteLine(texteditline* pLine)
{
	list_del(&pLine->list);
	textedit_free_line(pLine);
}
///////////////////////////////////////////////////////////////////////////////////////////////////
texteditline* textedit_findNext()
{
	struct list_head* pNext = _pFindCurPos->next;

	if (pNext == &_listHdr)
		return NULL;

	_pFindCurPos = pNext;

	return list_entry(_pFindCurPos, texteditline, list);
}

static void textedit_free_line(texteditline* pNewLine)
{
	if(!pNewLine)
		return;
	
	growingmem_destroy(pNewLine->g);
	free(pNewLine);
}

static texteditline* textedit_alloc_line()
{
	texteditline* pNewLine=NULL;
	
	// allocate a new line
	if ((pNewLine = malloc(sizeof(texteditline))) == NULL)
		goto err;
	
	memset(pNewLine, 0, sizeof(texteditline));
	
	/* create growing memory */
	pNewLine->g=growingmem_create(TEXTEDIT_LINE_LENGTH);
	if(pNewLine->g==NULL)
		goto err;
	
	/* do initial allocation */
	//growingmem_growIfNeeded(pNewLine->g,1);
			
	return pNewLine;
err:
	textedit_free_line(pNewLine);
		
	return NULL;	
}

///////////////////////////////////////////////////////////////////////////////////////////////////
static texteditline* textedit_addTailOrHead(BOOL fHead)
{
	texteditline* pNewLine;

	// allocate a new line
	if ((pNewLine = textedit_alloc_line()) == NULL)
		return NULL;
	
	if (fHead)
		list_add(&pNewLine->list, &_listHdr);
	else
		list_add_tail(&pNewLine->list, &_listHdr);

	return pNewLine;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
texteditline* textedit_addNext(texteditline* pLineTo)
{
	texteditline* pNewLine;
	
	// allocate a new line
	if ((pNewLine = textedit_alloc_line()) == NULL)
		return NULL;
	
	// allocate a new line
	list_add(&pNewLine->list, &pLineTo->list);

	return pNewLine;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
texteditline* textedit_addTail(void)
{
	return textedit_addTailOrHead(FALSE);
}
///////////////////////////////////////////////////////////////////////////////////////////////////
texteditline* textedit_addHead(void)
{
	return textedit_addTailOrHead(TRUE);
}
///////////////////////////////////////////////////////////////////////////////////////////////////
texteditline* textedit_findFirst(void)
{
	_pFindCurPos = &_listHdr;

	return textedit_findNext();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void textedit_unload(void)
{
	texteditline* pLine;

	while (!list_empty(&_listHdr))
	{
		pLine = list_entry(_listHdr.next, texteditline, list);
		list_del(&pLine->list);
		textedit_free_line(pLine);
	}
}
///////////////////////////////////////////////////////////////////////////////////////////////////
static void __trimStrCpy(char* pDst, char* pSrc)
{
	const char* szIgnoreLetters = " \n\r";

	// skip empty head
	while (*pSrc && strchr(szIgnoreLetters, *pSrc) != 0)
		pSrc++;

	// copy
    char* pLastPtr = pDst;
	char ch;
	while ((ch = *pSrc++) != 0)
	{
		*pDst++ = ch;

		if (!strchr(szIgnoreLetters, ch))
			pLastPtr = pDst;
	}

	*pLastPtr = 0;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
BOOL textedit_save(const char* szCfgFName,textedit_save_cb_t cb)
{
	FILE* fp;

	BOOL fSucc = TRUE;

	// open configuration file
	if ((fp = fopen(szCfgFName, "w")) == NULL)
	{
		syslog(LOG_CRIT, "failed to open configuration file(%s) for write - %s", szCfgFName,strerror(errno));
		return FALSE;
	}

	syslog(LOG_DEBUG, "succeeded to open configuration file (%s)", szCfgFName);

	int iLine = 0;
	texteditline* pLine = textedit_findFirst();
	while (pLine)
	{
		iLine++;
		if(cb) {
			cb(iLine,pLine);
		}
		
		if (fputs(textedit_get_line_ptr(pLine), fp) < 0 || fputs("\n", fp) < 0)
		{
			syslog(LOG_ERR, "failed to write a line(%d) in configuration file", iLine);
			fSucc = FALSE;
			break;
		}

		pLine = textedit_findNext();
	}

	// close
	fclose(fp);

	syslog(LOG_DEBUG, "configuration file written. (filename=%s, total lines=%d)", szCfgFName, iLine);

	return fSucc;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
int textedit_load(const char* szCfgFName)
{
	int stat = 0;
	FILE* fp;
	
	// open configuration file
	if ((fp = fopen(szCfgFName, "r")) == NULL)
	{
		syslog(LOG_CRIT, "failed to open %s for read-only", szCfgFName);
		return -1;
	}

	// read configuration file
	texteditline* pNewLine;
	int cbContent;

	int iLine = 0;


	while (fgets(lineBuf, sizeof(lineBuf), fp))
	{
		iLine++;
		cbContent = strlen(lineBuf);

		// waste if a big line detected
		if (lineBuf[cbContent-1] != '\n')
		{
			syslog(LOG_ERR, "the line(%d) is too big. any letter after column %d is igonored", iLine, sizeof(lineBuf));

			// cut the big line
			lineBuf[sizeof(lineBuf)-2] = 0;

			char achWaste[2];
			while (fgets(achWaste, sizeof(achWaste), fp))
			{
				if (achWaste[0] == '\n')
					break;
			}
		}

		// allocate a new line
		if ((pNewLine = textedit_alloc_line()) == NULL)
		{
			syslog(LOG_ERR, "failed to allocate memory for a line (%d)", iLine);
			stat = -1;
			break;
		}

		// do a trim copy
		__trimStrCpy(outBuf, lineBuf);
		textedit_sprintf_line(pNewLine,"%s",outBuf);
		
		// add to tail
		list_add_tail(&pNewLine->list, &_listHdr);
	}

	fclose(fp);

	syslog(LOG_DEBUG, "configuration file loaded. (filename=%s, total lines=%d)", szCfgFName, iLine);
	return stat;
}
