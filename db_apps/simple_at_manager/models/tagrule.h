#ifndef __SUPPL__H__
#define __SUPPL__H__


#include <sys/types.h>
#include <regex.h>

#define TAGREGEX_MAX_TAG				10

///////////////////////////////////////////////////////////////////////////////
struct tagrule {
	const char* szRule;

	regmatch_t tagRuleMatches[TAGREGEX_MAX_TAG];
	int cTag;
};

///////////////////////////////////////////////////////////////////////////////
struct tagrule* tagruleCreate(void);
int tagruleSetRules(struct tagrule* pT, const char* szRule);

int tagruleIsMatched(struct tagrule* pT,const char* szStr);
char* tagruleGetMatchbyTag(struct tagrule* pT,const char* szStr,const char* szTag,char* achBuf,int cbBuf);

int tagruleIsEmpty(struct tagrule* pT);

#endif
