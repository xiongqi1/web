#include "g.h"

#include "rdb.h"
#include "hwdev.h"
#include "hwcls.h"

#define RDBUDEV_RDB_NOTI_SCRIPT	"sys.hw.cfg.hwclass_script"

/* conversion table # subsystem */
enum udev_subsystem {
	udev_subsystem_tty,
	udev_subsystem_net,
	udev_subsystem_block,
	udev_subsystem_unknown,
	
	udev_subsystem_max,
};

const char* udev_subsystem_names[]={
	[udev_subsystem_tty]="tty",
	[udev_subsystem_net]="net",
	[udev_subsystem_block]="block",
	[udev_subsystem_unknown]="unknown",
};

/* conversion table # action */
enum udev_action {
	udev_action_add,
	udev_action_remove,
	udev_action_change,
	udev_action_unknown,
	
	udev_action_max,
};

const char* udev_action_names[]={
	[udev_action_add]="add",
	[udev_action_remove]="remove",
	[udev_action_change]="change",
	[udev_action_unknown]="unknown",
};

/* platform id */
static int platform_ids[udev_subsystem_max];

/* local functions */
int rdbudev_call_script(struct hwcls_t* hc);
int rdbudev_add_new_hwdev(enum udev_subsystem udev_subsys,const char* devpath);

/* conversion function # action */
enum udev_action get_action_id(const char* action)
{
	enum udev_action a;
	
	int i;
	
	a=udev_action_unknown;
	for(i=0;i<udev_action_max;i++) {
		if(udev_action_names[i] && !strcmp(udev_action_names[i],action))
			a=i;
	}
	
	return a;
}

/* conversion function # subsystem */
enum udev_subsystem get_subsystem_id(const char* subsystem)
{
	enum udev_subsystem ss;
	
	ss=udev_subsystem_unknown;
	
	int i;
	
	ss=udev_subsystem_unknown;
	for(i=0;i<udev_subsystem_max;i++) {
		if(udev_subsystem_names[i] && !strcmp(udev_subsystem_names[i],subsystem))
			ss=i;
	}
	
	return ss;
}

void bp()
{
}

struct sysfs_device* rdbudev_get_platform_parent(struct sysfs_device* sfdev)
{
	struct sysfs_device* sfdev_usb;

	/* 
		TODO: any external bus has to be checked here
		assume it is a platform device if is not usb 
	*/

	sfdev_usb=sysfs_device_get_parent_with_subsystem(sfdev,"usb");
	if(sfdev_usb)
		return NULL;

	/* return itself */
	if (!strcmp(sfdev->subsystem, "platform"))
		return sfdev;

	return sysfs_device_get_parent_with_subsystem(sfdev,"platform");;
}

int rdbudev_set_gadget_hd(enum udev_subsystem udev_subsys,struct hwdev_t* hd,struct sysfs_device* sfdev,struct sysfs_device* sfdev_device)
{
	struct sysfs_device* sfdev_platform;
	
	/*
		platform <-- platform <--- ... <--- device
	*/
	
	/* get platform parent */
	sfdev_platform=sysfs_device_get_parent_with_subsystem(sfdev,"platform");
	if(!sfdev_platform) {
		DBG(LOG_ERR,"platform sysfs node not found (devpath=%s)",sfdev->devpath);
		goto err;
	}
	
	/* go one more level */
	sfdev_platform=sysfs_device_get_parent_with_subsystem(sfdev_platform,"platform");
	if(!sfdev_platform) {
		DBG(LOG_ERR,"platform sysfs node not found (devpath=%s)",sfdev->devpath);
		goto err;
	}
	
	/* build paltform part */
	snprintf(hd->id,HWDEV_ID_LEN,"gadget-%s-%s",udev_subsystem_names[udev_subsys],sfdev_platform->kernel);
	snprintf(hd->loc,HWDEV_LOC_LEN,"gadget");
	snprintf(hd->driver,HWDEV_DRIVER_LEN,sfdev_device->driver);
	
	return 0;
	
err:
	return -1;	
}

int rdbudev_set_platform_hd_with_extra(enum udev_subsystem udev_subsys,struct hwdev_t* hd,struct sysfs_device* sfdev,struct sysfs_device* sfdev_device,const char* extra_id,const char* extra_loc)
{
	struct sysfs_device* sfdev_platform;
	int dev_id;
	char* sys_str_dev_id;
	char* sys_str_address;
	
	/* get paltform parent */
	sfdev_platform=rdbudev_get_platform_parent(sfdev);
	if(!sfdev_platform) {
		DBG(LOG_ERR,"platform sysfs node not found (devpath=%s)",sfdev->devpath);
		goto err;
	}
	
	/* read dev id - differentiate multiple platform devices */
	sys_str_dev_id=sysfs_attr_get_value(sfdev->devpath,"dev_id");
	if(sys_str_dev_id) {
		sscanf(sys_str_dev_id,"%i",&dev_id);

		DBG(LOG_DEBUG,"###rdbudev### dev_id found - (sys_str_dev_id=%s,dev_id=%d)",sys_str_dev_id,dev_id);
	}
	else {
		dev_id=0;

		DBG(LOG_DEBUG,"###rdbudev### skip dev_id - not existing (devpath=%s)",sfdev->devpath);
	}

	/* read address */
	sys_str_address=sysfs_attr_get_value(sfdev->devpath,"address");
	if(sys_str_address) {
		snprintf(hd->mac,HWDEV_MAC_LEN,"%s",sys_str_address);

		DBG(LOG_DEBUG,"###rdbudev### address found - (sys_str_dev_id=%s,dev_id=%d)",sys_str_dev_id,dev_id);
	}
	else {
		*hd->mac=0;

		DBG(LOG_DEBUG,"###rdbudev### skip addressdev_id - not existing (devpath=%s)",sfdev->devpath);
	}
	
	
	/* build paltform part */
	if(dev_id)
		snprintf(hd->id,HWDEV_ID_LEN,"platform-%s-%d",sfdev_platform->kernel,dev_id);
	else
		snprintf(hd->id,HWDEV_ID_LEN,"platform-%s",sfdev_platform->kernel);

	/* add extra id */
	if(extra_id) {
		strlcat(hd->id,"-",HWDEV_ID_LEN);
		strlcat(hd->id,extra_id,HWDEV_ID_LEN);
	}

	/* TODO: all platform devices are located in platform and every platform device has a different ID */
	snprintf(hd->loc,HWDEV_LOC_LEN,"platform");

	/* add extra location */
	if(extra_loc) {
		strlcat(hd->loc,"-",HWDEV_LOC_LEN);
		strlcat(hd->loc,extra_loc,HWDEV_LOC_LEN);
	}

	snprintf(hd->driver,HWDEV_DRIVER_LEN,sfdev_device->driver);
	
	return 0;
	
err:
	return -1;	
}

