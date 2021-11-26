#include "hwdev.h"
#include "str2ref.h"
#include "rdb.h"

#include "g.h"

static struct list_head hwdev_pool; /* hwdev pool - offline hwdev */
static struct list_head hwdev_root; /* online hwdev */

static struct str2ref_t* hwdev_by_devpath=NULL;

const char* hw_dev_type_names[]={
	[hw_dev_type_unknown]="unknown",
	[hw_dev_type_serial]="serial",
	[hw_dev_type_eth]="eth",
	[hw_dev_type_storage]="storage",
};

static void hwdev_free(struct hwdev_t* hd)
{
	#define free_hd_member(m) \
		do { \
			if(m) \
				free(m); \
			m=NULL; \
		} while(0)
		
	/* free members */
	free_hd_member(hd->name);
	free_hd_member(hd->loc);
	free_hd_member(hd->id);
	free_hd_member(hd->devpath);
	free_hd_member(hd->driver);
	free_hd_member(hd->mac);
}

void hwdev_destroy(struct hwdev_t* hd)
{
	hwdev_free(hd);
	free(hd);
}

int hwdev_alloc(struct hwdev_t* hd)
{
	#define malloc_hd_member(m,l,t) \
		do { \
			if(!m) { \
				m=(t)malloc(l); \
				if(!m) { \
					DBG(LOG_ERR,"failed to allocate %s - %s",#m,strerror(errno)); \
					goto err; \
				} \
				*m=0; \
			} \
		} while(0)
	
	malloc_hd_member(hd->name,HWDEV_NAME_LEN,char*);
	malloc_hd_member(hd->loc,HWDEV_LOC_LEN,char*);
	malloc_hd_member(hd->id,HWDEV_ID_LEN,char*);
	malloc_hd_member(hd->devpath,HWDEV_DEVPATH_LEN,char*);
	malloc_hd_member(hd->driver,HWDEV_DRIVER_LEN,char*);
	malloc_hd_member(hd->mac,HWDEV_MAC_LEN,char*);
		
	return 0;
err:		
	hwdev_free(hd);
	return -1;
}

static struct hwdev_t* hwdev_create()
{
	struct hwdev_t* hd;
	
	/* alloc. hd */
	hd=malloc(sizeof(*hd));
	if(!hd) {
		DBG(LOG_ERR,"failed to allocate hd - %s",strerror(errno));
		goto err;
	}
	
	/* init. members */
	memset(hd,0,sizeof(*hd));
	
	INIT_LIST_HEAD(&hd->list);
	
	return hd;
	
err:
	return NULL;	
	
}

struct hwdev_t* hwdev_search_by_devpath(const char* devpath)
{
	struct hwdev_t* hd;
	
	/* search */
	if(str2ref_find_first(hwdev_by_devpath,devpath,(void**)&hd)<0)
		goto err;
		
	return hd;
		
err:		
	return NULL;		
}

void hwdev_put(struct hwdev_t* hd,int tail)
{
	/* remove devpath */
	if(hd->devpath && *hd->devpath) {
		if(str2ref_del(hwdev_by_devpath,hd->devpath)<0) {
			DBG(LOG_ERR,"failed in str2ref_del() - cannot find devpath (devpath=%s)",hd->devpath);
		}
	}
	
	/* move to pool */
	list_del(&hd->list);
	
	if(tail)
		list_add_tail(&hd->list,&hwdev_pool);
	else
		list_add(&hd->list,&hwdev_pool);
		
	/* free member */
	hwdev_free(hd);
}

int hwdev_fixup(struct hwdev_t* hd)
{
	int stat;
	
	/* delete any previous hd */
	str2ref_del_by_ref(hwdev_by_devpath,hd);
	
	/* add devpath */
	stat=str2ref_add(hwdev_by_devpath,hd->devpath,hd);
	if(stat<0) {
		DBG(LOG_ERR,"failed in str2ref_add()");
		goto err;
	}
	
	return 0;
err:
	return -1;	
}	

struct hwdev_t* hwdev_get()
{
	struct hwdev_t* hd;
	
	/* bypass if no hwdev available */
	if(list_empty(&hwdev_pool)) {
		DBG(LOG_ERR,"no hwdev available");
		return NULL;
	}
	
	/* get hd */
	hd=container_of(hwdev_pool.next,struct hwdev_t,list);
	
	/* alloc */
	if(hwdev_alloc(hd)<0) {
		DBG(LOG_ERR,"failed to alloc a hwdev");
		goto err;
	}
	
	/* move from pool to root */ 
	list_del(&hd->list);
	list_add(&hd->list,&hwdev_root);
	
	return hd;
	
err:
	return NULL;	
}

void hwdev_fini()
{
	struct hwdev_t* hd;
	
	/* put all hwdevs */
	DBG(LOG_INFO,"put hwdevs");
	while(!list_empty(&hwdev_root)) {
		hd=container_of(hwdev_root.next,struct hwdev_t,list);
		hwdev_put(hd,0);
	}
	
	/* destroy all hwdevs in the pool */
	DBG(LOG_INFO,"destroy hwdevs");
	while(!list_empty(&hwdev_pool)) {
		hd=container_of(hwdev_pool.next,struct hwdev_t,list);
		list_del(&hd->list);
		hwdev_destroy(hd);
	}
	
	DBG(LOG_INFO,"destroy hwdev_by_devpath");
	str2ref_destroy(hwdev_by_devpath);
}

int hwdev_init()
{
	int i;
	struct hwdev_t* hd;
	
	/* init. list roots */
	INIT_LIST_HEAD(&hwdev_pool);
	INIT_LIST_HEAD(&hwdev_root);
	
	hwdev_by_devpath=str2ref_create();
	
	/* build pool of hwdevs */
	for(i=0;i<HWDEV_MAX_DEVICE;i++) {
	
		/* create hd */
		hd=hwdev_create();
		if(!hd) {
			DBG(LOG_ERR,"failed to create hd (i=%d,max=%d)",i,HWDEV_MAX_DEVICE);
			goto err;
		}
		
		/* init. members */
		hd->idx=i;
		
		/* add to pool */
		list_add_tail(&hd->list,&hwdev_pool);
	}
	
	return 0;
err:
	return -1;	
}

const char* hwdev_get_dev_type(struct hwdev_t* hd)
{
	return hw_dev_type_names[hd->type];
}

const char* hwdev_get_dev_name(struct hwdev_t* hd)
{
	return hd->name;
}

const char* hwdev_get_hwdev_name(struct hwdev_t* hd)
{
	static char dev_name[HWDEV_RDB_DEV_NAME];
	
	snprintf(dev_name,sizeof(dev_name),"dev.%d",hd->idx);
	
	return dev_name;
}

int hwdev_write_to_rdb(struct hwdev_t* hd,int removed)
{
	int stat;
	const char* var;
	const char* dev_type;
	
	#define update_hd_member_in_rdb(m,r,c) \
		do { \
			var=rdb_var_printf("%s%d.%s",HWDEV_RDB_PREFIX,hd->idx,r); \
			if(c && !removed) { \
				stat=rdb_set_value(var,m,0); \
				if(stat<0) \
					DBG(LOG_ERR,"failed to set rdb [%s] (var=%s) - %s",var,m,strerror(errno)); \
			} \
			else { \
				stat=rdb_del(var); \
				if(stat<0) \
					DBG(LOG_ERR,"failed to delete rdb [%s] - %s",var,strerror(errno)); \
			} \
		} while(0)
	
	
	update_hd_member_in_rdb(hd->name,"name",hd->name!=NULL);
	update_hd_member_in_rdb(hd->loc,"location",hd->loc!=NULL);
	update_hd_member_in_rdb(hd->id,"id",hd->id!=NULL);
	update_hd_member_in_rdb(hd->devpath,"devpath",hd->devpath!=NULL);
	update_hd_member_in_rdb(hd->driver,"driver",hd->driver!=NULL);
	
	dev_type=hwdev_get_dev_type(hd);
	update_hd_member_in_rdb(dev_type,"type",dev_type!=NULL);
	
	return 0;
}
