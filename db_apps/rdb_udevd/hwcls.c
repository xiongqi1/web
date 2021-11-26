#define _GNU_SOURCE

#include "hwcls.h"
#include "str2ref.h"
#include "rdb.h"

#include "g.h"

/* hwclass - linked list */
static struct list_head hwcls_root;

static struct str2ref_t* hwcls_by_loc=NULL;
static struct str2ref_t* hwcls_by_id=NULL;

static int hwcls_idx[hwcls_type_max];



const char* hwcls_type_names[]={
	[hwcls_type_serial]="serial",
	[hwcls_type_eth]="eth",
	[hwcls_type_storage]="storage",
	[hwcls_type_pm]="phonemodule",
};

const char* hwcls_stat_names[]={
	[hwcls_stat_removed]="removed",
	[hwcls_stat_inserted]="inserted",
};

#warning TODO: move all string to index function to a module
enum hwcls_type_t hwcls_get_type_id(const char* hwcls_type)
{
	return search_str_in_array(hwcls_type,hwcls_type_names,COUNTOF(hwcls_type_names),hwcls_type_unknown);
}

int hwcls_get_first(struct hwcls_t** hc)
{
	return str2ref_get_first(hwcls_by_id,NULL,(void**)hc);
}

int hwcls_get_next(struct hwcls_t** hc)
{
	return str2ref_get_next(hwcls_by_id,NULL,(void**)hc);
}

int hwcls_search_by_id_and_loc(const struct hwdev_t* key,struct hwcls_t** hc)
{
	struct hwcls_t* e;
	struct hwcls_t* e2;

	int e_loc_matched;
	
	int stat;
	int use_loc_as_id;
	
	/* init. results */	
	*hc=NULL;
	
	use_loc_as_id=0;
	
	/* search the first one */
	stat=str2ref_find_first(hwcls_by_id,key->id,(void**)&e);
	if(stat<0)
		goto err;

	/* if id and loc are matching together */
	e_loc_matched=!strcmp(key->loc,e->loc);
	if(e_loc_matched)
		goto fini;
		
	/* look for any 2nd class */
	stat=str2ref_find_next(hwcls_by_id,key->id,(void**)&e2);
	
	/* use location as an id if multiple ids are found or if the 1st one is not in the removed state */
	use_loc_as_id=(stat>=0) || (e->stat!=hwcls_stat_removed);
		
	if(use_loc_as_id) {
		stat=str2ref_find_first(hwcls_by_loc,key->loc,(void**)&e);
		if(stat<0)
			goto err;
	}

fini:
	*hc=e;
	
	return 0;
err:
	return -1;	
}

int hwcls_fixup(struct hwcls_t* hc,int valid_idx)
{
	int stat;
	int stat2;
	
	int existing;
	int existing2;
	
	/* delete any previous hc */
	stat=str2ref_del_by_ref(hwcls_by_id,hc);
	stat2=str2ref_del_by_ref(hwcls_by_loc,hc);
	
	existing=stat>=0;
	existing2=stat2>=0;
	
	if((existing && !existing2) || (!existing && existing2)) {
		DBG(LOG_ERR,"###hwcls## internal structure integrity broken in id and loc rbtree (stat=%d,stat2=%d)",stat,stat2);
		goto err;
	}
	
	/* take hwclass index if valid */
	if(valid_idx) {
		if(hwcls_idx[hc->type]<hc->idx+1)
			hwcls_idx[hc->type]=hc->idx+1;
	}
	else {
		if(!existing)
			hc->idx=hwcls_idx[hc->type]++;
	}
	
	/* add id */
	stat=str2ref_add(hwcls_by_id,hc->id,hc);
	if(stat<0) {
		DBG(LOG_ERR,"failed in str2ref_add(id)");
		goto err;
	}
	
	/* add loc */
	stat=str2ref_add(hwcls_by_loc,hc->loc,hc);
	if(stat<0) {
		DBG(LOG_ERR,"failed in str2ref_add(loc)");
		goto err;
	}
	
	return 0;
err:
	return -1;	
	
}

void hwcls_put(struct hwcls_t* hc)
{
	list_del(&hc->list);
	free(hc);
}

struct hwcls_t* hwcls_get()
{
	struct hwcls_t* hc;
	
	/* alloc hc */
	hc=malloc(sizeof(*hc));
	if(!hc) {
		DBG(LOG_ERR,"failed in malloc() - %s",strerror(errno));
		goto err;
	}
	
	/* init. members */
	memset(hc,0,sizeof(*hc));
	hc->idx=-1;
	hc->enable=1;
	hc->stat=hwcls_stat_removed;
	
	/* init. list */
	INIT_LIST_HEAD(&hc->list);
	
	/* add to root */
	list_add(&hwcls_root,&hc->list);
	
	
	
	
	return hc;
	
err:
	return NULL;
}


int hwcls_init()
{
	/* init hwcls root */
	INIT_LIST_HEAD(&hwcls_root);
	
	/* create rbtrees */
	hwcls_by_loc=str2ref_create();
	hwcls_by_id=str2ref_create();
	
	/* init. hwcls index */
	memset(hwcls_idx,0,sizeof(hwcls_idx));
	
	return 0;
}