int rdbudev_set_platform_hd(enum udev_subsystem udev_subsys,struct hwdev_t* hd,struct sysfs_device* sfdev,struct sysfs_device* sfdev_device)
{
	return rdbudev_set_platform_hd_with_extra(udev_subsys,hd,sfdev,sfdev_device,NULL,NULL);
}

static int isdigit_str(const char* v)
{
	while(*v) {
		if(!isdigit(*v++))
			return 0;
	}

	return 1;
}

static const char* get_bus_port_number(const char* devpath)
{
	regex_t reg;
	regmatch_t pm;
	int stat;
	int len;

	char* res=NULL;
	char* p;

	static char buf[1024];

	/* set default result */
	*buf=0;

	/* compile regex */
	stat=regcomp(&reg,"/usb[0-9]+/[^/]+",REG_EXTENDED);
	if(stat<0) {
		syslog(LOG_ERR,"failed in regcomp() - %s",strerror(errno));
		goto fini;
	}

	/* match */
	stat=regexec(&reg,devpath,1,&pm,0);
	if(stat==REG_NOMATCH) {
		goto fini;
	}

	len=pm.rm_eo-pm.rm_so;
	memcpy(buf,devpath+pm.rm_so,len);
	buf[len]=0;
	
	/* get start matching point */
	p=buf;
	
	/* search the 2nd slash */
	if((p=strchr(p+1,'/'))==NULL)
		goto fini;

	res=p+1;

fini:
	/* free regex */
	regfree(&reg);

	return res;
}

int rdbudev_set_usb_hd_with_extra(enum udev_subsystem udev_subsys,struct hwdev_t* hd,struct sysfs_device* sfdev,struct sysfs_device* sfdev_device,const char* extra_id,const char* extra_loc) 
{
	struct sysfs_device* sfdev_usb;
	struct sysfs_device* sfdev_gp;
	
	char* idproduct;
	char* idvendor;
	char* serial;
	char* address;
	char* busnum;
	char* binterfacenumber;
	char* bconfigurationvalue;
	char* devpath;
	char* driver;
	
	const char* cfg_busnum=RDBUDEV_ONBOARD_BUSNUM;
	const char* bus_port_num;
	int onboard_dev=0;

	int lmac;
	
	if(!strcmp(sfdev_device->subsystem,"usb")) {
		sfdev_usb=sfdev_device;
	}
	else {
		/* get usb node */
		sfdev_usb=sysfs_device_get_parent_with_subsystem(sfdev_device,"usb");
		if(!sfdev_usb) {
			DBG(LOG_ERR,"###rdbudev### no usb parent found");
			goto err;
		}
	}
	
	/* get parent */
	sfdev_gp=sysfs_device_get_parent(sfdev_usb);
	if(!sfdev_usb) {
		DBG(LOG_ERR,"###rdbudev### no parent found");
		goto err;
	}
	
	
	#define read_attr_to(var,devpath,attr,opt) \
		do { \
			var=sysfs_attr_get_value(devpath,attr); \
			if(!opt && !var) { \
				DBG(LOG_ERR,"###rdbudev### missing attr (attr=%s,devpath=%s)",attr,devpath); \
				goto err; \
			} \
		} while(0)
	
	/* read attributes */
	read_attr_to(binterfacenumber,sfdev_usb->devpath,"bInterfaceNumber",0);
	read_attr_to(driver,sfdev_usb->devpath,"driver",0);
	/* read parent attributes */
	read_attr_to(idvendor,sfdev_gp->devpath,"idVendor",0);
	read_attr_to(idproduct,sfdev_gp->devpath,"idProduct",0);
	read_attr_to(busnum,sfdev_gp->devpath,"busnum",0);

	/*
	#ifdef DEBUG
	bp();
	#endif
	*/
	
	/* bypass if the device is in the internal bus */
	
	/*

	When cfg_busnum contains digits only, it is treated as the bus number. However, when it is a string (e.g. 1-2) it is
	treated as an identifier of a USB port through which the internal USB devices (such as WiFi and 3G/LTE modules) connect.
	In particular, this workaround allows to use the external USB port on 30WV/40WV because internal devices always connect
	through internal SMS USB20 hub with port identifier of 1-2"
	
	*/
	
	if(!isdigit_str(cfg_busnum)) {
		bus_port_num=get_bus_port_number(sfdev_gp->devpath);
		if(bus_port_num && !strcmp(cfg_busnum,bus_port_num))
			onboard_dev=1;
	}
	else {
		if(atoi(busnum)==atoi(cfg_busnum)) {
			onboard_dev=1;
		}
	}

	if(onboard_dev) {
		DBG(LOG_DEBUG,"!!! internal device detected - ignored !!!");
		goto err;
	}
	
	read_attr_to(devpath,sfdev_gp->devpath,"devpath",0);
	read_attr_to(bconfigurationvalue,sfdev_gp->devpath,"bConfigurationValue",0);
	/* read optional attributes */
	read_attr_to(serial,sfdev_gp->devpath,"serial",1);
	read_attr_to(address,sfdev->devpath,"address",1);

	/* filter out locally-administered mac */
	lmac=0;
	if(address) {
		int msb;
		if(sscanf(address,"0x02%d",&msb)==1) {
			lmac=((msb&0x02)!=0);
		}
	}

	if(address && lmac)
		snprintf(hd->id,HWDEV_ID_LEN,"%s-%s-%s",idvendor,idproduct,address);
	else if(serial)
		snprintf(hd->id,HWDEV_ID_LEN,"%s-%s-%s",idvendor,idproduct,serial);
	else
		snprintf(hd->id,HWDEV_ID_LEN,"%s-%s",idvendor,idproduct);

	if(address)
		snprintf(hd->mac,HWDEV_MAC_LEN,"%s",address);
	else
		*hd->mac=0;
		
	if(extra_id) {
		strlcat(hd->id,"-",HWDEV_ID_LEN);
		strlcat(hd->id,extra_id,HWDEV_ID_LEN);
	}
	
	snprintf(hd->loc,HWDEV_LOC_LEN,"usb%s-%s:%s.%d",busnum,devpath,bconfigurationvalue,atoi(binterfacenumber));
	if(extra_loc) {
		strlcat(hd->loc,"-",HWDEV_LOC_LEN);
		strlcat(hd->loc,extra_loc,HWDEV_LOC_LEN);
	}
		
	strlcpy(hd->driver,driver,HWDEV_DRIVER_LEN);
	
	
	return 0;
err:
	return -1;
}

