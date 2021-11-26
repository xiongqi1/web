#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>

#include "mxml.h"

struct apn {
	mxml_node_t* root;
	mxml_node_t* branch;
	mxml_node_t* country;
	mxml_node_t* carrier;
	mxml_node_t* apn;
};

#define NODE_ELEMENT_NAME(x)	(((x)->value).element.name)
#define NODE_ATTRIB(x,a)	mxmlElementGetAttr(x,a)

static mxml_node_t* base_tree=0;
static char* cmdname=0;
	

int find_apn(struct apn* apn)
{
	// clear info.
	apn->branch=apn->country=apn->carrier=0;
	
	// find
	apn->apn=mxmlFindElement(apn->apn,apn->root,"APN",0,0,MXML_DESCEND);
	
	if(apn->apn)
		apn->carrier=apn->apn->parent;
	if(apn->carrier)
		apn->country=apn->carrier->parent;
	if(apn->country)
		apn->branch=apn->country->parent;
	
	return apn->apn && apn->carrier && apn->country && apn->branch;
}

int check_sanity(mxml_node_t* root_tree)
{
	int succ;
	struct apn apn;
	
	succ=1;
	
	// init. apn
	memset(&apn,0,sizeof(apn));
	apn.root=apn.apn=root_tree;
	
	while(find_apn(&apn)) {
		
		if(!NODE_ATTRIB(apn.country,"country")) {
			fprintf(stderr,"country attribute missing - %s\n",NODE_ATTRIB(apn.apn,"apn"));
			succ=0;
		}
		
		if(!NODE_ATTRIB(apn.country,"mcc")) {
			fprintf(stderr,"mcc attribute missing - %s\n",NODE_ATTRIB(apn.apn,"apn"));
			succ=0;
		}
		
		if(!NODE_ATTRIB(apn.carrier,"carrier")) {
			fprintf(stderr,"carrier attribute missing - %s\n",NODE_ATTRIB(apn.apn,"apn"));
			succ=0;
		}
			
		if(!NODE_ATTRIB(apn.carrier,"mnc")) {
			fprintf(stderr,"mnc attribute missing - %s\n",NODE_ATTRIB(apn.apn,"apn"));
			succ=0;
		}
		
		if(!NODE_ATTRIB(apn.apn,"apn")) {
			fprintf(stderr,"apn attribute missing - %s\n",NODE_ATTRIB(apn.carrier,"carrier"));
			succ=0;
		}
	}
	
	return succ;
}