void hwcls_fini()
{
	str2ref_destroy(hwcls_by_loc);
	str2ref_destroy(hwcls_by_id);
}

const char* hwcls_get_class_type(struct hwcls_t* hc)
{
	return hwcls_type_names[hc->type];
}

const char* hwcls_get_class_name(struct hwcls_t* hc)
{
	static char cls_name[RDB_VARIABLE_NAME_MAX_LEN];
	const char* type_name;
	
	/* get class type name */
	type_name=hwcls_type_names[hc->type];
	
	snprintf(cls_name,sizeof(cls_name),"%s.%d",type_name,hc->idx);
	
	return cls_name;
}

const char* hwcls_get_class_stat(struct hwcls_t* hc)
{
	return hwcls_stat_names[hc->stat];
}

int hwcls_write_to_rdb(struct hwcls_t* hc,int removed)
{
	int stat;
	
	const char* type_name;
	const char* var;
	
	const char* hwdev_name=NULL;
	
	const char* name;
	const char* mac;
	
	type_name=hwcls_type_names[hc->type];
	
	#define update_hc_member_in_rdb(m,r,p) \
		do { \
			var=rdb_var_printf("sys.hw.class.%s.%d.%s",type_name,hc->idx,r); \
			stat=rdb_set_value(var,(m)?(m):"",p); \
			if(stat<0) \
				DBG(LOG_ERR,"failed to write hwcls rdb (var=%s,val=%s)",var,m); \
		} while(0)
		
	
	/* get hwdev name */
	if(hc->legacy) {
		hwdev_name="";
		name=hc->legacy_name;
		mac="";
	}
	else if(hc->hwdev) {
		hwdev_name=hwdev_get_hwdev_name(hc->hwdev);
		name=hwdev_get_dev_name(hc->hwdev);
		mac=hc->hwdev->mac;
	}
	else {
		hwdev_name="";
		name="";
		mac="";
	}
	
	/* update rdb variables */
	if(removed)
		update_hc_member_in_rdb("","dev",0);
	else
		update_hc_member_in_rdb(hwdev_name,"dev",0);
		
	update_hc_member_in_rdb(name,"name",0);
	update_hc_member_in_rdb(mac,"mac",1);
	update_hc_member_in_rdb(hc->id,"id",1);
	update_hc_member_in_rdb(hc->loc,"location",1);
	update_hc_member_in_rdb(hwcls_stat_names[hc->stat],"status",0);
	update_hc_member_in_rdb(hc->enable?"1":"0","enable",1);

	return 0;
}

int hwcls_read_from_rdb(struct hwcls_t* hc,const char* rdb)
{
	char* rdb2;
	char* sp;
	
	const char* hwcls_type_str;
	const char* hwcls_idx_str;
	
	const char* val;
	
	const char* class_name;
	
	rdb2=strdupa(rdb);
	
	
	/* get hwclass type */
	hwcls_type_str=strtok_r(rdb2+STRLEN(HWCLS_RDB_PREFIX),".",&sp);
	if(!hwcls_type_str) {
		DBG(LOG_ERR,"hwclass type not found");
		goto err;
	}
	hc->type=hwcls_get_type_id(hwcls_type_str);
	
	/* get hwclass index */
	hwcls_idx_str=strtok_r(NULL,".",&sp);
	if(!hwcls_idx_str) {
		DBG(LOG_ERR,"hwclass index not found");
		goto err;
	}
	hc->idx=atoi(hwcls_idx_str);
	
	/* get class name */
	class_name=hwcls_get_class_name(hc);
	
	/* get id */
	rdb=rdb_var_printf("%s%s.id",HWCLS_RDB_PREFIX,class_name);
	val=rdb_get_value(rdb);
	if(!val) {
		DBG(LOG_ERR,"id of hwclass missing (rdb=%s)",rdb);
		goto err;
	}
	strlcpy(hc->id,val,HWDEV_ID_LEN);
	
	/* get location */
	rdb=rdb_var_printf("%s%s.location",HWCLS_RDB_PREFIX,class_name);
	val=rdb_get_value(rdb);
	if(!val) {
		DBG(LOG_ERR,"location of hwclass missing (rdb=%s)",rdb);
		goto err;
	}
	strlcpy(hc->loc,val,HWDEV_LOC_LEN);
	
	/* get enable */
	rdb=rdb_var_printf("%s%s.enable",HWCLS_RDB_PREFIX,class_name);
	val=rdb_get_value(rdb);
	hc->enable=!val || atoi(val);

	/* get static */
	rdb=rdb_var_printf("%s%s.static",HWCLS_RDB_PREFIX,class_name);
	val=rdb_get_value(rdb);
	hc->legacy=val && atoi(val);

	if(hc->legacy) {
		/* get dev */
		rdb=rdb_var_printf("%s%s.name",HWCLS_RDB_PREFIX,class_name);
		val=rdb_get_value(rdb);
		if(!val) {
			DBG(LOG_ERR,"dev of hwclass missing (rdb=%s)",rdb);
			goto err;
		}

		strlcpy(hc->legacy_name,val,HWDEV_RDB_DEV_NAME);
	}
	
	/* set stat */
	hc->stat=hwcls_stat_removed;

	return 0;
err:
	return -1;

}