int rdbudev_set_usb_hd(enum udev_subsystem udev_subsys,struct hwdev_t* hd,struct sysfs_device* sfdev,struct sysfs_device* sfdev_device) 
{
	return rdbudev_set_usb_hd_with_extra(udev_subsys,hd,sfdev,sfdev_device,NULL,NULL);
}

struct sysfs_device *sysfs_device_get_device(struct sysfs_device *dev)
{
	char sfdev_device_devpath[PATH_SIZE];
	
	strlcpy(sfdev_device_devpath,dev->devpath,sizeof(sfdev_device_devpath));
	strlcat(sfdev_device_devpath,"/",sizeof(sfdev_device_devpath));
	strlcat(sfdev_device_devpath,"device",sizeof(sfdev_device_devpath));
	
	return sysfs_device_get(sfdev_device_devpath);
}

struct sysfs_device *sysfs_device_get_parent_with_subsystems(struct sysfs_device *dev, const char *subsystems[])
{
	struct sysfs_device *dev_parent;
	const char **subsystem;

	dev_parent = sysfs_device_get_parent(dev);
	while (dev_parent != NULL) {
	
		subsystem=subsystems;
		while(*subsystem) {
			if (!strcmp(dev_parent->subsystem, *subsystem))
				return dev_parent;
			subsystem++;
		}
		
		dev_parent = sysfs_device_get_parent(dev_parent);
	}
	return NULL;
}

/*
 * Check whether the device is MMC and has any partitions
 * @sfdev: device to check
 *
 * return: -1: not MMC; 0: MMC and does not have partitions; 1: MMC and has partitions
 */
static int mmc_with_partitions(struct sysfs_device* sfdev)
{
	DIR *dir;
	char path[PATH_SIZE];
	struct sysfs_device* sfdev_mmc;

	/* is this a MMC device? */
	if (!sfdev || !sysfs_device_get_parent_with_subsystem(sfdev,"mmc")) {
		return -1;
	}
	/* try to open sys path of the first partition */
	snprintf(path, sizeof(path), "/sys%s/%sp1", sfdev->devpath,sfdev->kernel);

	dir = opendir(path);
	if (dir) {
		closedir(dir);
		return 1;
	}
	return 0;
}

int rdbudev_set_platform_block_hd(enum udev_subsystem udev_subsys,struct hwdev_t* hd,struct sysfs_device* sfdev,struct sysfs_device* sfdev_device)
{
	const char* partition;
	
	partition=sysfs_attr_get_value(sfdev->devpath,"partition");
	if(!partition) {
		int test_mmc = mmc_with_partitions(sfdev);

		if (test_mmc == -1 || test_mmc == 1) {
			/*
			 * Not a MMC or a MMC device which contains partitions: "goto err" to exit because partitions, which contain "partition" attribute, will come later.
			 */
			DBG(LOG_DEBUG,"###rdbudev### no partition found");
			goto err;
		}
		else {
			/*
			 * If this is a MMC and there aren't any partitions, it may not contain a partition table.
			 * Go ahead to mount entire block device. Mark it like the first partition
			 */
			partition = "1";
		}
	}
	
	return rdbudev_set_platform_hd_with_extra(udev_subsys,hd,sfdev,sfdev,partition,partition);;

err:
	return -1;
}

int rdbudev_set_usb_block_hd(enum udev_subsystem udev_subsys,struct hwdev_t* hd,struct sysfs_device* sfdev,struct sysfs_device* sfdev_device)
{
	const char* partition;
	
	struct sysfs_device* sfdev_usb;
	
	partition=sysfs_attr_get_value(sfdev->devpath,"partition");
	if(!partition) {
		DBG(LOG_DEBUG,"###rdbudev### no partition found");
		goto err;
	}
	
	sfdev_usb=sysfs_device_get_parent_with_subsystem(sfdev,"usb");
	if(!sfdev_usb) {
		DBG(LOG_ERR,"###rdbudev### usb subsystem not found");
		goto err;
	}
	
	return rdbudev_set_usb_hd_with_extra(udev_subsys,hd,sfdev,sfdev_usb,partition,partition);;
err:
	return -1;
}


