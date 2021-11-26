#include "tagrule.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <alloca.h>

#include "../dyna.h"

/*

	This class deals with named regular expression for tagruleementary dial string. Each
			
			\*004\*<number-[[:number:]]+>\*<timer-[[:number:]]+>#
			*21*<number>#

*/

#define TAGRULE_TAG_START			'<'
#define TAGRULE_TAG_MIDDY			':'
#define TAGRULE_TAG_END				'>'


///////////////////////////////////////////////////////////////////////////////
void tagruleDestroy(void* pRef)
{
	struct tagrule* pT=(struct tagrule*)pRef;

	if(pT->szRule)
		free((char*)pT->szRule);
}
///////////////////////////////////////////////////////////////////////////////
static int tagruleGetTagNameOrTagRule(struct tagrule* pT,int iTag,int fTagName,char* achBuf)
{
	regmatch_t* pMatched=&pT->tagRuleMatches[iTag];
	const char* szRule=pT->szRule;

	// bypass if out of range
	if(pMatched->rm_so<0 || pMatched->rm_eo<1)
		return -1;

	// check start and end bracket
	if(szRule[pMatched->rm_so]!=TAGRULE_TAG_START || szRule[pMatched->rm_eo-1]!=TAGRULE_TAG_END)
		return -1;
	
	// get middy
	int iMid=-1;
	int i;
	for(i=pMatched->rm_so;i<pMatched->rm_eo;i++)
	{
		if(szRule[i]!=TAGRULE_TAG_MIDDY)
			continue;

		iMid=i;
		break;
	}

	// bypass if no middy
	if(iMid<0)
		return -1;

	const char* pSrc;
	int cbRes;
		
	if(fTagName)
	{
		cbRes=iMid-(pMatched->rm_so+1);
		pSrc=&szRule[pMatched->rm_so+1];
	}
	else
	{
		cbRes=(pMatched->rm_eo-1)-(iMid+1);
		pSrc=&szRule[iMid+1];
	}
	
	strncpy(achBuf,pSrc,cbRes);
	achBuf[cbRes]=0;
	
	return cbRes;
}
///////////////////////////////////////////////////////////////////////////////
static int tagruleBuildRule(struct tagrule* pT,int iSTag,int fSTag, int iETag, int fETag, char* pBuf)
{
	char* pDst=pBuf;
	int i;

	regmatch_t* pTagPrev;
	regmatch_t* pTagCur;
	const char* szRule=pT->szRule;

	int cInc;

	pTagPrev=0;

	for(i=iSTag;i<=iETag;i++)
	{
		pTagCur=&pT->tagRuleMatches[i];

		// copy pad rule
		if(pTagPrev)
		{
			int iPad=pTagPrev->rm_eo;
			int cPad=pTagCur->rm_so-iPad;
			strncpy(pDst,&szRule[iPad],cPad);

			pDst+=cPad;
		}

		// copy tag rule
		if( ( (i==iSTag) && fSTag ) || ( (i==iETag) && fETag) || (i!=iSTag && i!=iETag) )
		{
			cInc=tagruleGetTagNameOrTagRule(pT,i,0,pDst);
			if(cInc<0)
				fprintf(stderr,"failed to get tag(%d) in rule(%s)",i,pT->szRule);
			else
				pDst+=cInc;
		}

		pTagPrev=pTagCur;
	}

	*pDst=0;

	return pDst-pBuf;
}

///////////////////////////////////////////////////////////////////////////////
static int __getMatch(const char* szPattern, const char* szStr, regmatch_t* pRegMatch)
{
	int fRegInit = 0;

	regex_t reEx;
	int stat;

	if(!szPattern || !strlen(szPattern))
		goto error;

	// compile
	stat = regcomp(&reEx, szPattern, REG_EXTENDED | REG_NEWLINE);
	if (stat < 0)
		goto error;

	fRegInit = 1;

	regmatch_t tmpRegMatch;
	if (!pRegMatch)
		pRegMatch = &tmpRegMatch;

	// exec
	int nEFlag = 0;
	stat = regexec(&reEx, szStr, 1, pRegMatch, nEFlag);
	if (stat != 0)
		goto error;

	regfree(&reEx);
	return 0;

error:
	if (fRegInit)
		regfree(&reEx);

	return -1;
}
///////////////////////////////////////////////////////////////////////////////
static int tagruleGetTagIdxByName(struct tagrule* pT,const char* szTag)
{
	char* pTag=alloca(strlen(pT->szRule)+1);

	int i;
	for(i=0;i<pT->cTag;i++)
	{
		if(tagruleGetTagNameOrTagRule(pT,i,1,pTag)<0)
			continue;

		if(!strcmp(pTag,szTag))
			return i;
	}

	return -1;
}

