#include <string.h>
#include <syslog.h>

#include "endian.h"
#include "qmimsg.h"

static void qmitlv_destroy(struct qmitlv_t* tlv)
{
	if(!tlv)
		return;

	_free(tlv->v);
	_free(tlv);
}

static int qmitlv_set(struct qmitlv_t* tlv, unsigned char t,unsigned short l,const void* v)
{
	if(l) {
		// reallocate v if bigger
		if((tlv->v==NULL) || (l>tlv->v_len)) {

			_free(tlv->v);

			tlv->v_len=((l+QMIMSG_TLV_BLOCK-1)/QMIMSG_TLV_BLOCK)*QMIMSG_TLV_BLOCK;

			tlv->v=(void*)_malloc(tlv->v_len);
			if(!tlv->v) {
				SYSLOG(LOG_ERROR,"failed to allocate value memory - size=%d",tlv->v_len);
				goto err;
			}
		}

		memcpy(tlv->v,v,l);
	}

	tlv->t=t;
	tlv->l=l;

	return 0;
err:
	return 	-1;
}

static struct qmitlv_t* qmitlv_create(void)
{
	struct qmitlv_t* tlv;

	// create object
	tlv=_malloc(sizeof(struct qmitlv_t));
	if(!tlv) {
		SYSLOG(LOG_ERROR,"failed to allocate qmitlv_t - size=%d",sizeof(struct qmitlv_t));
		goto err;
	}

	return tlv;
err:
	qmitlv_destroy(tlv);
	return NULL;
}

static int qmimsg_get_phys_payload_length(struct qmimsg_t* msg)
{
	int total_len;
	int i;

	total_len=0;

	// add tlv headers
	total_len+=sizeof(struct qmi_phys_tlv_hdr_t)*msg->tlv_count;
	// add values
	i=0;
	while(i<msg->tlv_count)
		total_len+=msg->tlvs[i++]->l;

	return total_len;
}

int qmimsg_get_total_phy_length(struct qmimsg_t* msg)
{
	return sizeof(struct qmi_phys_msg_hdr_t)+qmimsg_get_phys_payload_length(msg);
}

int qmimsg_read_from_buf(struct qmimsg_t* msg, const void* buf,int buf_len)
{
	struct qmi_phys_msg_hdr_t* phys_msg;
	struct qmi_phys_tlv_hdr_t* phys_tlv;
	struct qmitlv_t* tlv;

	int i;
	int payload_len;


	unsigned char t;
	unsigned short l;

	int read_len;

	if(buf_len<sizeof(*phys_msg)) {
		SYSLOG(LOG_ERROR,"invalid qmi message packet detected (too small) - buf_len=%d",buf_len);
		goto err;
	}

	// get qmi trans header
	phys_msg=(struct qmi_phys_msg_hdr_t*)buf;
	// set read point
	read_len=sizeof(*phys_msg);

	SYSLOG(LOG_DEBUG,"read_len=%d",read_len);

	// check payload length
	payload_len=read16_from_little_endian(phys_msg->msg_len);
	// get msg id
	msg->msg_id=read16_from_little_endian(phys_msg->msg_id);

	if(buf_len-read_len<payload_len) {
		SYSLOG(LOG_ERROR,"invalid qmi message packet detected (length not mached) - buf_len=%d,payload_len=%d,msg_id=0x%04x",buf_len,payload_len,msg->msg_id);
		goto err;
	}

	// reset tvl count
	msg->tlv_count=0;

	// extract qmi tlvs
	i=0;
	while(read_len<payload_len+sizeof(*phys_msg)) {

		// err if exceed max tlvs
		if(i>=QMIMSG_MAX_TLVS_COUNT) {
			SYSLOG(LOG_ERROR,"exceed max TLV - max=%d",QMIMSG_MAX_TLVS_COUNT);
			goto err;
		}

		// check if buf_len contains at least a tlv
		if(buf_len<read_len+sizeof(*phys_tlv)) {
			SYSLOG(LOG_ERROR,"incompleted TLV found at the end");
			goto err;
		}

		// read tl
		phys_tlv=(struct qmi_phys_tlv_hdr_t*)((char*)phys_msg+read_len);
		t=phys_tlv->t;
		l=read16_from_little_endian(phys_tlv->l);
		read_len+=sizeof(*phys_tlv);

		// check if buf_len contains value
		if(buf_len<l+read_len) {
			SYSLOG(LOG_ERROR,"incompleted value found in TLV at the end");
			goto err;
		}

		// get tlv - create tlv if not existing
		tlv=msg->tlvs[i];
		if(!tlv) {
			tlv=msg->tlvs[i]=qmitlv_create();
			if(!tlv)
				goto err;
		}

		SYSLOG(LOG_DEBUG,"reading TLV t=0x%02x,l=%d",t,l);

		// set tlv
		if( qmitlv_set(tlv,t,l,phys_tlv->v)<0 )
			goto err;
		read_len+=read16_from_little_endian(phys_tlv->l);
		i++;
	}

	msg->tlv_count=i;

	return read_len;
err:
	return -1;
}

