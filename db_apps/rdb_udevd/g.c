#include <string.h>

#include "g.h"

int search_str_in_array(const char* key,const char** strs,int cnt,int def)
{
	int i;
	
	
	for(i=0;i<cnt;i++) {
		if(strs[i] && !strcmp(strs[i],key)) {
			return i;
		}
	}
	
	return def;
}