int rdbudev_set_hd_by_devpath(enum udev_subsystem udev_subsys,struct hwdev_t* hd,const char* devpath)
{
	struct sysfs_device* sfdev;
	struct sysfs_device* sfdev_device;
	//const char* subsystems[]={"usb","platform",NULL};

	int platform;
	int usb;
	int gadget;

	/* get sysfs device */
	sfdev=sysfs_device_get(devpath);
	if(!sfdev) {
		DBG(LOG_ERR,"unable to access '%s'",devpath);
		goto err;
	}

	/* get the first bus */
	sfdev_device=sysfs_device_get_device(sfdev);

	platform=rdbudev_get_platform_parent(sfdev_device?sfdev_device:sfdev)!=NULL;

	gadget=sfdev_device && !strcmp(sfdev_device->kernel,"gadget");

	switch(udev_subsys) {
		case udev_subsystem_block: {

			if(platform) {
				if(rdbudev_set_platform_block_hd(udev_subsys,hd,sfdev,sfdev_device)<0) {
					DBG(LOG_DEBUG,"###rdbudev### failed in rdbudev_set_platform_block_hd()");
					goto err;
				}
			}
			else {
				if(rdbudev_set_usb_block_hd(udev_subsys,hd,sfdev,sfdev_device)<0) {
					DBG(LOG_DEBUG,"###rdbudev### failed in rdbudev_set_usb_block_hd()");
					goto err;
				}
			}

			/* set generic parts of storage hd */
			hd->type=hw_dev_type_storage;
			snprintf(hd->name,HWDEV_NAME_LEN,"/dev/%s",sfdev->kernel);

			break;
		}

		case udev_subsystem_net: {

			if(!sfdev_device) {
				DBG(LOG_DEBUG,"###rdbudev### device not found");
				goto err;
			}

			usb=!strcmp(sfdev_device->subsystem,"usb");
			if(gadget) {
				if(rdbudev_set_gadget_hd(udev_subsys,hd,sfdev,sfdev_device)<0) {
					DBG(LOG_ERR,"###rdbudev### failed in rdbudev_set_gadget_hd()");
					goto err;
				}
			}
			else if(platform) {
				if(rdbudev_set_platform_hd(udev_subsys,hd,sfdev,sfdev_device)<0) {
					DBG(LOG_ERR,"###rdbudev### failed in rdbudev_set_platform_hd()");
					goto err;
				}
			}
			else if(usb) {
				/* set hd by usbsfdev */
				if(rdbudev_set_usb_hd(udev_subsys,hd,sfdev,sfdev_device)<0) {
					DBG(LOG_DEBUG,"###rdbudev### failed in rdbudev_set_usb_hd()");
					goto err;
				}
			}
			else {
				DBG(LOG_ERR,"###rdbudev### unknown subsystem (subsystem=%s)",sfdev_device->subsystem);
				goto err;
			}

			/* set generic parts of serial hd */
			hd->type=hw_dev_type_eth;
			strlcpy(hd->name,sfdev->kernel,HWDEV_NAME_LEN);

			break;
		}

		case udev_subsystem_tty: {

			if(!sfdev_device) {
				DBG(LOG_DEBUG,"###rdbudev### device not found");
				goto err;
			}

			/* get platform id */
			if(gadget) {
				/* set hd by usbsfdev */
				if(rdbudev_set_gadget_hd(udev_subsys,hd,sfdev,sfdev_device)<0) {
					DBG(LOG_DEBUG,"###rdbudev### failed in rdbudev_set_gadget_hd()");
					goto err;
				}
			}
			else if(platform) {
				if(rdbudev_set_platform_hd(udev_subsys,hd,sfdev,sfdev_device)<0) {
					DBG(LOG_ERR,"###rdbudev### failed in rdbudev_set_platform_hd()");
					goto err;
				}
			}
			else if( ( 0 == strcmp(sfdev_device->subsystem,"usb-serial") )
#ifdef V_MODCOMMS
					|| ( 0 == strcmp(sfdev_device->subsystem,"usb") )
#endif
					) {
				/* set hd by usbsfdev */
				if(rdbudev_set_usb_hd(udev_subsys,hd,sfdev,sfdev_device)<0) {
					DBG(LOG_DEBUG,"###rdbudev### failed in rdbudev_set_usb_hd()");
					goto err;
				}
			}
			else {
				DBG(LOG_ERR,"###rdbudev### unknown subsystem (subsystem=%s)",sfdev_device->subsystem);
				goto err;
			}


			/* set generic parts of serial hd */
			hd->type=hw_dev_type_serial;
			snprintf(hd->name,HWDEV_NAME_LEN,"/dev/%s",sfdev->kernel);

			break;
		}

		default:
			goto err;
	}

	/* set generic parts of all hd */
	strlcpy(hd->devpath,devpath,HWDEV_DEVPATH_LEN);

	hwdev_fixup(hd);

	return 0;

err:
	return -1;
}

int rdbudev_remove_hwdev(enum udev_subsystem udev_subsys,const char* devpath)
{
	struct hwdev_t* hd=NULL;
	struct hwcls_t* hc=NULL;
	
	/* get hd */
	hd=hwdev_search_by_devpath(devpath);
	if(!hd) {
		DBG(LOG_DEBUG,"no hwdev found");
		goto err;
	}
	
	/* get hc */
	hc=hd->hwcls;
	if(!hc) {
		DBG(LOG_ERR,"###rdbudev### orphan hwdev detected");
	}
	else {
	
		/* update hwcls status */
		hc->stat=hwcls_stat_removed;
		hwcls_write_to_rdb(hc,0);
		
		/* call script before removing the entry */
		rdbudev_call_script(hc);
		
		hc->hwdev=NULL;
	}
		
		
	/* update to rdb */
	hwcls_write_to_rdb(hc,1);
	hwdev_write_to_rdb(hd,1);
	
	#warning TODO: trigger hwcls entry trigger
	
	/* put hd */
	hwdev_put(hd,1);
		
	return 0;
err:
	return -1;	
}

int rdbudev_call_script(struct hwcls_t* hc)
{
	const char* script;
	char cmd[1024];
	const char* cls_name;
	const char* stat;
	
	const char* type_name;
	int idx;
	
	int res;
	
	/* get script */
	script=rdb_get_value(RDBUDEV_RDB_NOTI_SCRIPT);
	if(!script || !*script) {
		DBG(LOG_ERR,"rdbudev script not specified [%s]",RDBUDEV_RDB_NOTI_SCRIPT);
		goto err;
	}
	
	/* get class name */
	cls_name=hwcls_get_class_name(hc);
	stat=hwcls_get_class_stat(hc);
	type_name=hwcls_get_class_type(hc);
	idx=hc->idx;
	
	/* build command */
	snprintf(cmd,sizeof(cmd),"%s %s%s %s %d %s",script,HWCLS_RDB_PREFIX,cls_name,type_name,idx,stat);
	
	/* call system */
	res=system(cmd);
	if(res==-1)
		goto err;
	
	return WEXITSTATUS(res);
	
err:
	return -1;	
}

int enum_cb_on_build_hwcls_from_rdb(const char* name,int ref)
{
	char* prefix;
	char* p;
	
	struct hwcls_t* hc=NULL;
	
	/* get prefix */
	prefix=strdupa(name);
	p=strstr(prefix,".id");
	if(!p) {
		DBG(LOG_ERR,"hwclass id rdb format is incorrect");
		goto err;
	}
	*p=0;
	
	/* get if not existing */
	hc=hwcls_get();
	if(!hc) {
		DBG(LOG_ERR,"failed in hwcls_get()");
		goto err;
	}

	/* read hc from rdb */
	if(hwcls_read_from_rdb(hc,prefix)<0) {
		DBG(LOG_ERR,"failed in hwcls_read_from_rdb()");
		goto err;
	}
	
	/* fixup hwcls */
	hwcls_fixup(hc,1);
	
	/* update to rdb */
	hwcls_write_to_rdb(hc,0);
	
	return 0;

err:
	if(hc)
		hwcls_put(hc);
		
	/* continue enumeration regardless of error condtiion */
	return 0;	
}