int add_apn(struct apn* apn)
{
	int add;
	const char* include_name;
	
	const char* mnc_list;
	const char* new_mnc_list;
	
	int found;
	
	struct apn base_apn;
	struct apn prev_apn;
	
	mxml_node_t* child_node;
	mxml_node_t* top_node;
	
	struct apn match_apn;
	
	mxml_node_t* rt_node;
	mxml_node_t* detail_node;
	mxml_node_t* new_detail_node;
	
	const char* branch_name;
	
	const char* apn_country;
	const char* apn_mcc;
	const char* apn_carrier;
	const char* apn_mnc;
	const char* apn_apn;
	
	apn_country=NODE_ATTRIB(apn->country,"country");
	apn_mcc=NODE_ATTRIB(apn->country,"mcc");
	apn_carrier=NODE_ATTRIB(apn->carrier,"carrier");
	apn_mnc=NODE_ATTRIB(apn->carrier,"mnc");
	apn_apn=NODE_ATTRIB(apn->apn,"apn");
	
	if(!apn_country || !apn_mcc || !apn_carrier || !apn_mnc || !apn_apn) {
		fprintf(stderr,"missing attribute detected\n");
		return -1;
	}
	
	branch_name=NODE_ELEMENT_NAME(apn->branch);
	// get include or exclude
	if(!strcmp(branch_name,"Include")) {
		add=1;
	}
	else if(!strcmp(branch_name,"Exclude")) {
		add=0;
	}
	else {
		fprintf(stderr,"branch tag not found - %s\n",NODE_ATTRIB(apn->apn,"apn"));
		return -1;
	}
	
	found=0;
	
	// init. each apn
	base_apn.root=base_apn.apn=base_tree;
	memcpy(&prev_apn,&base_apn,sizeof(prev_apn));
	
	// for each apn in base apn
	while(find_apn(&base_apn)) {
		
		// skip if country not matched
		if(strcmp(apn_country,NODE_ATTRIB(base_apn.country,"country"))) {
		}
		// skip if mcc
		else if(strcmp(apn_mcc,NODE_ATTRIB(base_apn.country,"mcc"))) {
		}
		// skip if apn carrier not matched
		else if(strcmp(apn_carrier,NODE_ATTRIB(base_apn.carrier,"carrier"))) {
		}
		// get mnc list
		else if(strcmp(apn_mnc,NODE_ATTRIB(base_apn.carrier,"mnc"))) {
		}
		// skip if apn not matched
		else if(strcmp(apn_apn,NODE_ATTRIB(base_apn.apn,"apn"))) {
		}
		else {
			found++;
			
			if(add) {
			}
			else {
				// get empty top tree
				top_node=base_apn.apn;
				int e;
				
				while(top_node && top_node->parent) {
					
					// search any silibing element
					e=0;
					child_node=top_node->parent->child;
					while(child_node) {
						if(child_node->type==MXML_ELEMENT)
							e++;
						child_node=child_node->next;
					}
					
					if(e>=2)
						break;
					
					top_node=top_node->parent;
				}
				
						
				// delete
				mxmlDelete(top_node);
				
				// restore
				memcpy(&base_apn,&prev_apn,sizeof(base_apn));
			}
		}
				
		memcpy(&prev_apn,&base_apn,sizeof(base_apn));
	}
	
	// warning - duplicated apn already exists
	if(add && found)
		fprintf(stderr,"the apn(%s) in include already exists\n",NODE_ATTRIB(apn->apn,"apn"));
	
	// warning - missing apn
	if(!add && !found)
		fprintf(stderr,"the apn(%s) in exclude does not exists\n",NODE_ATTRIB(apn->apn,"apn"));
	
	// bypass if done
	if(!add || (add && found))
		return 0;
	
	// add
	memset(&match_apn,0,sizeof(match_apn));
		
	// find rt node
	rt_node=mxmlFindElement(base_tree,base_tree,"Rt",0,0,MXML_DESCEND);
	if(!rt_node) {
		fprintf(stderr,"Rt node does not exist in base xml\n");
		exit(-1);
	}
	
	// find country
	match_apn.country=mxmlFindElement(base_tree,base_tree,"Country","mcc",apn_mcc,MXML_DESCEND);
	if(!match_apn.country) {
		match_apn.country=mxmlNewElement(rt_node,"Country");
		mxmlElementSetAttr(match_apn.country,"country",apn_country);
		mxmlElementSetAttr(match_apn.country,"mcc",apn_mcc);
		mxmlNewText(match_apn.country,1,"");
	}
	
	// find carrier
	match_apn.carrier=mxmlFindElement(match_apn.country,base_tree,"Carrier","mnc",apn_mnc,MXML_DESCEND_FIRST);
	if(!match_apn.carrier) {
		match_apn.carrier=mxmlNewElement(match_apn.country,"Carrier");
		mxmlElementSetAttr(match_apn.carrier,"carrier",apn_carrier);
		mxmlElementSetAttr(match_apn.carrier,"mnc",apn_mnc);
		mxmlNewText(match_apn.carrier,1,"");
	}		
	
	// create apn
	match_apn.apn=mxmlNewElement(match_apn.carrier,"APN");
	mxmlElementSetAttr(match_apn.apn,"apn",apn_apn);
	mxmlNewText(match_apn.apn,1,"");
			
	// create login detail
	detail_node=apn->apn->child;
	while(detail_node) {
		
		if((detail_node->type==MXML_ELEMENT) && detail_node->child && (detail_node->child->type==MXML_TEXT)) {
			new_detail_node=mxmlNewElement(match_apn.apn,NODE_ELEMENT_NAME(detail_node));
			mxmlNewText(new_detail_node,0,(detail_node->child->value).text.string);
			mxmlNewText(match_apn.apn,1,"");
		}
		
		detail_node=detail_node->next;
	}
	
	mxmlNewText(match_apn.carrier,1,"");
	
	return 0;
}

int put_addon(mxml_node_t* addon_xml)
{
	struct apn apn;
	
	// init. each apn
	apn.root=apn.apn=addon_xml;
	
	// for each apn in an addon xml
	while(find_apn(&apn)) {
		if(add_apn(&apn)<0)
			return -1;
	}
	
	return 0;
}