int qmimsg_write_to_buf(struct qmimsg_t* msg, void* buf,int buf_len)
{
	int length;
	int total;
	int i;

	struct qmi_phys_msg_hdr_t* phys_msg;
	struct qmi_phys_tlv_hdr_t* phys_tlv;
	struct qmitlv_t* tlv;

	// check total length
	total=qmimsg_get_total_phy_length(msg);
	if(buf_len<total) {
		SYSLOG(LOG_ERROR,"buffer size insufficient - buf_len=%d,total=%d",buf_len,total);
		goto err;
	}

	// get payload length
	length=qmimsg_get_phys_payload_length(msg);
	if(length>0xffff) {
		SYSLOG(LOG_ERROR,"payload is big big - length=%d",length);
		goto err;
	}

	// fill up msg header
	phys_msg=(struct qmi_phys_msg_hdr_t*)buf;
	write16_to_little_endian(msg->msg_id,phys_msg->msg_id);
	write16_to_little_endian(length,phys_msg->msg_len);

	phys_tlv=(struct qmi_phys_tlv_hdr_t*)(phys_msg+1);

	// dump all tlvs
	i=0;
	while(i<msg->tlv_count) {
		tlv=msg->tlvs[i];

		phys_tlv->t=tlv->t;
		write16_to_little_endian(tlv->l,phys_tlv->l);
		memcpy(phys_tlv->v,tlv->v,tlv->l);

		phys_tlv=(struct qmi_phys_tlv_hdr_t*)( (char*)phys_tlv+sizeof(*phys_tlv)+tlv->l );

		i++;
	}

	return total;
err:
	return -1;
}

void qmimsg_clear_tlv(struct qmimsg_t* msg)
{
	msg->tlv_count=0;
}

struct qmitlv_t* qmimsg_get_tlv(struct qmimsg_t* msg, unsigned char t)
{
	int i;

	struct qmitlv_t* tlv;

	for(i=0;i<msg->tlv_count;i++) {
		tlv=msg->tlvs[i];

		if(tlv->t == t)
			return tlv;
	}

	return NULL;
}

int qmimsg_add_tlv(struct qmimsg_t* msg, unsigned char t, unsigned short l, const void* v)
{
	int i;
	struct qmitlv_t* tlv;

	i=msg->tlv_count;

	tlv=msg->tlvs[i];
	if(!tlv) {
		tlv=msg->tlvs[i]=qmitlv_create();
		if(!tlv)
			goto err;
	}

	if(qmitlv_set(msg->tlvs[i],t,l,v)<0)
		goto err;

	msg->tlv_count++;

	return 0;
err:
	return -1;
}

void qmimsg_dump(struct qmimsg_t* msg,const char* name,int loglevel)
{
	int i;
	struct qmitlv_t* tlv;

	SYSLOG(loglevel,"name=%s msg_id=0x%04x tlv_count=%d",name,msg->msg_id,msg->tlv_count);

	for(i=0;i<msg->tlv_count;i++) {
		tlv=msg->tlvs[i];
		SYSLOG(loglevel,"TLV idx=%d, t=0x%02x, l=%d, v=0x%08x",i,tlv->t,tlv->l,(unsigned int)tlv->v);
	}
}

int qmimsg_copy_from(struct qmimsg_t* msg,const struct qmimsg_t* src_msg)
{
	int i;
	struct qmitlv_t* tlv;
	unsigned short src_msg_id;

	// copy msg id
	src_msg_id=qmimsg_get_msg_id(src_msg);
	qmimsg_set_msg_id(msg,src_msg_id);

	for(i=0;i<src_msg->tlv_count;i++) {
		tlv=msg->tlvs[i];
		if(!tlv) {
			tlv=msg->tlvs[i]=qmitlv_create();
			if(!tlv)
				goto err;
		}

		// set tlv
		if( qmitlv_set(tlv,src_msg->tlvs[i]->t,src_msg->tlvs[i]->l,src_msg->tlvs[i]->v)<0 )
			goto err;
	}

	msg->tlv_count=src_msg->tlv_count;

	return 0;

err:
	return -1;
}

void qmimsg_destroy(struct qmimsg_t* msg)
{
	int i;

	if(!msg)
		return;

	// delete tlvs
	for(i=0;i<QMIMSG_MAX_TLVS_COUNT;i++)
		qmitlv_destroy(msg->tlvs[i]);

	_free(msg->tlvs);
	_free(msg);
}

void qmimsg_set_msg_id(struct qmimsg_t* msg,unsigned short msg_id)
{
	msg->msg_id=msg_id;
}

unsigned short qmimsg_get_msg_id(const struct qmimsg_t* msg)
{
	return msg->msg_id;
}

struct qmimsg_t* qmimsg_create(void)
{
	struct qmimsg_t* msg;

	// create the object
	msg=(struct qmimsg_t*)_malloc(sizeof(struct qmimsg_t));
	if(!msg) {
		SYSLOG(LOG_ERROR,"failed to allocate qmimsg_t - size=%d",sizeof(struct qmimsg_t));
		goto err;
	}

	// init. members
	msg->tlvs=_malloc(sizeof(struct qmitlv*)*QMIMSG_MAX_TLVS_COUNT);
	if(!msg->tlvs) {
		SYSLOG(LOG_ERROR,"failed to allocate tlvs - size=%d",sizeof(struct qmitlv*)*QMIMSG_MAX_TLVS_COUNT);
		goto err;
	}

	return msg;

err:
	qmimsg_destroy(msg);
	return NULL;
}