int enum_cb_on_delete_hwdev_rdbs(const char* name,int ref)
{
	rdb_del(name);
	
	return 0;
}

int delete_hwdev_rdbs()
{
	const char* regex;
	
	#warning TODO: use HWDEV_RDB_PREFIX instead
	
	regex="^sys\\.hw\\.dev\\.[0-9]\\+\\..*";
	
	DBG(LOG_INFO,"delete RDBs (rdb=%s,regex=%s)",HWDEV_RDB_PREFIX,regex);
	return rdb_regex_enum(HWDEV_RDB_PREFIX,regex,enum_cb_on_delete_hwdev_rdbs,0);
}

int build_hwcls_from_rdb()
{
	#warning TODO: use HWCLS_RDB_PREFIX instead
	return rdb_regex_enum(HWCLS_RDB_PREFIX,"^sys\\.hw\\.class\\.[^\\.]\\+\\.[0-9]\\+\\.id",enum_cb_on_build_hwcls_from_rdb,0);
}

struct sysfs_class_info_t {
	enum udev_subsystem subsys;
	const char* devpath;
};

struct sysfs_class_info_t sysfs_class_info[]={
	{udev_subsystem_tty,"/class/tty"},
	{udev_subsystem_net,"/class/net"},
	{udev_subsystem_block,"/class/block"},
};

int is_subsystem(const char* devpath,const char* subsystem)
{
	struct sysfs_device* sfdev;
	struct sysfs_device* sfdev_device;
	
	/* get sysfs device */
	sfdev=sysfs_device_get(devpath);
	if(!sfdev) {
		DBG(LOG_ERR,"unable to access '%s'",devpath);
		goto err;
	}
	
	/* get device node */
	sfdev_device=sysfs_device_get_device(sfdev);
	
	return sfdev_device && !strcmp(sfdev_device->subsystem,subsystem);
	
err:
	return 0;	
}

int update_legacy_hwcls()
{
	struct hwcls_t* hc;
	int stat;

	stat=hwcls_get_first(&hc);
	while(stat>=0) {

		if(hc->enable && hc->legacy) {
			/* set inserted flag */
			hc->stat=hwcls_stat_inserted;
			
			/* update to rdb */
			hwcls_write_to_rdb(hc,0);
		
			/* call script - notify */
			rdbudev_call_script(hc);
		}

		stat=hwcls_get_next(&hc);
	}
 
	return 0;
}

int build_hwdev_from_sysfs(const char* subsystem,int exclude)
{
	int i;
	
	char base[PATH_SIZE];
	DIR *dir;
	struct dirent *dent;
	
	char devpath[PATH_SIZE];
	
	int subsystem_matching;
	
	for(i=0;i<COUNTOF(sysfs_class_info);i++) {
		
		snprintf(base, sizeof(base), "%s/%s", sysfs_path,sysfs_class_info[i].devpath);
		
		dir = opendir(base);
		if (!dir) {
			DBG(LOG_ERR,"not able to access sysfs class (base=%s)",base);
			continue;
		}
		
		for (dent = readdir(dir); dent != NULL; dent = readdir(dir)) {

			if (dent->d_name[0] == '.')
				continue;

			snprintf(devpath, sizeof(devpath), "%s/%s", sysfs_class_info[i].devpath, dent->d_name);
			if(sysfs_resolve_link(devpath,sizeof(devpath))<0) {
				DBG(LOG_ERR,"failed in sysfs_resolve_link(devpath=%s) - %s",devpath,strerror(errno));
			}
			else {
				subsystem_matching=is_subsystem(devpath,subsystem);
				
				if(exclude && subsystem_matching) {
					DBG(LOG_DEBUG,"subsystem excluded (subsystem=%s,devpath=%s)",subsystem,devpath);
					continue;
				}
				else if(!exclude && !subsystem_matching) {
					DBG(LOG_DEBUG,"subsystem not matching (subsystem=%s,devpath=%s)",subsystem,devpath);
					continue;
				}

					
				if(rdbudev_add_new_hwdev(sysfs_class_info[i].subsys,devpath)<0) {
					DBG(LOG_DEBUG,"devpath is not a hwdev (devpath=%s)",devpath);
				}
				else {
					DBG(LOG_INFO,"devpath added as a hwdev (devpath=%s)",devpath);
				}
			}
		}
		closedir(dir);
	}

	return 0;
}

#ifdef V_MODCOMMS
static void decorateId(struct hwdev_t* hd)
{
//	DBG(LOG_DEBUG,"mice - loc %s",hd->loc);
	// We're going to look at loc which should be of form  usb1-1.4:1.0
	// From this we will determine the modcomms slot number by counting the dots before the colon
	if (strncmp(hd->loc, "usb1-", 5) != 0) {
		return;
	}
	const char * pCh;
	int dotCnt = 0;
	for (pCh = hd->loc+5; 1; pCh++) {
		if (!*pCh || (*pCh == ':')) {
			break;
		}
		if (*pCh == '.') {
			dotCnt++;
		}
	}
//	DBG(LOG_DEBUG,"mice - slot %d",dotCnt);
	if ((dotCnt <= 0) || (dotCnt >= 100) ) { // Make sure its valid and will only need two digits
		return;
	}
	pCh = hd->id+5; // This should point past the - at the product id
	const char * pMice;
	if (strncmp(pCh,"1001-",5) == 0 ) {
		pMice = "NMA-1400";
	}
	else if (strncmp(pCh,"1002-",5) == 0 ) {
		pMice = "NMA-1500";
	}
	else if (strncmp(pCh,"1003-",5) == 0 ) {
		pMice = "NMA-0001";
	}
	else if (strncmp(pCh,"1004-",5) == 0 ) {
		pMice = "NMA-1300";
	}
	else if (strncmp(pCh,"1005-",5) == 0 ) {
		pMice = "NMA-1200";
	}
	else if (strncmp(pCh,"1007-",5) == 0 ) {
		pMice = "NMA-1500";
	}
	else {
		DBG(LOG_DEBUG,"mice - unrecognised %s",pCh);
		return;
	}
//	DBG(LOG_DEBUG,"mice - recognised %s",pMice);
	pCh += 5; // This should point past the - after the product id
	// We are going to replace the current location ( which was malloced) with a new string
	int origLen = strlen(pCh);
	int newLen = strlen(pMice) + origLen + 4 + 1; // We are going to add ".xx."
	char * pNewBuf = malloc(newLen);
	if (!pNewBuf) {
		return;
	}
	snprintf(pNewBuf,newLen, "%s.%d.%s", pMice, dotCnt, pCh);
//	DBG(LOG_DEBUG,"mice - new id %s",pNewBuf);
	free(hd->id);
	hd->id = pNewBuf;
}
#endif