///////////////////////////////////////////////////////////////////////////////
char* tagruleGetMatchbyTag(struct tagrule* pT,const char* szStr,const char* szTag,char* achBuf,int cbBuf)
{
	int iTag=tagruleGetTagIdxByName(pT,szTag);
	if(iTag<0)
		return 0;

	char* szHdrRule=alloca(strlen(pT->szRule)+1);
	char* szTagRule=alloca(strlen(pT->szRule)+1);

	tagruleBuildRule(pT,0,0,iTag,0,szHdrRule);
	tagruleBuildRule(pT,iTag,1,iTag,1,szTagRule);

	int stat;
	
	// find header
	regmatch_t hdrMatch;
	stat=__getMatch(szHdrRule,szStr,&hdrMatch);
	if(stat<0)
		return 0;

	int iBase=hdrMatch.rm_eo;

	// find tag
	regmatch_t tagMatch;
	stat=__getMatch(szTagRule,&szStr[iBase],&tagMatch);
	if(stat<0)
		return 0;

	int cbPadLen=tagMatch.rm_eo-tagMatch.rm_so;
	if(cbPadLen<0)
		return 0;

	const char* pSrc=&szStr[iBase+tagMatch.rm_so];
	strncpy(achBuf,pSrc,cbPadLen);
	achBuf[cbPadLen]=0;

	return achBuf;
}
///////////////////////////////////////////////////////////////////////////////
int tagruleIsMatched(struct tagrule* pT,const char* szStr)
{
	char* pRegEx=alloca(strlen(pT->szRule)+1);

	tagruleBuildRule(pT,0,0,pT->cTag-1,0,pRegEx);

	return __getMatch(pRegEx,szStr,0)>=0;
}
///////////////////////////////////////////////////////////////////////////////
int tagruleIsEmpty(struct tagrule* pT)
{
	return !strlen(pT->szRule);
}
///////////////////////////////////////////////////////////////////////////////
int tagruleSetRules(struct tagrule* pT, const char* szRule)
{
	regex_t regEx;

	// set activation rule
	if(pT->szRule)
		free((char*)pT->szRule);
	pT->szRule=strdup(szRule);

	// get tags position
	char achTagRule[128];
	sprintf(achTagRule,"%c[a-zA-Z_][a-zA-Z_0-9]*%c[^>]+%c",TAGRULE_TAG_START,TAGRULE_TAG_MIDDY,TAGRULE_TAG_END);

	int stat;

	regcomp(&regEx,achTagRule, REG_EXTENDED | REG_NEWLINE);

	// get tags
	int iBase=0;
	int i;
	for(i=1;i<TAGREGEX_MAX_TAG-1;i++)
	{
		regmatch_t regMatch;

		stat=regexec(&regEx, &pT->szRule[iBase], 1, &regMatch, 0);
		if(stat<0 || stat==REG_NOMATCH)
			break;

		pT->tagRuleMatches[i].rm_so=iBase+regMatch.rm_so;
		pT->tagRuleMatches[i].rm_eo=iBase+regMatch.rm_eo;

		iBase=pT->tagRuleMatches[i].rm_eo;
	}

	regfree(&regEx);

	pT->cTag=i;


	// set virtual header tag
	pT->tagRuleMatches[0].rm_so=-1;
	pT->tagRuleMatches[0].rm_eo=0;
	// set virtual tail tag
	pT->tagRuleMatches[pT->cTag].rm_so=strlen(szRule);
	pT->tagRuleMatches[pT->cTag].rm_eo=-1;

	pT->cTag++;

	return pT->cTag;
}


///////////////////////////////////////////////////////////////////////////////
struct tagrule* tagruleCreate(void)
{
	struct tagrule* pT=0;

	// create tagruleementary
	pT =	(struct tagrule*)dynaCreate(sizeof(struct tagrule),tagruleDestroy);
	if(!pT)
		goto error;

	return pT;

error:
	dynaFree(pT);
	return 0;
}