void print_usage(FILE* fp)
{
	fprintf(fp,
		"Usage:\n"
		"\t %s -b <base.xml> [-o output.xml] addon1.xml...\n"
		"\n"
		"Description:\n"
		"\t Merge auto-apn xml files into a single xml v1.0\n"
		"\n"
		"Options:\n"
		"\t -b \t base xml file\n"
		"\t -o \t output xml file\n"
		"\t addon.xml \t the file contains include or exclude APNs\n"
		"\n", 
  		cmdname);
}

int main(int argc,char* argv[])
{
	const char* base_file=0;
	const char** addon_files=0;
	const char* output_file=0;
	const char** addon_file;
	
	FILE* output_fp;
	FILE* base_fp;
	FILE* addon_fp;
	
	mxml_node_t* addon_xml;
	
	int opt;
	int addon_file_count;
	
	int i;
	
	cmdname=basename(strdup(argv[0]));
	
	// parse command line
	while((opt=getopt(argc,argv,"b:o:h"))!=EOF) {
		switch(opt) {
			
			case 'b':
				base_file=optarg;
				break;
				
			case 'o':
				output_file=optarg;
				break;
				
			case 'h':
				print_usage(stdout);
				exit(-1);
				break;
				
			case '?':
				print_usage(stderr);
				exit(-1);
				break;
				
			default:
				fprintf(stderr,"%s: unknown error - %c\n",cmdname,opt);
				print_usage(stderr);
				exit(-1);
				break;
		}
	}
	
	// check parameter - base.xml
	if(!base_file) {
		fprintf(stderr,"%s: base.xml is not specified\n",cmdname);
		print_usage(stderr);
		exit(-1);
	}
	
	// check parameter - addon.xml
	addon_file_count=argc-optind;
	if(addon_file_count<=0) {
		fprintf(stderr,"%s: no addon xml is specified\n",cmdname);
		print_usage(stderr);
		exit(-1);
	}
	
	
	// set addon files
	addon_files=malloc((addon_file_count+1)*sizeof(*addon_files));
	
	i=0;
	while(i<addon_file_count) {
		addon_files[i]=argv[i+optind];
		i++;
	}
	addon_files[i]=0;
		
	
	// open output file
	output_fp=stdout;
	if(output_file) {
		output_fp=fopen(output_file,"w+");
		if(!output_fp) {
			fprintf(stderr,"cannot open %s - %s\n",output_file,strerror(errno));
			exit(-1);
		}
	}
	
	// open base file
	base_fp=fopen(base_file,"r");
	if(!base_fp) {
		fprintf(stderr,"cannot open %s - %s\n",base_file,strerror(errno));
		exit(-1);
	}
	
	
	// open base tree
	base_tree=mxmlLoadFile(0,base_fp,MXML_NO_CALLBACK);
	if(!base_tree) {
		fprintf(stderr,"cannot load xml - %s\n",base_file);
		exit(-1);
	}
	
	if(!check_sanity(base_tree)) {
		fprintf(stderr,"xml error in %s file",base_file);
		exit(-1);
	}
	
	// loop for addon files
	addon_file=addon_files;
	while(*addon_file) {
		
		// open addon file
		addon_fp=fopen(*addon_file,"r");
		if(!addon_fp) {
			fprintf(stderr,"cannot open %s - %s\n",*addon_file,strerror(errno));
			exit(-1);
		}
				
		// load xml
		addon_xml=mxmlLoadFile(0,addon_fp,MXML_NO_CALLBACK);
		if(!addon_xml) {
			fprintf(stderr,"cannot load xml - %s\n",*addon_file);
			exit(-1);
		}
		
		if(!check_sanity(addon_xml)) {
			fprintf(stderr,"xml error in %s file",*addon_file);
			exit(-1);
		}
		
		// apply addon xml
		if(put_addon(addon_xml)<0) {
			fprintf(stderr,"fail to add addon xml (%s)\n",*addon_file);
			exit(-1);
		}
		
		// delete and close addon file
		mxmlDelete(addon_xml);
		fclose(addon_fp);
		
		addon_file++;
	}
	
	mxmlSetWrapMargin(0);
		
	if(mxmlSaveFile(base_tree,output_fp,MXML_NO_CALLBACK)<0) {
		fprintf(stderr,"fail to write into output file (%s)\n",output_file);
		exit(-1);
	}
	
	// delete base tree
	mxmlDelete(base_tree);
	// close base file
	fclose(base_fp);
	
	// close output
	if(output_file)
		fclose(output_fp);
	
	free(addon_files);
	
	return 0;
}