int rdbudev_add_new_hwdev(enum udev_subsystem udev_subsys,const char* devpath)
{
	struct hwdev_t* hd=NULL;
	struct hwcls_t* hc=NULL;

	int already_inserted;
	
	/* get hd */
	hd=hwdev_get();
	if(!hd) {
		DBG(LOG_ERR,"failed in hwdev_get()");
		goto err;
	}
	
 	#if DEBUG
	if(strstr(devpath,"ttyGS0")) {
 		bp();
 	}
 	#endif

	/* fill up hd */
	if(rdbudev_set_hd_by_devpath(udev_subsys,hd,devpath)<0) {
		DBG(LOG_DEBUG,"failed in rdbudev_set_hd_by_devpath()");
		goto err;
	}

#ifdef V_MODCOMMS
	if (hd->type == hw_dev_type_serial) {
		// Does the id start with Netcomm's vendor Id?
		if (strncmp(hd->id,"220e-",5) == 0 ) {
			decorateId(hd);
		}
	}
#endif

	/* search hwcls */
	if(hwcls_search_by_id_and_loc(hd,&hc)<0) {

		DBG(LOG_DEBUG,"* new device found");
		DBG(LOG_DEBUG,"name    : %s",hd->name);
		DBG(LOG_DEBUG,"loc     : %s",hd->loc);
		DBG(LOG_DEBUG,"id      : %s",hd->id);
		DBG(LOG_DEBUG,"devpath : %s",hd->devpath);
		
		/* get if not existing */
		hc=hwcls_get();
		if(!hc) {
			DBG(LOG_ERR,"failed in hwcls_get()");
			goto err;
		}
	}

	/* check if the device is already inserted */
	already_inserted=hc->stat==hwcls_stat_inserted;

#if 0
	#if DEBUG
	if(already_inserted)
		bp();
	#endif
#endif	
	
	/* log detail of the device */
	if(already_inserted) {
		DBG(LOG_WARNING,"device already inserted - possibly missing udev message");

		DBG(LOG_WARNING,"* detail of duplicated device");
		DBG(LOG_WARNING,"name    : %s",hd->name);
		DBG(LOG_WARNING,"loc     : %s",hd->loc);
		DBG(LOG_WARNING,"id      : %s",hd->id);
		DBG(LOG_WARNING,"devpath : %s",hd->devpath);

		/*
			if the device already exists, call the script to remove first

			* description
			The system can see the same device inserted at startup due to the race condtion of startup sequence.
			This startup sequence is to avoid missing udev messages but it can process the same device twice

			* startup sequence
			1. start listening to kernel udev message
			2. manual scan
			3. process kernel udev message

			If a device appears after #1 but before the system finishes "#2", the device will appear in #3 again.
			This is the case that a device appears twice.
		*/

		DBG(LOG_WARNING,"remove the previously inserted device (name:'%s')",hd->name);

		hc->stat=hwcls_stat_removed;
		hwcls_write_to_rdb(hc,0);

		/* call script to remove */
		rdbudev_call_script(hc);
		
	}
	
	/* update hwcls */
	strlcpy(hc->id,hd->id,HWDEV_ID_LEN);
	strlcpy(hc->loc,hd->loc,HWDEV_LOC_LEN);
	/* setup a cross link between hwdev and hwcls */
	hc->hwdev=hd;
	hd->hwcls=hc;
	
	/* convert hwdev type to hwclass type */
	switch(hd->type) {
		case hw_dev_type_serial:
			hc->type=hwcls_type_serial;
			break;
			
		case hw_dev_type_eth:
			hc->type=hwcls_type_eth;
			break;
			
		case hw_dev_type_storage:
			hc->type=hwcls_type_storage;
			break;
			
		default:
			hc->type=hwcls_type_unknown;
			DBG(LOG_ERR,"###rdbudev### unknown hwdev type (hd->type=%d)",hd->type);
			break;
	}

	/* set inserted flag */
	hc->stat=hwcls_stat_inserted;
	
	/* fixup hwcls */
	hwcls_fixup(hc,0);
	
	/* update to rdb */
	hwdev_write_to_rdb(hd,0);
	hwcls_write_to_rdb(hc,0);
	
	DBG(LOG_DEBUG,"* update class info");
	DBG(LOG_DEBUG,"name    : %s",hd->name);
	DBG(LOG_DEBUG,"idx     : %d",hc->idx);
	DBG(LOG_DEBUG,"loc     : %s",hd->loc);
	DBG(LOG_DEBUG,"id      : %s",hd->id);
	DBG(LOG_DEBUG,"devpath : %s",hd->devpath);
	
	/* call script - notify */
	rdbudev_call_script(hc);
	
	return 0;
	
	
err:
	/* put back hd */
	if(hd)
		hwdev_put(hd,0);
	if(hc)
		hwcls_put(hc);
		
	return -1;
}

static int uevent_netlink_sock = -1;
static int udev_monitor_sock = -1;
static volatile int udev_exit;

