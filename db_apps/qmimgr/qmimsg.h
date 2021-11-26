#ifndef __QMIMSG_H__
#define __QMIMSG_H__

#include "minilib.h"
#include "endian.h"

#define QMIMSG_MAX_TLVS_COUNT	64		// max TLVs count in a service message
#define QMIMSG_TLV_BLOCK	16		// value memory growing unit in TLVs

struct qmi_phys_tlv_hdr_t {
	unsigned char t;
	unsigned short l;
	char v[0];
} __packed;

struct qmi_phys_msg_hdr_t {
	unsigned short msg_id;
	unsigned short msg_len;
	struct qmi_phys_tlv_hdr_t tlv_hdr[0];
} __packed;

struct qmitlv_t {
	unsigned char t;
	unsigned short l;
	void* v;
	int v_len;
};

struct qmimsg_t {
	unsigned short msg_id;

	int tlv_count;
	struct qmitlv_t** tlvs;
};

int qmimsg_write_to_buf(struct qmimsg_t* msg, void* buf,int buf_len);
int qmimsg_read_from_buf(struct qmimsg_t* msg, const void* buf,int buf_len);

int qmimsg_get_total_phy_length(struct qmimsg_t* msg);
int qmimsg_add_tlv(struct qmimsg_t* msg, unsigned char t, unsigned short l, const void* v);
void qmimsg_clear_tlv(struct qmimsg_t* msg);
void qmimsg_destroy(struct qmimsg_t* msg);
struct qmimsg_t* qmimsg_create(void);

void qmimsg_set_msg_id(struct qmimsg_t* msg,unsigned short msg_id);
unsigned short qmimsg_get_msg_id(const struct qmimsg_t* msg);
struct qmitlv_t* qmimsg_get_tlv(struct qmimsg_t* msg, unsigned char t);
const struct qmitlv_t* _get_tlv(struct qmimsg_t* msg,unsigned char t,int min_len);

int qmimsg_copy_from(struct qmimsg_t* msg,const struct qmimsg_t* src_msg);
void qmimsg_dump(struct qmimsg_t* msg,const char* name,int loglevel);


#endif
