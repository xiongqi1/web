#ifndef __FEATUREHASH_H__
#define __FEATUREHASH_H__

#ifndef __USE_GNU
#define __USE_GNU
#endif

#include <search.h>

#define FEATUREHASH_MAX		100


#define FEATUREHASH_ALL			"all"
#define FEATUREHASH_UNSPECIFIED		"unspecified"

#define FEATUREHASH_CMD_BANDSEL		"bandsel"
#define FEATUREHASH_CMD_PROVIDER	"providerscan"
#define FEATUREHASH_CMD_SIMCARD		"simcard"
#define FEATUREHASH_CMD_CONNECT		"connect"
#define FEATUREHASH_CMD_BANDSTAT	"bandstat"

// cns feature
#define FEATUREHASH_CMD_ALLCNS		"allcns"
#define FEATUREHASH_CMD_ALLAT		"allat"


struct featurehash_t {
	int data_created;
	struct hsearch_data htab;
};

struct featurehash_element_t {
	const char* str;
	
};

void featurehash_destroy(struct featurehash_t* hash);
struct featurehash_t* featurehash_create(void);
int featurehash_is_enabled(struct featurehash_t* hash,const char* str);
int featurehash_add(struct featurehash_t* hash, const char* str,const int enable);

int is_enabled_feature(const char* str);
int add_feature(const char* str,const int enable);
int init_feature();
void fini_feature();

		
#endif