static int udev_init_mon_sck(void)
{
	struct sockaddr_un saddr;
	socklen_t addrlen;
	const int feature_on = 1;
	int retval;

	memset(&saddr, 0x00, sizeof(saddr));
	saddr.sun_family = AF_LOCAL;
	/* use abstract namespace for socket path */
	strcpy(&saddr.sun_path[1], "/org/kernel/udev/monitor");
	addrlen = offsetof(struct sockaddr_un, sun_path) + strlen(saddr.sun_path+1) + 1;

	udev_monitor_sock = socket(AF_LOCAL, SOCK_DGRAM, 0);
	if (udev_monitor_sock == -1) {
		fprintf(stderr, "error getting socket: %s\n", strerror(errno));
		return -1;
	}

	/* the bind takes care of ensuring only one copy running */
	retval = bind(udev_monitor_sock, (struct sockaddr *) &saddr, addrlen);
	if (retval < 0) {
		fprintf(stderr, "bind failed: %s\n", strerror(errno));
		close(udev_monitor_sock);
		udev_monitor_sock = -1;
		return -1;
	}

	/* enable receiving of the sender credentials */
	setsockopt(udev_monitor_sock, SOL_SOCKET, SO_PASSCRED, &feature_on, sizeof(feature_on));

	return 0;
}

static int udev_init_netlink_sck(void)
{
	struct sockaddr_nl snl;
	int retval;

	memset(&snl, 0x00, sizeof(struct sockaddr_nl));
	snl.nl_family = AF_NETLINK;
	snl.nl_pid = getpid();
	snl.nl_groups = 1;

	uevent_netlink_sock = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
	if (uevent_netlink_sock == -1) {
		fprintf(stderr, "error getting socket: %s\n", strerror(errno));
		return -1;
	}

	retval = bind(uevent_netlink_sock, (struct sockaddr *) &snl,
		      sizeof(struct sockaddr_nl));
	if (retval < 0) {
		fprintf(stderr, "bind failed: %s\n", strerror(errno));
		close(uevent_netlink_sock);
		uevent_netlink_sock = -1;
		return -1;
	}

	return 0;
}

static void sig_handler(int signum)
{
	if (signum == SIGINT || signum == SIGTERM)
		udev_exit = 1;
}

static const char *udev_search_key(const char *searchkey, const char *buf, size_t buflen)
{
	size_t bufpos = 0;
	size_t searchkeylen = strlen(searchkey);

	while (bufpos < buflen) {
		const char *key;
		int keylen;

		key = &buf[bufpos];
		keylen = strlen(key);
		if (keylen == 0)
			break;
		 if ((strncmp(searchkey, key, searchkeylen) == 0) && key[searchkeylen] == '=')
			return &key[searchkeylen + 1];
		bufpos += keylen + 1;
	}
	return NULL;
}


void print_usage(FILE* fp)
{
	fprintf(fp,
		"rdb udev manager\n\n"

		"description:\n"
		"\tThis daemon converts UDEV kernel messages into [sys.hw.class.] RDBs\n"
		"\n"
		"command line options:\n"
		"\t-h print this help screen\n\n"
		
		"RDB options:\n"
		"\t[sys.hw.cfg.hwclass_script] : RDB udev event script\n"
		"\t\tscript.sh <RDB Class entry prefix> <device type> <index of RDB Class entry> <inserted|removed>\n"
		"\t\t# $1 rdb prefix - sys.hw.class.<device type>.<n>\n"
		"\t\t# $2 class type (device type) <serial|eth|storage|phonemodule>\n"
		"\t\t# $3 index of RDB Device class <n>\n"
		"\t\t# $4 status <removed|inserted>\n"
		"\n"
		"\t[sys.hw.cfg.debug] : debug level - only taken at startup (1-7, default:4)\n\n"

		"RDBs \n"
		"\t[sys.hw.dev.<index>] : volatile sys device tree\n"
		"\t[sys.hw.class.<device type>.<index>] : persistant device tree\n"
		"\n"
	);

}

int main(int argc,char* argv[])
{
	struct sigaction act;
	int env = 0;
	int kernel = 0;
	int udev = 0;
	fd_set readfds;
	int retval = 0;
	
	const char* debug_str;
	int logmask;

	enum udev_subsystem udev_subsys;
	enum udev_action udev_act;

	int opt;

	/* get options */
	while ((opt = getopt(argc, argv, "h")) != EOF) {
		switch(opt) {
			case 'h':
				print_usage(stdout);
				exit(0);
				break;

			case ':':
				fprintf(stderr,"missing argument - %c\n",opt);
				print_usage(stderr);
				exit(-1);
				break;

			case '?':
				fprintf(stderr,"unknown option - %c\n",opt);
				print_usage(stderr);
				exit(-1);
				break;

			default:
				print_usage(stderr);
				exit(-1);
				break;
		}
	}

	/* init. local members */
	memset(platform_ids,0,sizeof(platform_ids));
	
	/* init. rdb-udev components */
	rdb_init();
	
	/* init log */
	openlog("rdb_udevd",LOG_PID,LOG_DAEMON);
	debug_str=rdb_get_value("sys.hw.cfg.debug");
	
	/* set initial log mask to DEBUG */
	logmask=LOG_UPTO(debug_str?atoi(debug_str):LOG_WARNING);
	setlogmask(logmask);

	DBG(LOG_ERR,"rdb_udevd daemon starts (%s %s)",__DATE__,__TIME__);

	DBG(LOG_ERR,    "[loglevel-check] LOG_ERR");
	DBG(LOG_WARNING,"[loglevel-check] LOG_WARNING");
	DBG(LOG_NOTICE, "[loglevel-check] LOG_NOTICE");
	DBG(LOG_INFO,   "[loglevel-check] LOG_INFO");
	DBG(LOG_DEBUG,  "[loglevel-check] LOG_DEBUG");
	
	/* init. udev components */
	DBG(LOG_INFO,"initiate sysfs module");
	sysfs_init();

	DBG(LOG_INFO,"initiate hwdev module");
	hwdev_init();
	DBG(LOG_INFO,"initiate hwcls module");
	hwcls_init();
	
	/* delete hwdev in rdb */
	DBG(LOG_INFO,"delete existing hwdev RDBs");
	delete_hwdev_rdbs();
	
	/* read hwclass from rdb */
	DBG(LOG_INFO,"build hwclass objects from RDB");
	build_hwcls_from_rdb();
	
	if (!kernel && !udev) {
		kernel = 1;
		udev =1;
	}

	if (getuid() != 0 && kernel) {
		fprintf(stderr, "root privileges needed to subscribe to kernel events\n");
		goto out;
	}

	/* set signal handlers */
	DBG(LOG_INFO,"setup signal handlers");
	memset(&act, 0x00, sizeof(struct sigaction));
	act.sa_handler = (void (*)(int)) sig_handler;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_RESTART;
	sigaction(SIGINT, &act, NULL);
	sigaction(SIGTERM, &act, NULL);

	if (udev) {
		DBG(LOG_INFO,"initiate monitor socket");
		retval = udev_init_mon_sck();
		if (retval)
			goto out;
	}
	if (kernel) {
		DBG(LOG_INFO,"initiate netlink socket");
		retval = udev_init_netlink_sck();
		if (retval)
			goto out;
	}
	
	#if 0
 	#if DEBUG
 	bp();
 	#endif
	#endif
		
	/* search platform built-in devices first  */
	DBG(LOG_INFO,"scan platform devices");
	build_hwdev_from_sysfs("platform",0);
	/* update all legacy devices */
	DBG(LOG_INFO,"build legacy devices");
	update_legacy_hwcls();
	/* search all other devices  */
	DBG(LOG_INFO,"scan devices");
	build_hwdev_from_sysfs("platform",1);

	while (!udev_exit) {
		char buf[UEVENT_BUFFER_SIZE*2];
		ssize_t buflen;
		ssize_t bufpos;
		ssize_t keys;
		int fdcount;
		struct timeval tv;
		struct timezone tz;
		char timestr[64];
		const char *source = NULL;
		const char *devpath, *action, *subsys;
		
		/* init sysfs */
		sysfs_cleanup();
		sysfs_init();

		buflen = 0;
		
		/* set fds */
		FD_ZERO(&readfds);
		if (uevent_netlink_sock >= 0)
			FD_SET(uevent_netlink_sock, &readfds);
		if (udev_monitor_sock >= 0)
			FD_SET(udev_monitor_sock, &readfds);

		/* select */
		fdcount = select(UDEV_MAX(uevent_netlink_sock, udev_monitor_sock)+1, &readfds, NULL, NULL, NULL);
		if (fdcount < 0) {
			if (errno != EINTR)
				DBG(LOG_ERR, "error receiving uevent message: %s\n", strerror(errno));
			continue;
		}
		
		DBG(LOG_INFO,"select() triggered");

		if (gettimeofday(&tv, &tz) == 0) {
			snprintf(timestr, sizeof(timestr), "%llu.%06u",
				 (unsigned long long) tv.tv_sec, (unsigned int) tv.tv_usec);
		} else
			timestr[0] = '\0';

		/* recieve kernel notification */
		if ((uevent_netlink_sock >= 0) && FD_ISSET(uevent_netlink_sock, &readfds)) {
			buflen = recv(uevent_netlink_sock, &buf, sizeof(buf), 0);
			if (buflen <= 0) {
				DBG(LOG_ERR, "error receiving uevent message: %s\n", strerror(errno));
				continue;
			}
			source = "UEVENT";
			
			DBG(LOG_INFO,"recieved from netlink socket");
		}

		/* recieve udev notificaiton */
		if ((udev_monitor_sock >= 0) && FD_ISSET(udev_monitor_sock, &readfds)) {
			buflen = recv(udev_monitor_sock, &buf, sizeof(buf), 0);
			if (buflen <= 0) {
				DBG(LOG_ERR, "error receiving udev message: %s\n", strerror(errno));
				continue;
			}
			source = "UDEV  ";
			
			DBG(LOG_INFO,"recieved from monitor socket");
		}

		if (buflen == 0)
			continue;

		/* get keys */
		keys = strlen(buf) + 1; /* start of payload */
		devpath = udev_search_key("DEVPATH", &buf[keys], buflen);
		action = udev_search_key("ACTION", &buf[keys], buflen);
		subsys = udev_search_key("SUBSYSTEM", &buf[keys], buflen);
		printf("%s[%s] %-8s %s (%s)\n", source, timestr, action, devpath, subsys);
		
		DBG(LOG_INFO,"%s[%s] %-8s %s (%s)\n", source, timestr, action, devpath, subsys);
		

		/* print environment */
		bufpos = keys;
		if (env) {
			while (bufpos < buflen) {
				int keylen;
				char *key;

				key = &buf[bufpos];
				keylen = strlen(key);
				if (keylen == 0)
					break;
				printf("%s\n", key);
				bufpos += keylen + 1;
			}
			printf("\n");
		}

		int stat;
		
		/* get action and subsystem id */
		udev_subsys=get_subsystem_id(subsys);
		udev_act=get_action_id(action);
		
		/* process add or remove action */
		switch(udev_act) {
			case udev_action_add:
				DBG(LOG_DEBUG,"###rdbudev### start to add");
				
				/* add hwdev */
				stat=rdbudev_add_new_hwdev(udev_subsys,devpath);
				if(stat<0) {
					DBG(LOG_DEBUG,"###rdbudev### no device added");
					break;
				}
				
				DBG(LOG_INFO,"###rdbudev### device added (devpath=%s)",devpath);
				break;
				
			case udev_action_remove:
				DBG(LOG_DEBUG,"###rdbudev### start to remove");
				
				stat=rdbudev_remove_hwdev(udev_subsys,devpath);
				if(stat<0) {
					DBG(LOG_DEBUG,"###rdbudev### no device removed");
					break;
				}
				
				DBG(LOG_INFO,"###rdbudev### device removed (devpath=%s)",devpath);
				break;
				
			case udev_action_change: {
				DBG(LOG_DEBUG,"###rdbudev### start to change");
				
				/* ignore for now */
				
				break;
			}
				
			default:
				DBG(LOG_ERR,"###rdbudev### unknown action found (action=%s)",action);
				break;
		}
			
	}

out:
	if (uevent_netlink_sock >= 0) {
		DBG(LOG_INFO,"close netlink socket");
		close(uevent_netlink_sock);
	}
	
	if (udev_monitor_sock >= 0) {
		DBG(LOG_INFO,"close monitor socket");
		close(udev_monitor_sock);
	}

#if 0
	if (retval)
		return 1;
	return 0;
#endif
	
	
	
	/* add new hwdev's by searching other bus devices */
	
	
	/* listen to Kernel udev notification */
	
	
	/* fini. rdb-udev components */
	DBG(LOG_INFO,"close hwclass module");
	hwcls_fini();
	DBG(LOG_INFO,"close hwdev module");
	hwdev_fini();
	
	/* fini. udev components */
	DBG(LOG_INFO,"close sysfs module");
	sysfs_cleanup();
	
	DBG(LOG_INFO,"close RDB module");
	rdb_fini();
	
	return 0;
}

