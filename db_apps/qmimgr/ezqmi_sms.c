#include "g.h"

// variable buffer length
////////////////////////////////////////////////////////////////////////////////

/* max buffer length for SMS number */
#define SMS_ADDR_BUF_MAX_LEN 256
/* max buffer length for timestamp */
#define SMS_TIMESTAMP_BUF_MAX_LEN 64
/* max qmi route count */
#define SMS_ROUTE_MAX_COUNT	16
/* SMS count to read at once on a event */
#define SMS_READ_POOL_COUNT	512

// default values
////////////////////////////////////////////////////////////////////////////////

/* default SMS encoding scheme */
#define SMS_ENCODING_SMS_DEFAULT "GSM7"
/* default browser encoding scheme */
#define SMS_ENCODING_BROWSER_DEFAULT "UTF-8"

// inbox settings
////////////////////////////////////////////////////////////////////////////////

/* spool directory - ramfs */
#define SMS_INBOX_SPOOL_DIR "/var/spool/sms/incoming"
/* inbox directory - only for Bovine */
#define SMS_INBOX_DIR "/usr/local/cdcs/sms/inbox"
/* inbox and spool max file count */
#define SMS_INBOX_MAX_FILE_COUNT 10000
/* extention for inbox files */
#define SMS_INBOX_FILE_EXT ".raw"
/* expiry time for sms message body */
#define SMS_INBOX_MEMORY_EXPIRY_SEC 60
/* retry count to read a unread message */
#define SMS_INBOX_READ_RETRY 3


// 3GPP defines
////////////////////////////////////////////////////////////////////////////////

/* 3GPP max User-Data-Length */
#define GSM_SMS_UD_MAX_LEN 140
/* 3GPP max chained SMS count */
#define GSM_MAX_UD_COUNT 255

// 3GPP structures
////////////////////////////////////////////////////////////////////////////////

/* 3GPP - address fields */
struct gsm_address_fields_t {
	unsigned char address_length;
	unsigned char type_of_address;
	char address_value[0];
} __packed;

	/* 3GPP - Type-of-number */
	enum type_of_number_t {
		/*
			Bits 6 5 4
				0 0 0 Unknown 1)
				0 0 1 International number 2)
				0 1 0 National number 3)
				0 1 1 Network specific number 4)
				1 0 0 Subscriber number 5)
				1 0 1 Alphanumeric, (coded according to 3GPP TS 23.038 [9] GSM 7-bit default alphabet)
				1 1 0 Abbreviated number
				1 1 1 Reserved for extension

		*/
		type_of_number_mask=(0x07<<4),

		type_of_number_unknown=(0x00),
		type_of_number_international=(0x01<<4),
		type_of_number_national=(0x02<<4),
		type_of_number_network=(0x03<<4),
		type_of_number_subscriber=(0x04<<4),
		type_of_number_alphanumeric=(0x05<<4),
		type_of_number_abbr=(0x06<<4),
		type_of_number_reserved=(0x07<<4)
	};


	/* 3GPP - Numbering-plan-identification */
	enum numbering_plan_ident_t {
		/*

			Bits 3 2 1 0
				0 0 0 0 Unknown
				0 0 0 1 ISDN/telephone numbering plan (E.164 [17]/E.163[18])
				0 0 1 1 Data numbering plan (X.121)
				0 1 0 0 Telex numbering plan
				0 1 0 1 Service Centre Specific plan 1)
				0 1 1 0 Service Centre Specific plan 1)
				1 0 0 0 National numbering plan
				1 0 0 1 Private numbering plan
				1 0 1 0 ERMES numbering plan (ETSI DE/PS 3 01-3)
				1 1 1 1 Reserved for extension
		*/

		numbering_plan_ident_unknown=0x00,
		numbering_plan_ident_isdntelephonenumberingplan=0x01,
		numbering_plan_ident_datanumberingplan=0x03,
		numbering_plan_ident_telexnumberingplan=0x04,
		numbering_plan_ident_servicecenterspecific1=0x5,
		numbering_plan_ident_servicecenterspecific2=0x6,
		numbering_plan_ident_nationalnumberingplan=0x8,
		numbering_plan_ident_privatenumberingplan=0x9,
		numbering_plan_ident_ermesnumberingplan=0x0a,
		numbering_plan_ident_reserved=0x0f,
	};



/* 3GPP - TP-Service-Centre-Time-Stamp */
struct gsm_timestamp_t {
	unsigned char year;
	unsigned char month;
	unsigned char day;
	unsigned char hour;
	unsigned char minute;
	unsigned char second;
	unsigned char tz;
} __packed;

/* 3GPP - sms deliver type : 3GPP TS 123.040 9.2.2.1 */
struct gsm_sms_deliver_type_t {
	unsigned int mti:2;		/* Message Type Indicator */
	unsigned int mms:1;		/* More Message to Send
						0 More messages are waiting for the MS in this SC
						1 No more messages are waiting for the MS in this SC
					*/

	unsigned int lp:1;		/* Loop Prevention */
	unsigned int __reserved:1;
	unsigned int sri:1;		/* Status Report Indication */
	unsigned int udhi:1;		/* User Data Header Indicator */
	unsigned int rp:1;		/* Reply Path */
} __packed;


struct gsm_sms_submit_type_t {
	unsigned int mti:2;		/* Message Type Indicator */
	unsigned int rd:1;		/* Reject Duplicates */
	unsigned int vpf:2;		/* Validity Period Format */
	unsigned int srr:1;		/* Status Report Request */
	unsigned int udhi:1;		/* User Data Header Indicator */
	unsigned int rp:1;		/* Reply Path */
} __packed;

	/* 3GPP - sms submit type : 3GPP TS 123.040 9.2.2.2 */
	enum gsm_sms_submit_type_mti {
		SMS_DELIVER = 0,	/* SMS_DELIVER_REPORT */
		SMS_SUBMIT,		/* SMS_SUBMIT_REPORT */
		SMS_STATUS_REPORT,	/* SMS_COMMAND */
		SMS_RESERVED_TYPE
	};

	enum gsm_sms_submit_type_vpf {
		NO_VPF = 0,
		ENHANCED_VPF_FORMAT,
		RELATIVE_VPF_FORMAT,
		ABSOLUTE_VPF_FORMAT
	};


/* 3GPP - TP-UD header */
struct gsm_user_data_header_t {
	unsigned char udhl;	/* Length of User Data Header */
} __packed;

struct gsm_user_data_sub_header_t {
	unsigned char iei;	/* Information-Element-Identifier */
	unsigned char iedl;	/* Length of Information-Element */
	char ied[0];		/* Information-Element Data */
} __packed;

struct gsm_user_data_concat_t {
	unsigned char ref;	/* Concatenated short message reference number */
	unsigned char max_no;	/* Maximum number of short messages in the concatenated short message */
	unsigned char seq_no;	/* Sequence number of the current short message*/
} __packed;

union gsm_sms_validity_period_t {

	unsigned char relative;

	struct gsm_timestamp_t absolute;

	struct {
		struct {
			unsigned int extension_bit:1;
			unsigned int single_shot_sm:1;
			unsigned int __reserved:3;

			/*
				0 0 0 No Validity Period specified
				0 0 1 Validity Period is as specified for the relative case. The following octet contains the TPVP
				value as described in 9.2.3.12.1
				0 1 0 Validity period is relative in integer representation and the following octet contains the
				TP-VP value in the range 0 to 255 representing 0 to 255 seconds. A TP-VP value of
				zero is undefined and reserved for future use.
				0 1 1 Validity period is relative in semi-octet representation. The following 3 octets contain
				the relative time in Hours, Minutes and Seconds giving the length of the validity period
				counted from when the SMS-SUBMIT is received by the SC. The representation of time
				uses the same representation as the Hours, Minutes and Seconds in the
				TP-Service-Centre-Time-Stamp.
				1 0 0 Reserved
				1 0 1 Reserved
				1 1 0 Reserved
				1 1 1 Reserved
			*/
			unsigned int validity_period_format:3;
		} h;

		unsigned char data[6];
	} enhanced;

};

struct sms_tp_deliver_t {
	struct gsm_sms_deliver_type_t type;	/* TP-MTI, TP-MMS, TP-LP, TP-SRI, TPUDHI, TP-RP */

	struct gsm_address_fields_t* oa;	/* TP-Originating-Address */
	unsigned char pid;			/* TP-Protocol-Identifier */
	unsigned char dcs;			/* TP-Data-Coding-Scheme */

	struct gsm_timestamp_t scts;		/* TP-Service-Centre-Time-Stamp */

	unsigned char udl;			/* TP-User-Data-Length */
	char ud[0];				/* TP-User-Data */

} __packed;

struct sms_tp_sumbit_t {
	struct gsm_address_fields_t* smsc;

	struct gsm_sms_submit_type_t type;	/* TP-MTI, TP-RD, TP-VPF TP-SRR, TP-UDHI, TP-RP */
	unsigned char mr;			/* TP-Message-Reference */
	struct gsm_address_fields_t* da;	/* TP-Destination-Address */
	unsigned char pid;			/* TP-Protocol-Identifier */
	unsigned char dcs;			/* TP-Data-Coding-Scheme */

	union gsm_sms_validity_period_t vp;	/* TP-Validity-Period */

	unsigned char udl;			/* TP-User-Data-Length */
	char ud[0];				/* TP-User-Data */

} __packed;

/*
	* SMS PDU message example
	0x00, 0x01, 0x00, 0x0C, 0x91, 0x19, 0x79, 0x97, 0x11,
	0x21, 0x82, 0x00, 0x00, 0x05, 0xE8, 0x32, 0x9B, 0xFD,
	0x06

	SMSC:		00
	PDU header:	01
	TP-MTI:		01
	TP-RD:		00
	TP-VPF:		00
	TP-SRR:		00
	TP-UDHI:	00
	TP-RP:		00
	TP-MR:		00
	TP-DA:		0C91197997112182
	TP-PID:		00
	TP-DCS:		00
	TP-UDL:		05
	TP-UD:		E8329BFD06
*/

// 3GPP2 structures
////////////////////////////////////////////////////////////////////////////////

/* 3gpp2 transport layer header */
struct cdma_sms_trans_layer_hdr_t {
	unsigned char msg_type;
	char payload[0];
} __packed;

	/* 3gpp2 transport layer message type */
	enum cdma_msg_type_t {
		sms_msg_p2p = 0x00,
		sms_msg_bro = 0x01,
		sms_msg_ack = 0x02,
	};

/* 3gpp2 parameter header */
struct cdma_sms_param_hdr_t {
	unsigned char param_id;
	unsigned char param_len;
	char param[0];
} __packed;

	/* 3gpp2 parameter id */
	enum cdma_sms_param_id_t {
		param_id_teleservice_id=0,	/* 0 - common (required) */
		param_id_service_category,	/* 1 */
		param_id_orig_addr,		/* 2 - common (required) */
		param_id_orig_subaddr,		/* 3 */
		param_id_dest_addr,		/* 4 */
		param_id_dest_subaddr,		/* 5 */
		param_id_bearer_reply,		/* 6 - common (not required) */
		param_id_cause_codes,		/* 7 */
		param_id_bearer_data,		/* 8 - common (required) */
		param_id_count
	};

/* 3gpp2 teleservice parameter */
struct cdma_sms_param_teleserv_id_t {
	struct cdma_sms_param_hdr_t phdr;
	unsigned short id;
} __packed;

/* 3gpp2 bearer sub-parameter - timestamp */
struct cdma_sms_subparam_timestamp_t {
	struct cdma_sms_param_hdr_t phdr;
	unsigned char year;
	unsigned char month;
	unsigned char day;
	unsigned char hours;
	unsigned char minutes;
	unsigned char seconds;
} __packed;

enum cdma_sms_teleservice_id_t {
	cdma_sms_tele_id_cmt_91		= 0x1000, //4096
	cdma_sms_tele_id_cpt_95		= 0x1001, //4097
	cdma_sms_tele_id_cmt_95		= 0x1002, //4098
	cdma_sms_tele_id_vmn_95		= 0x1003, //4099
	cdma_sms_tele_id_wap		= 0x1004, //4100
	cdma_sms_tele_id_wemt		= 0x1005, //4101
	cdma_sms_tele_id_scpt		= 0x1006, //4102
	cdma_sms_tele_id_catpt		= 0x1007, //4103
	cdma_sms_tele_id_unknown	= 0xffff,
};

enum cdma_sms_digit_mode_t {
	cdma_sms_digit_mode_4_bit	= 0,  // 4bit dtmf
	cdma_sms_digit_mode_8_bit	= 1,
	cdma_sms_digit_mode_max		= 0xff
};

enum cdma_sms_number_mode_t {
	cdma_sms_number_mode_none_data_network	= 0,
	cdma_sms_number_mode_data_network	= 1,
	cdma_sms_number_mode_data_network_max	= 0xff
};

enum cdma_sms_number_type_t {
	cdma_sms_number_unknown		= 0,

	/* number mode '0' */
	cdma_sms_number_international	= 1,
	cdma_sms_number_national	= 2,
	cdma_sms_number_network		= 3,
	cdma_sms_number_subscriber	= 4,
	cdma_sms_number_alphanumeric	= 5, /* gsm sms: addr value is gsm 7-bit chars */
	cdma_sms_number_abbreviated	= 6,
	cdma_sms_number_reserved_7	= 7,

	/* number mode '1' */
	cdma_sms_number_data_ip		= 1,
	cdma_sms_number_internet_email	= 2,

	cdma_sms_number_max32		= 0x10000000
};

enum cdma_sms_number_plan_t {
	cdma_sms_number_plan_unknown	= 0,
	cdma_sms_number_plan_isdn	= 1,
	cdma_sms_number_plan_data	= 3,
	cdma_sms_number_plan_telex	= 4,
	cdma_sms_number_plan_private	= 9,

	cdma_sms_number_plan_max	= 0xff,
} ;

enum cdma_sms_subparam_t {
	message_identifier=0,			/* 0 - common (required) */
	user_data,				/* 1 - common (required) */
	user_response_code,			/* 2 */
	message_center_time_stamp,		/* 3 - common (required) */
	validity_period_absolute,		/* 4 */
	validity_period_relative,		/* 5 */
	deferred_delivery_time_absolute,	/* 6 */
	deferred_delivery_time_relative,	/* 7 */
	priority_indicator,			/* 8 - common */
	privacy_indicator,			/* 9 */
	reply_option,				/* 10 */
	number_of_messages,			/* 11 */
	alert_on_message_delivery,		/* 12 */
	language_indicator,			/* 13 - common */
	call_back_number,			/* 14 */
	message_display_mode,			/* 15 */
	multiple_encoding_user_data,		/* 16 */
	message_deposit_index,			/* 17 */
	service_category_program_data,		/* 18 */
	service_category_program_results,	/* 19 */
	message_status,				/* 20 */
	tp_failure_cause,			/* 21 */
	enhanced_vmn,				/* 22 */
	enhanced_vmn_ack,			/* 23 */
	cdma_sms_subparameter_count
};

enum cdma_sms_bd_sub_msg_enc_t {
	cdma_bd_sub_msg_enc_8bit_oct=0,	/* 0x00, 8-bit */
	cdma_bd_sub_msg_enc_ext_pm,	/* 0x01, varies */
	cdma_bd_sub_msg_enc_7bit_acsii,	/* 0x02, 7-bit */
	cdma_bd_sub_msg_enc_ia5,	/* 0x03, 7-bit */
	cdma_bd_sub_msg_enc_unicode,	/* 0x04, 16-bit */
	cdma_bd_sub_msg_enc_jis,	/* 0x05, 8 or 16-bit */
	cdma_bd_sub_msg_enc_korean,	/* 0x06, 8 or 16-bit */
	cdma_bd_sub_msg_enc_latin_heb,	/* 0x07, 8-bit */
	cdma_bd_sub_msg_enc_latin,	/* 0x08, 8-bit */
	cdma_bd_sub_msg_enc_7bit_gsm,	/* 0x09, 7-bit */
	cdma_bd_sub_msg_enc_ext_gsm,	/* 0x0a, */
 	cdma_bd_sub_msg_enc_count,
};

enum cdma_sms_msg_type_t {
	reserved=0,
	deliver,
	submit,
	cancellation,
	delivery_acknowledgment,
	user_acknowledgment,
	read_acknowledgment,
	deliver_report,
	submit_report,
};

// ezqmi sms structures
////////////////////////////////////////////////////////////////////////////////

/* sms body chain for sending */
struct sms_body_schain_t {
	struct qmi_wms_raw_send_req_raw_message_data* tlv_raw_msg;
	struct gsm_user_data_concat_t* udh_concat;
};

/* sms body chain for receiving */
struct sms_body_rchain_t {
	struct linkedlist list;

	unsigned int msg_idx; /* to delete actual messages in NV */
	unsigned int uptime; /* for garbage collection */

	char* buf;
	int buf_len;

	int processed;
	int broken;

	enum sms_msg_type_t sms_type;

	union {
		/* WCDMA (3GPP) */
		struct {
			/* 3GPP specific */
			struct gsm_address_fields_t* smsc;
			int smsc_len;
			struct gsm_sms_deliver_type_t* type;	/* TP-MTI, TP-MMS, TP-LP, TP-SRI, TPUDHI, TP-RP */
			struct gsm_address_fields_t* oa;	/* TP-Originating-Address */
			int oa_len;
			unsigned char* pid;			/* TP-Protocol-Identifier */
			unsigned char* dcs;			/* TP-Data-Coding-Scheme */
			struct gsm_timestamp_t* scts;		/* TP-Service-Centre-Time-Stamp */
			unsigned char* udl;			/* TP-User-Data-Length */
		};

		/* CDMA (3GPP2) */
		struct {
			struct cdma_sms_trans_layer_hdr_t* thdr;

			struct cdma_sms_param_hdr_t* phdrs[param_id_count];

			/* sub-parameters of bearer parameter */
			struct cdma_sms_param_hdr_t* sphdrs[cdma_sms_subparameter_count];

			int dig_mode;
			int num_mode;
			int num_type;
			int num_plan;
			int num_fields;
		};
	};

	/* extra parsed information */
	char* ud;
	struct gsm_user_data_concat_t* udh_concat;

	int total_udhl;

	char* ud_msg;
	int ud_msg_len; /* octet or septet count */

	enum sms_code_type_t code_from;

	clock_t queue_ts;
};

// ezqmi tables and state variables
////////////////////////////////////////////////////////////////////////////////

/* sms code to string table */
static char* _sms_code_to_table[]={
	[dcs_7bit_coding]="GSM-7",
	[dcs_8bit_coding]="ASCII",
	[dcs_ucs2_coding]="UCS-2",
	[dcs_7bit_ascii_coding]="ASCII",
};

/* sms technology to qmi format table */
static int _qmi_format_table[]={
	[smt_unknown]=-1,
	[smt_cdma]=QMI_MESSAGE_FORMAT_CDMA,
	[smt_umts]=QMI_MESSAGE_FORMAT_GW_PP
};

/* current sms technology */
static int _sms_service_ready[smt_max]={0,};
static enum sms_msg_type_t _sms_type=smt_unknown;

/* current browser encoding scheme */
static char _input_coding_scheme[64]={0,};

/* read chain */
static struct sms_body_rchain_t* _rchain=NULL;


// local functions
////////////////////////////////////////////////////////////////////////////////
const char* cdma_decode_address(struct cdma_sms_param_hdr_t* phdr);
const char* cdma_decode_subparam_timestamp(struct cdma_sms_param_hdr_t* phdr);
int cdma_decode_subparam_userdata(struct cdma_sms_param_hdr_t* sphdr,int* msg_encoding,int* msg_type,int* num_fields,char* chari,int chari_len);
enum sms_msg_type_t cdma_get_sms_msg_type_by_encoding(int msg_encoding);

int _qmi_sms_init_sms_type(enum sms_msg_type_t sms_type)
{
	_sms_type=sms_type;

	return 0;
}

void _qmi_sms_update_sms_type_based_on_firmware()
{
	const char* pri;
	
	/* unfortunately, we have to look at PRI since VZW and SPRINT are using 3GPP2 protocol regardless of network */
	pri=_get_str_db("priid_carrier","");
	if(!strcmp(pri,"SPRINT") || !strcmp(pri,"VZW")) {
		SYSLOG(LOG_DEBUG,"###sms### forced to use 3GPP2 protocol (network detected='%s')",pri);
		_qmi_sms_init_sms_type(smt_cdma);
	}
}


void sms_destroy_memory(char** buf)
{
	if(buf) {
		if(*buf)
			free(*buf);
		*buf=NULL;
	}
}

int sms_create_memory_from_file(const char* fname,char** buf)
{
	int fd;
	struct stat st;
	int len;
	int read_byte;

	*buf=NULL;

	/* open */
	fd=open(fname,O_RDONLY);
	if(fd<0)
		goto err;

	/* get stat */
	if(fstat(fd,&st)<0)
		goto err;

	/* limit the size */
	if(st.st_size<=SMS_MAX_FILE_LENGTH)
		len=st.st_size;
	else
		len=SMS_MAX_FILE_LENGTH;

	/* allocate memory */
	*buf=malloc(st.st_size);
	if(!*buf)
		goto err;

	/* read file */
	read_byte=read(fd,*buf,len);
	if(read_byte<0)
		goto err;

	/* close */
	close(fd);

	return read_byte;

err:
	sms_destroy_memory(buf);
	return -1;
}


void _qmi_sms_fini()
{
}

int _qmi_sms_update_sms_type(int* rstat)
{
	int cur_rstat;

	/* get rstat if not given */
	if(!rstat) {
		if(_qmi_sms_get_service_ready_status(NULL,&cur_rstat)<0) {
			SYSLOG(LOG_ERROR,"###sms### failed to get service ready status - assuming umts network");
			cur_rstat=0x01;
		}
	}
	else {
		cur_rstat=*rstat;
	}

	_sms_service_ready[smt_umts]=(cur_rstat&0x01)!=0;
	_sms_service_ready[smt_cdma]=(cur_rstat&0x02)!=0;

	if(_sms_service_ready[smt_umts] || _sms_service_ready[smt_cdma]) {
		if(_sms_service_ready[smt_umts])
			SYSLOG(LOG_INFO,"###sms### 3GPP network available for SMS");
		if(_sms_service_ready[smt_cdma])
			SYSLOG(LOG_INFO,"###sms### 3GPP2 network available for SMS");
	}
	else {
		SYSLOG(LOG_ERROR,"###sms### no network available for SMS");
	}
	

	return 0;
}

int _qmi_sms_init_coding_scheme(const char* input_coding_scheme)
{
	__strncpy_with_zero(_input_coding_scheme,input_coding_scheme,sizeof(_input_coding_scheme));

	return 0;
}


int pdu_decode_address_i_to_c(int i)
{
	char c;

	i&=0x0f;

	if(i<0x0a) {
		c=(char)(i+'0');
	}
	else {
		const char charset[]="*#abc\0";

		i-=0x0a;
		if(i<sizeof(charset)/sizeof(charset[0]))
			c=charset[i];
	}

	return c;
}


/*
	encode   : struct --> pdu
	decode   : pdu -----> struct
*/

int pdu_encode_address_c_to_i(char c)
{
	int i;

	if(c>='0' && c<='9') {
		i=c-'0';
	}
	else {
		switch(c) {
/*
			If a mobile receives an address field containing non-integer information in the semi-octets other than "1111" (e.g. 1110)
			it shall display the semi-octet as the representation given in GSM 44.008 [12] under "called BCD number", viz
			1010="*", 1011="#", 1100="a", 1101="b", 1110="c".
*/
			case '*':
				i=0x0a;
				break;

			case '#':
				i=0x0b;
				break;

			case 'a':
				i=0x0c;
				break;

			case 'b':
				i=0x0d;
				break;

			case 'c':
				i=0x0e;
				break;

			default:
				i=0x0f;
				break;
		}
	}

	return i;
}

int pdu_decode_bcd_octet(char octet)
{
	return ((octet&0x0f)*10) + ((octet>>4)&0x0f);
}

int cdma_decode_bcd_octet(char octet)
{
	return (octet&0x0f)+(((octet>>4)&0x0f)*10);
}

const char* pdu_decode_timestamp(struct gsm_timestamp_t* ts)
{
	static char buf[SMS_TIMESTAMP_BUF_MAX_LEN];

	snprintf(buf,sizeof(buf),"%04d/%02d/%02d,%02d:%02d:%02d",
		pdu_decode_bcd_octet(ts->year)+2000,
		pdu_decode_bcd_octet(ts->month),
		pdu_decode_bcd_octet(ts->day),
		pdu_decode_bcd_octet(ts->hour),
		pdu_decode_bcd_octet(ts->minute),
		pdu_decode_bcd_octet(ts->second)
	);

	return buf;
}

const char* pdu_decode_address(unsigned char type_of_address,const char* pdu_address,int pdu_address_len,int semi_octet_mode)
{
	static char buf[SMS_ADDR_BUF_MAX_LEN];
	int o;
	char ch;
	int c;
	int i;


	int s;

	o=0;

	/* international */
	if((type_of_address&numbering_plan_ident_isdntelephonenumberingplan) && ((type_of_address&type_of_number_mask) == type_of_number_international)) {
		buf[o++]='+';
	}

	if(semi_octet_mode)
		c=pdu_address_len;
	else
		c=pdu_address_len*2;

	/* convert numeric part */
	for(i=0;(i<c) && (o<sizeof(buf)-1);i++) {
		s=(i&0x01)?4:0;

		ch=pdu_decode_address_i_to_c(pdu_address[i/2]>>s);
		if(ch)
			buf[o++]=ch;
	}

	buf[o++]=0;

	return buf;
}



int pdu_encode_address(void* pdu,int pdu_len,const unsigned char type_of_address,const char* address,int semi_octet_mode)
{
	struct gsm_address_fields_t* a;

	int address_octet_len; /* octet length of address */
	int address_len; /* string length of address */
	int exp_pdu_len; /* octet length of total address structure */

	char* p;
	int j;
	int k;
	int o;
	char ch;
	int semi_octet_cnt;

	/* get address length */
	address_len=address?strlen(address):0;
	address_octet_len=(address_len+1)/2;

	/* calculate length of pdu */
	if(!address_octet_len) {
		exp_pdu_len=sizeof(a->address_length);
	}
	else {
		exp_pdu_len=sizeof(*a)+address_octet_len;
	}

	/* if only for testing */
	if(!pdu) {
		goto fini;
	}

	/* check buffer size */
	if(pdu_len<exp_pdu_len) {
		goto err;
	}

	/* build gsm address fields */
	a=(struct gsm_address_fields_t*)pdu;
	a->type_of_address=type_of_address;

	/* build address */
	p=(char*)(a+1);
	j=0;
	o=0;
	k=0;
	semi_octet_cnt=0;
	while(ch=*address++, ch) {
		switch(ch) {
			/* ignore special characters */
			case '+':
			case ' ':
				break;

			default:
				/* reset o when j is 0 */
				if(!j)
					o=0;

				/* collect half-octet */
				o|=pdu_encode_address_c_to_i(ch)<<(j++*4);
				semi_octet_cnt++;
				/* increase j */
				j%=2;

				if(!j)
					p[k++]=o;

		}
	}

	/* put 0xF0 if input is odd */
	if(j) {
		o|=pdu_encode_address_c_to_i(0)<<(j++*4);
		p[k++]=o;
	}

	/* get length */
	exp_pdu_len=sizeof(*a)+k;

	if(semi_octet_mode)
		a->address_length=semi_octet_cnt;
	else
		a->address_length=exp_pdu_len-sizeof(a->address_length);
fini:
	return exp_pdu_len;

err:
	return -1;

}

int _qmi_sms_get_msg_mode(enum sms_msg_type_t sms_type)
{
	int msg_mode;

	/* get message mode */
	switch(sms_type) {
		case smt_cdma:
			msg_mode=QMI_WMS_LIST_MESSAGES_REQ_TYPE_MESSAGE_MODE_CDMA;
			break;

		case smt_umts:
			msg_mode=QMI_WMS_LIST_MESSAGES_REQ_TYPE_MESSAGE_MODE_GW;
			break;

		default:
			msg_mode=-1;
			break;
	}

	return msg_mode;
}

int _qmi_sms_delete(enum sms_msg_type_t sms_type, unsigned char* storage_type,unsigned char* tag_type,unsigned int* msg_idx)
{
	struct qmi_easy_req_t er;
	int rc;
	int msg_mode;

	/* init req */
	rc=_qmi_easy_req_init(&er,QMIWMS,QMI_WMS_DELETE);
	if(rc<0)
		goto err;

	/* get msg mode */
	msg_mode=_qmi_sms_get_msg_mode(sms_type);
	if(msg_mode<0) {
		SYSLOG(LOG_ERROR,"###sms### [delete] incorrect sms type detected (sms_type=%d)",sms_type);
		goto err;
	}

	/* build tvls */
	if(storage_type)
		qmimsg_add_tlv(er.msg,QMI_WMS_DELETE_REQ_TYPE_STORAGE_TYPE,sizeof(*storage_type),storage_type);

	if(msg_idx)
		qmimsg_add_tlv(er.msg,QMI_WMS_DELETE_REQ_TYPE_INDEX,sizeof(*msg_idx),msg_idx);

	if(tag_type)
		qmimsg_add_tlv(er.msg,QMI_WMS_DELETE_REQ_TYPE_TAG_TYPE,sizeof(*tag_type),tag_type);

	qmimsg_add_tlv(er.msg,QMI_WMS_DELETE_REQ_TYPE_MSG_MODE,sizeof(char),&msg_mode);

	/* init. tlv */
	rc=_qmi_easy_req_do(&er,0,0,NULL,QMIMGR_GENERIC_RESP_TIMEOUT);
	if(rc<0)
		goto err;

	_qmi_easy_req_fini(&er);
	return 0;

err:
	_qmi_easy_req_fini(&er);
	return -1;
}

int _qmi_sms_register_indications(char* tlinfo,char* nwreg,char* callstat,char* sready,char* bcevent)
{
	struct qmi_easy_req_t er;
	int rc;

	/* init req */
	rc=_qmi_easy_req_init(&er,QMIWMS,QMI_WMS_INDICATION_REGISTER);
	if(rc<0)
		goto err;

	if(tlinfo)
		qmimsg_add_tlv(er.msg,QMI_WMS_INDICATION_REGISTER_REQ_TYPE_TLFINO,sizeof(*tlinfo),tlinfo);
	if(nwreg)
		qmimsg_add_tlv(er.msg,QMI_WMS_INDICATION_REGISTER_REQ_TYPE_NWREG,sizeof(*nwreg),nwreg);
	if(callstat)
		qmimsg_add_tlv(er.msg,QMI_WMS_INDICATION_REGISTER_REQ_TYPE_CALLSTAT,sizeof(*callstat),callstat);
	if(sready)
		qmimsg_add_tlv(er.msg,QMI_WMS_INDICATION_REGISTER_REQ_TYPE_SREADY,sizeof(*sready),sready);
	if(bcevent)
		qmimsg_add_tlv(er.msg,QMI_WMS_INDICATION_REGISTER_REQ_TYPE_BCEEVENT,sizeof(*bcevent),bcevent);

	/* init. tlv */
	rc=_qmi_easy_req_do(&er,0,0,NULL,QMIMGR_GENERIC_RESP_TIMEOUT);
	if(rc<0)
		goto err;

	_qmi_easy_req_fini(&er);

	return 0;

err:
	_qmi_easy_req_fini(&er);
	return -1;


}

int _qmi_sms_get_service_ready_status(int* ind,int* rstat)
{
	struct qmi_easy_req_t er;
	int rc;
	const struct qmitlv_t* tlv;

	char* resp_ind;
	int* resp_ready_status;

	/* init req */
	rc=_qmi_easy_req_init(&er,QMIWMS,QMI_WMS_GET_SERVICE_READY_STATUS);
	if(rc<0)
		goto err;

	/* init. tlv */
	rc=_qmi_easy_req_do(&er,0,0,NULL,QMIMGR_TIMEOUT_10_S);
	if(rc<0)
		goto err;

	/* get transport layer info */
	tlv=_get_tlv(er.rmsg,QMI_WMS_GET_SERVICE_READY_STATUS_RESP_TYPE_REG,sizeof(*resp_ind));
	if(!tlv)
		goto err;
	resp_ind=(char*)(tlv->v);

	/* get transport layer info */
	tlv=_get_tlv(er.rmsg,QMI_WMS_GET_SERVICE_READY_STATUS_RESP_TYPE_RSTAT,sizeof(*resp_ready_status));
	if(!tlv)
		goto err;
	resp_ready_status=(int*)(tlv->v);

	SYSLOG(LOG_DEBUG,"###sms### QMI_WMS_GET_SERVICE_READY_STATUS_RESP_TYPE_REG - ind=%d",*resp_ind);
	SYSLOG(LOG_DEBUG,"###sms### QMI_WMS_GET_SERVICE_READY_STATUS_RESP_TYPE_REG - resp_ready_status=%d",*resp_ready_status);

	if(ind)
		*ind=*resp_ind;
	if(rstat)
		*rstat=*resp_ready_status;

	_qmi_easy_req_fini(&er);

	return 0;

err:
	_qmi_easy_req_fini(&er);
	return -1;


}

int _qmi_sms_get_domain_ref_config()
{
	struct qmi_easy_req_t er;
	int rc;
	const struct qmitlv_t* tlv;

	char* lte_domain_ref;
	char* gw_domain_ref;


	/* init req */
	rc=_qmi_easy_req_init(&er,QMIWMS,QMI_WMS_GET_DOMAIN_PREF_CONFIG);
	if(rc<0)
		goto err;

	/* init. tlv */
	rc=_qmi_easy_req_do(&er,0,0,NULL,QMIMGR_GENERIC_RESP_TIMEOUT);
	if(rc<0)
		goto err;

	/* get lte domain preference */
	tlv=_get_tlv(er.rmsg,QMI_WMS_GET_DOMAIN_PREF_CONFIG_RESP_TYPE_LTE_DP,sizeof(*lte_domain_ref));
	if(tlv) {
		lte_domain_ref=(char*)(tlv->v);
		SYSLOG(LOG_INFO,"###sms### QMI_WMS_GET_DOMAIN_PREF_CONFIG - lte_domain_ref = %d",*lte_domain_ref);
	}
	else {
		SYSLOG(LOG_INFO,"###sms### QMI_WMS_GET_DOMAIN_PREF_CONFIG - lte_domain_ref not available");
	}
		

	/* get lte domain preference */
	tlv=_get_tlv(er.rmsg,QMI_WMS_GET_DOMAIN_PREF_CONFIG_RESP_TYPE_DP_PREF,sizeof(*gw_domain_ref));
	if(tlv) {
		gw_domain_ref=(char*)(tlv->v);
		SYSLOG(LOG_INFO,"###sms### QMI_WMS_GET_DOMAIN_PREF_CONFIG - gw_domain_ref = %d",*gw_domain_ref);
	}
	else {
		SYSLOG(LOG_INFO,"###sms### QMI_WMS_GET_DOMAIN_PREF_CONFIG - gw_domain_ref not available");
	}

	_qmi_easy_req_fini(&er);

	return 0;

err:
	_qmi_easy_req_fini(&er);
	return -1;


}

int _qmi_sms_get_trans_layer_info()
{
	struct qmi_easy_req_t er;
	int rc;
	const struct qmitlv_t* tlv;

	char* resp_ind;
	struct qmi_wms_get_transport_layer_info_resp_tlinfo* resp_tlinfo;


	/* init req */
	rc=_qmi_easy_req_init(&er,QMIWMS,QMI_WMS_GET_TRANSPORT_LAYER_INFO);
	if(rc<0)
		goto err;

	/* init. tlv */
	rc=_qmi_easy_req_do(&er,0,0,NULL,QMIMGR_GENERIC_RESP_TIMEOUT);
	if(rc<0)
		goto err;

	/* get transport layer info */
	tlv=_get_tlv(er.rmsg,QMI_WMS_GET_TRANSPORT_LAYER_INFO_RESP_TYPE_REG,sizeof(*resp_ind));
	if(!tlv)
		goto err;
	resp_ind=(char*)(tlv->v);

	/* get transport layer info */
	tlv=_get_tlv(er.rmsg,QMI_WMS_GET_TRANSPORT_LAYER_INFO_RESP_TYPE_TLINFO,sizeof(*resp_tlinfo));
	if(!tlv)
		goto err;
	resp_tlinfo=(struct qmi_wms_get_transport_layer_info_resp_tlinfo*)(tlv->v);

	SYSLOG(LOG_INFO,"###sms### QMI_WMS_GET_TRANSPORT_LAYER_INFO - ind=%d",*resp_ind);
	SYSLOG(LOG_INFO,"###sms### QMI_WMS_GET_TRANSPORT_LAYER_INFO - type=%d",resp_tlinfo->transport_type);
	SYSLOG(LOG_INFO,"###sms### QMI_WMS_GET_TRANSPORT_LAYER_INFO - cap=%d",resp_tlinfo->transport_cap);

	_qmi_easy_req_fini(&er);

	return 0;

err:
	_qmi_easy_req_fini(&er);
	return -1;


}


int _qmi_sms_get_message_proto(unsigned char* msg_mode)
{
	struct qmi_easy_req_t er;
	int rc;
	const struct qmitlv_t* tlv;
	unsigned char* resp;

	/* init req */
	rc=_qmi_easy_req_init(&er,QMIWMS,QMI_WMS_GET_MESSAGE_PROTOCOL);
	if(rc<0)
		goto err;

	/* init. tlv */
	rc=_qmi_easy_req_do(&er,0,0,NULL,QMIMGR_GENERIC_RESP_TIMEOUT);
	if(rc<0)
		goto err;

	/* get resp */
	tlv=_get_tlv(er.rmsg,QMI_WMS_GET_MESSAGE_PROTOCOL_RESP_TYPE,sizeof(*resp));
	if(!tlv)
		goto err;

	resp=(unsigned char*)(tlv->v);

	if(msg_mode)
		*msg_mode=*resp;

	_qmi_easy_req_fini(&er);

	return 0;

err:
	_qmi_easy_req_fini(&er);
	return -1;


}

int _qmi_sms_get_list_messages(enum sms_msg_type_t sms_type, unsigned char storage_type,unsigned char tag_type,unsigned* rmsg_idx,unsigned char* rtag_type,int* cnt)
{
	struct qmi_easy_req_t er;
	int rc;
	int msg_mode;

	struct qmi_wms_list_messages_req_storage_type req_st;
	struct qmi_wms_list_messages_req_req_tag req_rt;
	struct qmi_wms_list_messages_req_type_msg_mode	req_mm;
	struct qmi_wms_list_messages_resp* resp;

	const struct qmitlv_t* tlv;

	/* init req */
	rc=_qmi_easy_req_init(&er,QMIWMS,QMI_WMS_LIST_MESSAGES);
	if(rc<0)
		goto err;

	/* get msg mode */
	msg_mode=_qmi_sms_get_msg_mode(sms_type);
	if(msg_mode<0) {
		SYSLOG(LOG_ERROR,"###sms### [getlist] incorrect sms type detected (sms_type=%d)",sms_type);
		goto err;
	}

	/* build tvls */
	req_st.storage_type=storage_type;
	req_rt.tag_type=tag_type;
	req_mm.message_mode=(char)msg_mode;

	qmimsg_add_tlv(er.msg,QMI_WMS_LIST_MESSAGES_REQ_TYPE_STORAGE_TYPE,sizeof(req_st),&req_st);
	qmimsg_add_tlv(er.msg,QMI_WMS_LIST_MESSAGES_REQ_TYPE_REQ_TAG,sizeof(req_rt),&req_rt);
	qmimsg_add_tlv(er.msg,QMI_WMS_LIST_MESSAGES_REQ_TYPE_MSG_MODE,sizeof(req_mm),&req_mm);

	/* init. tlv */
	rc=_qmi_easy_req_do(&er,0,0,NULL,QMIMGR_GENERIC_RESP_TIMEOUT);
	if(rc<0)
		goto err;

	/* get resp */
	tlv=_get_tlv(er.rmsg,QMI_WMS_LIST_MESSAGES_RESP_TYPE,sizeof(*resp));
	if(!tlv)
		goto err;

	resp=tlv->v;

	int i;
	for(i=0;(i<*cnt) && (i<resp->N_messages);i++) {
		rmsg_idx[i]=resp->n[i].message_index;
		rtag_type[i]=resp->n[i].tag_type;
	}

	*cnt=resp->N_messages;

	_qmi_easy_req_fini(&er);
	return 0;

err:
	_qmi_easy_req_fini(&er);
	return -1;
}

int _qmi_sms_set_event(int en)
{
	struct qmi_easy_req_t er;
	int rc;

	struct qmi_wms_set_event_report_req req;

	/* init req */
	rc=_qmi_easy_req_init(&er,QMIWMS,QMI_WMS_SET_EVENT_REPORT);
	if(rc<0)
		goto err;

	/* build req */
	req.report_mt_message=en?1:0;

	/* init. tlv */
	rc=_qmi_easy_req_do(&er,QMI_WMS_SET_EVENT_REPORT_REQ_TYPE,sizeof(req),&req,QMIMGR_GENERIC_RESP_TIMEOUT);
	if(rc<0)
		goto err;

	_qmi_easy_req_fini(&er);
	return 0;

err:
	_qmi_easy_req_fini(&er);
	return -1;
}

int _qmi_sms_set_smsc(int address_type,const char* address)
{
	struct qmi_easy_req_t er;
	int rc;

	struct qmi_wms_smsc_address_req* req;
	char address_type_buf[sizeof(req->smsc_address_type)+1];

	/* check address type range */
	if(address_type>999 && address_type<0) {
		SYSLOG(LOG_ERROR,"###sms### invalid address type given (address_type=%d)",address_type);
		goto err;
	}

	/* init req */
	rc=_qmi_easy_req_init(&er,QMIWMS,QMI_WMS_SET_SMSC_ADDRESS);
	if(rc<0)
		goto err;

	/* build req */
	snprintf(address_type_buf,sizeof(address_type_buf),"%d",address_type);
	qmimsg_add_tlv(er.msg,QMI_WMS_SET_SMSC_ADDRESS_REQ_TYPE_ADDR_TYPE,strlen(address_type_buf),address_type_buf);
	qmimsg_add_tlv(er.msg,QMI_WMS_SET_SMSC_ADDRESS_REQ_TYPE_SMSC_ADDR,strlen(address),address);

	/* init. tlv */
	rc=_qmi_easy_req_do(&er,0,0,NULL,QMIMGR_GENERIC_RESP_TIMEOUT);
	if(rc<0)
		goto err;

	_qmi_easy_req_fini(&er);
	return 0;

err:
	_qmi_easy_req_fini(&er);
	return -1;
}

int _qmi_sms_get_smsc(int* address_type,char* address,int address_len)
{
	struct qmi_easy_req_t er;
	const struct qmitlv_t* tlv;
	int rc;

	const char* address_type_str;

	struct qmi_wms_smsc_address_req* resp;

	/* init req */
	rc=_qmi_easy_req_init(&er,QMIWMS,QMI_WMS_GET_SMSC_ADDRESS);
	if(rc<0)
		goto err;

	/* init. tlv */
	rc=_qmi_easy_req_do_ex(&er,0,0,NULL,QMIMGR_TIMEOUT_10_S,0,1);
	if(rc<0)
		goto err;

	tlv=_get_tlv(er.rmsg,QMI_WMS_GET_SMSC_ADDRESS_RESP_TYPE,sizeof(*resp));
	if(!tlv)
		goto err;

	resp=tlv->v;

	/* check buffer size for address */
	if(address_len<resp->smsc_address_length+1) {
		SYSLOG(LOG_ERROR,"###sms### not enough buffer space for address (size=%d,given=%d)",resp->smsc_address_length,address_len);
		goto err;
	}

	/* get address */
	strncpy(address,resp->smsc_address_digits,resp->smsc_address_length);
	address[resp->smsc_address_length]=0;

	/* get address type */
	address_type_str=strndupa(resp->smsc_address_type,sizeof(resp->smsc_address_type));
	*address_type=atoi(address_type_str);

	_qmi_easy_req_fini(&er);
	return 0;

err:
	_qmi_easy_req_fini(&er);
	return -1;
}

 void _qmi_sms_remove_from_pool(struct sms_body_rchain_t* ce)
{
	/* remove ce from the list */
	linkedlist_del(&ce->list);

	/* clear link information in ce */
	linkedlist_init(&ce->list);
}

void _qmi_sms_del_from_pool(struct sms_body_rchain_t* ce)
{
	if(!ce)
		return;

	_qmi_sms_remove_from_pool(ce);

	_free(ce->buf);
	_free(ce);

}

int _dirscan_on_inbox_filter(const struct dirent* de)
{
	/*
		Obviously, this filter is not strictly filtering files in inbox. Although, we keep the exactly same filter from the orignal design of
		Simple AT port manager to avoid the backward incompatibility.
	*/

	int store;

	const char* ignore_files[]={".","..",NULL};
	const char** p;

	/* ignore files */
	p=ignore_files;
	while(*p) {
		store=strcmp(de->d_name,*p++);
		if(!store)
			goto ignore;
	}

	/* ignore if read not found in the name */
	return strstr(de->d_name,"read")!=0;

ignore:
	return 0;
}

const char* get_inbox_fname_from_idx(int idx)
{
	static char fname[PATH_MAX];

	snprintf(fname,sizeof(fname),"rxmsg_%05d_unread",idx);
	return fname;
}

int get_inbox_idx_from_fname(const char* fname)
{
	const char* p;

	p=strstr(fname,"rxmsg_");
	if(!p)
		return -1;

	p+=strlen("rxmsg_");

	return atoi(p);
}

int _dirscan_on_inbox_compare(const void* p1, const void* p2)
{
	int idx1;
	int idx2;
	const struct dirent** de1=(const struct dirent**)p1;
	const struct dirent** de2=(const struct dirent**)p2;

	idx1=get_inbox_idx_from_fname((*de1)->d_name);
	idx2=get_inbox_idx_from_fname((*de2)->d_name);

	return idx1-idx2;
}

void _destroy_de(struct dirent** de,int cnt)
{
	int i;
	if(!de)
		return;

	for(i=0;i<cnt;i++)
		_free(de[i]);
}
const char* _qmi_sms_get_inbox_file_to_write(int* idx)
{
	const char* inbox_path;

	int cnt_inbox=0;
	int cnt_spool=0;
	struct dirent** de=NULL;

	int new;
	const char* new_inbox_file;

	static char fname[PATH_MAX];
	const char* ret;

	struct dirent** de_inbox=NULL;
	struct dirent** de_spool=NULL;

	int de_len;
	int cnt;

	char _inbox_path[PATH_MAX];

	/*
		This new index structure is fixing the orignal design of AT Port manager SMS inbox structure. Although, it is still wrong since total count
		is not limited by SMS total number but by the last index. This incorrect structure is kept as it is for the backward incompatibility..
	*/

	/* get inbox path */
	inbox_path=_get_str_db_ex("smstools.inbox_path","",0);
	__strncpy_with_zero(_inbox_path,inbox_path,sizeof(_inbox_path));
	if(!*inbox_path) {
		SYSLOG(LOG_ERR,"###sms### failed to get inbox path from rdb [smstools.inbox_path]");
		goto err;
	}

	/* scan dir */
	cnt_inbox=scandir(_inbox_path, &de_inbox, _dirscan_on_inbox_filter, NULL);
	if(cnt_inbox<0) {
		SYSLOG(LOG_ERR,"###sms### failed to scan inbox directory (inbox=%s)",_inbox_path);
		goto err;
	}

	/* scan dir spool dir */
	cnt_spool=scandir(SMS_INBOX_SPOOL_DIR, &de_spool, _dirscan_on_inbox_filter, NULL);
	if(cnt_spool<0) {
		SYSLOG(LOG_ERR,"###sms### failed to scan inbox spool directory (inbox=%s)",SMS_INBOX_SPOOL_DIR);
		goto err;
	}

	/* allocate memory for merged list */
	cnt=cnt_inbox+cnt_spool;
	de_len=cnt*sizeof(struct dirent*);

	if(de_len) {
		de=_malloc(de_len);
		if(!de) {
			SYSLOG(LOG_ERR,"###sms### failed to allocate memory (cnt_spool=%d,cnt_inbox=%d)",cnt_spool,cnt_inbox);
			goto err;
		}
	}

	if(de) {
		/* merge two list into one */
		memcpy(de,de_inbox,cnt_inbox*sizeof(struct dirent*));
		memcpy(de+cnt_inbox,de_spool,cnt_spool*sizeof(struct dirent*));
		/* sort */
		if(cnt>1)
			qsort(de,cnt,sizeof(de[1]),_dirscan_on_inbox_compare);
	}

	struct stat sb;
	time_t mtime;
	int lde_idx;
	int i;

	/* search oldest one */
	lde_idx=-1;
	mtime=0;
	for(i=0;i<cnt;i++) {


		snprintf(fname,sizeof(fname),"%s/%s",_inbox_path,de[i]->d_name);
		if(stat(fname, &sb)<0)
			continue;

		//SYSLOG(LOG_DEBUG,"###sms### [INBOX] %d / %s / %ld",i,de[i]->d_name,sb.st_mtime);

		if(!mtime || (mtime>sb.st_mtime)) {
			mtime=sb.st_mtime;
			lde_idx=i;
		}
	}

	/* overwrap if full */
	if(cnt<SMS_INBOX_MAX_FILE_COUNT) {
		new=(cnt<1)?0:(get_inbox_idx_from_fname(de[cnt-1]->d_name)+1);
	}
	else {
		new=get_inbox_idx_from_fname(de[lde_idx]->d_name);
	}

	SYSLOG(LOG_OPERATION,"###sms### [INBOX] oldest_idx=%d, total_cnt=%d, new=%d",lde_idx,cnt,new);

	//_system("ls -la /usr/local/cdcs/sms/inbox/ /var/spool/sms/incoming/ | logger -t '###sms###'");
	/* print log */
	if(cnt<SMS_INBOX_MAX_FILE_COUNT)
		SYSLOG(LOG_OPERATION,"###sms### [MSG] write to inbox message / new (idx=%d)",new);
	else
		SYSLOG(LOG_OPERATION,"###sms### [MSG] write to inbox message / overwrite (idx=%d)",new);

	/* return new index */
	if(idx)
		*idx=new;

	new_inbox_file=get_inbox_fname_from_idx(new);
	snprintf(fname,sizeof(fname),"%s/%s",SMS_INBOX_SPOOL_DIR,new_inbox_file);

	ret=fname;
fini:
	_destroy_de(de_inbox,cnt_inbox);
	_free(de_inbox);

	_destroy_de(de_spool,cnt_spool);
	_free(de_spool);

	_free(de);

	/* get new inbox file name */
	return ret;

err:
	ret=NULL;
	goto fini;
}

int _qmi_sms_flush_celements_to_file(struct sms_body_rchain_t* wc)
{
	struct sms_body_rchain_t* ce;
	struct sms_body_rchain_t* nce;
	struct sms_body_rchain_t* pce;
	struct gsm_user_data_concat_t* udh_concat;

	const char* new_inbox_file;
	int inbox_idx;

	int rc=-1;

	iconv_t cd=NULL;

	/* print log */
	ce=wc;
	while(ce) {

		udh_concat=ce->udh_concat;
		if(udh_concat)
			SYSLOG(LOG_DEBUG,"###sms### write messages to inbox (ref=%d,idx=%d,max_idx=%d,udl=%d)",udh_concat->ref,udh_concat->seq_no,udh_concat->max_no,ce->ud_msg_len);
		else
			SYSLOG(LOG_DEBUG,"###sms### write message to inbox (udl=%d)",ce->ud_msg_len);

		ce=(struct sms_body_rchain_t*)ce->list.next;
	}

	/* get new inbox file name */
	new_inbox_file=_qmi_sms_get_inbox_file_to_write(&inbox_idx);

	char* p;
	char prebuf[(GSM_SMS_UD_MAX_LEN*8+6)/7];

	#define CDMA_UD_MAX_LEN 160

	char cdma_prebuf[CDMA_UD_MAX_LEN*8/7];
	size_t inbytesleft;
	int prebuf_len;
	char outbuf[GSM_SMS_UD_MAX_LEN*4];

	char* o;
	size_t outbytesleft;

	/* get first ce */
	ce=wc;

	enum sms_code_type_t code_from;
	code_from=ce->code_from;

	int fd=-1;
	char text_buf[1024];
	int text_buf_idx;
	int text_buf_len=sizeof(text_buf);

	SYSLOG(LOG_OPERATION,"###sms## open inbox file (new_inbox_file=%d)",inbox_idx);

	/* build sms text header */
	text_buf_idx=0;

	const char* oa_str;
	const char* smsc_str;
	const char* ts_str;

	if(ce->sms_type==smt_cdma) {

		/* get "from" */
		oa_str=strdupa(cdma_decode_address(ce->phdrs[param_id_orig_addr]));

		/* get "smsc" */
		smsc_str="";

		/* get "timestamp" */
		ts_str=strdupa(cdma_decode_subparam_timestamp(ce->sphdrs[message_center_time_stamp]));
	}
	else {
		/* get "from" */
		struct gsm_address_fields_t* oa;
		oa=ce->oa;
		oa_str=strdupa(pdu_decode_address(oa->type_of_address,oa->address_value,oa->address_length,1));

		/* get "smsc" */
		struct gsm_address_fields_t* smsc;
		smsc=ce->smsc;
		smsc_str=strdupa(pdu_decode_address(smsc->type_of_address,smsc->address_value,smsc->address_length-sizeof(smsc->type_of_address),0));


		/* get "timestamp" */
		struct gsm_timestamp_t* ts;
		ts=ce->scts;
		ts_str=strdupa(pdu_decode_timestamp(ts));
	}


	/* print to text buffer and set rdb variables */
	text_buf_idx+=snprintf(text_buf+text_buf_idx,text_buf_len-text_buf_idx,"From: %s\n",oa_str);
	_set_str_db("sms.read.dstno",oa_str,-1);

	text_buf_idx+=snprintf(text_buf+text_buf_idx,text_buf_len-text_buf_idx,"From_SMSC: %s\n",smsc_str);
	_set_str_db("sms.read.dstscno",smsc_str,-1);

	text_buf_idx+=snprintf(text_buf+text_buf_idx,text_buf_len-text_buf_idx,"Sent: %s\n",ts_str);
	_set_str_db("sms.read.time_stamp",ts_str,-1);

	/* print misc - hardcoded */
	text_buf_idx+=snprintf(text_buf+text_buf_idx,text_buf_len-text_buf_idx,
		"Subject: GSM1\n"
		"Alphabet:  ISO\n"
		"UDH: false\n\n"
	);

	/* print encoding type */
	const char* inbox_charset_names[]={
		[dcs_7bit_coding]="GSM7:",
		[dcs_8bit_coding]="GSM7:", /* this GSM7 tag is wrong. Although this is the AT port manage setup */
		[dcs_ucs2_coding]="UCS2:",
		[dcs_7bit_ascii_coding]="GSM7:",
	};
	text_buf_idx+=snprintf(text_buf+text_buf_idx,text_buf_len-text_buf_idx,"%s",inbox_charset_names[code_from]);

	/* open inbox file */
	fd=open(new_inbox_file,O_CREAT|O_TRUNC|O_WRONLY,0644);
	if(fd<0) {
		SYSLOG(LOG_ERROR,"###sms### failed to open inbox file (f=%s,e=%s)",new_inbox_file,strerror(errno));
		goto err;
	}

	/* write text header */
	int written;
	int len_to_write;

	len_to_write=text_buf_idx;
	written=write(fd,text_buf,len_to_write);
	if(written<0) {
		SYSLOG(LOG_ERR,"###sms### failed to write text infomration to inbox file (inbox_idx=%d,err=%s)",inbox_idx,strerror(errno));
		goto err;
	}

	/* open iconv */
	if(code_from!=dcs_unknown) {
		cd=iconv_open(_input_coding_scheme,_sms_code_to_table[code_from]);
	}
	else {
		SYSLOG(LOG_ERROR,"###sms### unknown DCS detected (dcs=%d)",*ce->dcs);
	}

	nce=wc;
	ce=NULL;

	while(pce=ce,(ce=nce)!=NULL) {

		nce=(struct sms_body_rchain_t*)ce->list.next;
		udh_concat=ce->udh_concat;

		/* check concat ref number - bypass if seq_no is not smaller than previous */
		if(pce && pce->udh_concat && ce->udh_concat) {
			if(udh_concat->seq_no<=pce->udh_concat->seq_no) {
				SYSLOG(LOG_OPERATION,"###sms### duplicated seq_no detected, skip (ref=%d,idx=%d,max_idx=%d)",udh_concat->ref,udh_concat->seq_no,udh_concat->max_no);
				continue;
			}
		}

		/* sms-specific decode */
		switch(ce->sms_type) {
			case smt_cdma: {
				int msg_encoding;
				int msg_type;
				int num_fields;
				struct cdma_sms_param_hdr_t* sphdr;

				sphdr=ce->sphdrs[user_data];

				if(cdma_decode_subparam_userdata(sphdr,&msg_encoding,&msg_type,&num_fields,cdma_prebuf,sizeof(cdma_prebuf))<0) {
					SYSLOG(LOG_OPERATION,"###sms### failed to unpack cdma user data");
					goto err;
				}

#ifdef CONFIG_CDMA_SMS_TEST_FAKEDATA
				{
					struct gsm_user_data_header_t* udh;
					struct gsm_user_data_sub_header_t* udsh;

					int pdu_idx=0;

					/* get udh */
					udh=(struct gsm_user_data_header_t*)(sphdr->param+pdu_idx);
					pdu_idx+=sizeof(*udh);

					/* search concat header */
					int pdu_idx_udh;
					pdu_idx_udh=0;
					while(pdu_idx_udh+sizeof(*udsh)<=udh->udhl) {
						udsh=(struct gsm_user_data_sub_header_t*)(ce->buf+pdu_idx+pdu_idx_udh);

						if(udsh->iei==0x00)
							ce->udh_concat=(struct gsm_user_data_concat_t*)(udsh+1);

						pdu_idx_udh+=sizeof(*udsh)+udsh->iedl;
					}

					pdu_idx+=udh->udhl;

					/* get msg */
					ce->total_udhl=sizeof(*udh)+udh->udhl;
					ce->ud_msg=ce->buf+pdu_idx;

					/* septet count for 7 bit encoding */
					if(ce->code_from==dcs_7bit_coding || ce->code_from==dcs_7bit_ascii_coding)
						ce->ud_msg_len=*ce->udl-((pdu_idx_udh+sizeof(*udh))*8+6)/7;
					else
						ce->ud_msg_len=*ce->udl-(pdu_idx_udh+sizeof(*udh));
				}

				unsigned char* r=(unsigned char*)(sphdr->param+11);
				prebuf_len=SMS_GSMDecode(cdma_prebuf,r,ce->ud_msg_len,0);
				prebuf_len=SMS_GSMDecode(cdma_prebuf,r,ce->ud_msg_len,1);
				prebuf_len=SMS_GSMDecode(cdma_prebuf,r,ce->ud_msg_len,2);
				prebuf_len=SMS_GSMDecode(cdma_prebuf,r,ce->ud_msg_len,3);
				prebuf_len=SMS_GSMDecode(cdma_prebuf,r,ce->ud_msg_len,4);
				prebuf_len=SMS_GSMDecode(cdma_prebuf,r,ce->ud_msg_len,5);
				prebuf_len=SMS_GSMDecode(cdma_prebuf,r,ce->ud_msg_len,6);
				prebuf_len=SMS_GSMDecode(cdma_prebuf,r,ce->ud_msg_len,7);
#endif

				p=cdma_prebuf;
				prebuf_len=(ce->code_from==dcs_ucs2_coding)?num_fields*2:num_fields;

				SYSLOG(LOG_ERROR,"###sms### [cdma] write to file (code=%d,num_fields=%d,prebuf_len=%d,param_len=%d)",ce->code_from,num_fields,prebuf_len,sphdr->param_len);

				break;
			}

			case smt_umts: {
				/* unpack 7 bits */
				if(code_from==dcs_7bit_coding) {

					int pad_bits=0;

					if(ce->total_udhl) {
						pad_bits = (ce->total_udhl*8)%7;
						if(pad_bits)
							pad_bits=7-pad_bits;
					}

					p=prebuf;
					prebuf_len=SMS_GSMDecode(prebuf,(unsigned char*)ce->ud_msg,ce->ud_msg_len,pad_bits);
				}
				else {
					p=ce->ud_msg;
					prebuf_len=ce->ud_msg_len;
				}
				break;
			}

			default:
				SYSLOG(LOG_ERROR,"###sms### unknown sms type (sms_type=%d)",ce->sms_type);
				goto err;
		}

		/* set in info */
		inbytesleft=prebuf_len;
		/* set out info */
		o=outbuf;
		outbytesleft=sizeof(outbuf);

		int len_to_write;
		int written;

		if(cd) {
			if( (iconv(cd,&p,&inbytesleft,&o,&outbytesleft)==(size_t)-1) && (outbytesleft==sizeof(outbuf)) ) {
				SYSLOG(LOG_ERROR,"###sms### failed to convert #1 (from=%s,to=%s)",_sms_code_to_table[code_from],_input_coding_scheme);
				goto err;
			}

			/* we do not expect any leftover */
			if(inbytesleft) {
				SYSLOG(LOG_ERROR,"###sms### too big sms text detected (outbytesleft=%d)",outbytesleft);
				goto err;
			}

			len_to_write=sizeof(outbuf)-outbytesleft;
			p=outbuf;
		}
		else {
			len_to_write=prebuf_len;
		}

		SYSLOG(LOG_OPERATION,"###sms## write into inbox file (new_inbox_file=%d,len_to_write=%d)",inbox_idx,len_to_write);

		/* write */
		if(udh_concat)
			SYSLOG(LOG_OPERATION,"###sms### write message body to inbox (inbox_idx=%d,len=%d,ref=%d,idx=%d,max_idx=%d)",inbox_idx,len_to_write,udh_concat->ref,udh_concat->seq_no,udh_concat->max_no);
		else
			SYSLOG(LOG_OPERATION,"###sms### write message body to inbox (inbox_idx=%d,len=%d)",inbox_idx,len_to_write);


		written=write(fd,p,len_to_write);
		if(written<0) {
			SYSLOG(LOG_ERROR,"###sms### failed to write SMS text (err=%s)",strerror(errno));
			goto err;
		}
	}

	/* notify rdb */
	qmimgr_callback_on_schedule_sms_spool(NULL);

	rc=0;
fini:
	if(cd)
		iconv_close(cd);

	if(!(fd<0))
		close(fd);

	return rc;

err:
	rc=-1;
	goto fini;
}

int _qmi_sms_eat_last_in_pool()
{
	struct sms_body_rchain_t* ce;
	struct sms_body_rchain_t* pce;
	struct sms_body_rchain_t* wc=NULL;
	struct gsm_user_data_concat_t* udh_concat;

	ce=_rchain;
	if(!ce)
		goto fini;

	if(!ce->processed)
		goto fini;

	/* do not rely on mms flag since the flag is not reliable sometimes. use the existance of udh concat header only */
	/* bypass if we need to wait for more messages */
	udh_concat=ce->udh_concat;
	if(udh_concat && (udh_concat->max_no!=udh_concat->seq_no)) {
		SYSLOG(LOG_OPERATION,"###sms### [MSG] incomplete message body received (ref=%d,idx=%d,max_idx=%d)",udh_concat->ref,udh_concat->seq_no,udh_concat->max_no);
		goto fini;
	}
	else {
		if(udh_concat)
			SYSLOG(LOG_OPERATION,"###sms### [MSG] complete message body received, start processing (ref=%d,idx=%d,max_idx=%d)",udh_concat->ref,udh_concat->seq_no,udh_concat->max_no);
		else
			SYSLOG(LOG_OPERATION,"###sms### [MSG] single body message received, start processing");
	}

	/* we got the last body, start rock and roll!  */
	struct sms_body_rchain_t* lce;
	/* store the last ce */
	lce=ce;

	while(ce) {
		pce=(struct sms_body_rchain_t*)ce->list.prev;

		/* remove from read-chain pool */
		if(ce==_rchain)
			_rchain=pce;
		_qmi_sms_remove_from_pool(ce);

		/* insert to write-chain */
		if(wc)
			linkedlist_insert(&wc->list,&ce->list);

		/* update wc */
		wc=ce;

		/* break if single message */
		udh_concat=ce->udh_concat;
		if(!udh_concat) {
			SYSLOG(LOG_DEBUG,"###sms### prepare a message to write (ce=%p)",ce);
			break;
		}
		else {
			SYSLOG(LOG_DEBUG,"###sms### prepare messages to write (ref=%d,idx=%d,max_idx=%d,ce=%p)",udh_concat->ref,udh_concat->seq_no,udh_concat->max_no,ce);
		}

		/* search body parts */
		while( (ce=pce)!= NULL ) {

			pce=(struct sms_body_rchain_t*)ce->list.prev;

			/* bypass if no concat */
			if(!ce->udh_concat)
				continue;

			/* bypass if ref not matching */
			if(ce->udh_concat->ref!=lce->udh_concat->ref)
				continue;

			/* bypass if oa length not matching */
			if(ce->oa_len!=lce->oa_len)
				continue;

			/* bypass if oa not matching */
			if(memcmp(ce->oa,lce->oa,ce->oa_len))
				continue;

			break;
		}

	}

	/* flush to file */
	if(_qmi_sms_flush_celements_to_file(wc)<0)
		SYSLOG(LOG_ERROR,"###sms### failed to flush message");

	/* destory all entries write-chain */
	while((ce=wc)!=NULL) {
		SYSLOG(LOG_DEBUG,"###sms### destroy message from write-chain (ce=%p)",ce);

		wc=(struct sms_body_rchain_t*)ce->list.next;
		_qmi_sms_del_from_pool(ce);
	}

	SYSLOG(LOG_DEBUG,"###sms### read-chain header (ce=%p)",_rchain);

fini:
	return 0;
}

typedef int (*_qmi_sms_cdma_decode_params_func_t)(struct cdma_sms_param_hdr_t* phdr,void* ref);

int _qmi_sms_cdma_decode_params(char* buf,int buf_len,_qmi_sms_cdma_decode_params_func_t func,void* ref)
{
	struct cdma_sms_param_hdr_t* phdr;
	int i;
	int ps;

	i=0;
	while(i<buf_len) {

		/* get parameter header */
		phdr=(struct cdma_sms_param_hdr_t*)(buf+i);

		/* get parameter size */
		ps=sizeof(*phdr)+phdr->param_len;

		/* check validation of parameter length */
		if((i+ps)>buf_len) {
			SYSLOG(LOG_ERR,"###sms## incorrect parameter size detected (base=%d,ps=%d,tlen=%d)",i,ps,buf_len);
			goto err;
		}

		/* call call back */
		if(func(phdr,ref)<0) {
			SYSLOG(LOG_ERR,"###sms## failed in callback function (id=%d,len=%d)",phdr->param_id,phdr->param_len);
			goto err;
		}

		i+=ps;
	}

	return 0;

err:
	return -1;
}

int _qmi_sms_cook_last_in_pool_cdma_cb_subparams(struct cdma_sms_param_hdr_t* phdr,void* ref)
{
	struct sms_body_rchain_t* ce;

	ce=(struct sms_body_rchain_t*)ref;

	/* store parameter */
	if(phdr->param_id<__countof(ce->sphdrs)) {
		ce->sphdrs[phdr->param_id]=phdr;

		SYSLOG(LOG_DEBUG,"###sms## parse sub-parameters [bearer_data] (msg_type=%d,spid=%d,splen=%d)",ce->thdr->msg_type,phdr->param_id,phdr->param_len);
	}
	else {
		SYSLOG(LOG_ERR,"###sms## unsupported sub-parameter id found (pid=%d,plen=%d)",phdr->param_id,phdr->param_len);
	}

	return 0;

}
int _qmi_sms_cook_last_in_pool_cdma_cb_params(struct cdma_sms_param_hdr_t* phdr,void* ref)
{
	struct sms_body_rchain_t* ce;
	char* sbuf;

	ce=(struct sms_body_rchain_t*)ref;

	/* store parameter */
	if(phdr->param_id<__countof(ce->phdrs)) {
		ce->phdrs[phdr->param_id]=phdr;

		SYSLOG(LOG_DEBUG,"###sms## parse parameters (msg_type=%d,pid=%d,plen=%d)",ce->thdr->msg_type,phdr->param_id,phdr->param_len);
	}
	else {
		SYSLOG(LOG_ERR,"###sms## unsupported parameter id found (msg_type=%d,pid=%d,plen=%d)",ce->thdr->msg_type,phdr->param_id,phdr->param_len);
	}

	/* store subparameters of bearer */
	if(phdr->param_id==param_id_bearer_data) {

		sbuf=phdr->param;

		if(_qmi_sms_cdma_decode_params(sbuf,phdr->param_len,_qmi_sms_cook_last_in_pool_cdma_cb_subparams,ce)<0) {
			SYSLOG(LOG_ERR,"###sms## failed in _qmi_sms_cdma_decode_params() - (pid=%d,plen=%d)",phdr->param_id,phdr->param_len);
			goto err;
		}
	}


	return 0;

err:
	return -1;
}


int cdma_getset_bit_data(int get,char* dst,int dst_len,int* dst_bit_off,unsigned int* val,int val_bit_len)
{
	int i;

	int dst_idx;
	int dst_bit_idx;
	int dst_mask;

	int src_bit_idx;
	int src_mask;

	int bit_per_char;

	int off;

	/* reset result value */
	if(get)
		*val=0;

	if(val_bit_len<0)
		goto err;


	if(dst_bit_off)
		off=*dst_bit_off;
	else
		off=0;

	bit_per_char=sizeof(char)*8;

	/* check length */
	if( (dst_len*bit_per_char) < off+val_bit_len )
		goto err;

	for(i=0;i<val_bit_len;i++) {

		/* get dst info */
		dst_idx=(off+i)/bit_per_char;
		dst_bit_idx=bit_per_char-1-((off+i)%bit_per_char);
		dst_mask=1<<dst_bit_idx;

		/* get src info */
		src_bit_idx=val_bit_len-1-i;
		src_mask=1<<src_bit_idx;

		if(get) {
			if( dst[dst_idx] & dst_mask)
				*val|=src_mask;
			else
				*val&=~src_mask;
		}
		else {
			if( *val & src_mask )
				dst[dst_idx]|=dst_mask;
			else
				dst[dst_idx]&=~dst_mask;
		}
	}

	if(dst_bit_off)
		(*dst_bit_off)+=val_bit_len;

	return val_bit_len;

err:
	return -1;
}

int cdma_get_bit_data(const char* src,int src_len,int* src_bit_off,int* val,int val_bit_len)
{
	return cdma_getset_bit_data(1,(char*)src,src_len,src_bit_off,(unsigned int*)val,val_bit_len);
}

int cdma_set_bit_data(char* dst,int dst_len,int* dst_bit_off,int val,int val_bit_len)
{
	return cdma_getset_bit_data(0,dst,dst_len,dst_bit_off,(unsigned int*)&val,val_bit_len);

}

int _qmi_sms_cook_last_in_pool_cdma()
{
	struct sms_body_rchain_t* ce;
	struct cdma_sms_param_hdr_t* phdr;

	char* pbuf;
	int pbuf_len;

	ce=_rchain;

	/* nothing to process */
	if(!ce)
		goto fini;

	/* bypass if processed */
	if(ce->processed)
		goto fini;

	/* get transport layer header */
	ce->thdr=(struct cdma_sms_trans_layer_hdr_t*)ce->buf;

	/* get parameter buffer and buffer length */
	pbuf=ce->thdr->payload;
	pbuf_len=ce->buf_len-sizeof(*ce->thdr);
	if(pbuf_len<0) {
		SYSLOG(LOG_ERR,"###sms## too short transport packet received");
		goto err;
	}

	/* check transport layer message type */
	if(ce->thdr->msg_type!=sms_msg_p2p) {
		SYSLOG(LOG_ERR,"###sms## transport layer message type not supported (msg_type=%d)",ce->thdr->msg_type);
		goto err;
	}


	SYSLOG(LOG_DEBUG,"###sms## parse parameters (msg_type=%d)",ce->thdr->msg_type);

	/* decode cdma parameters */
	if(_qmi_sms_cdma_decode_params(pbuf,pbuf_len,_qmi_sms_cook_last_in_pool_cdma_cb_params,ce)<0) {
		SYSLOG(LOG_ERR,"###sms## failed in _qmi_sms_cdma_decode_params()");
		goto err;
	}

	char* buf;

	/* parse teleservice_id parameter */
	struct cdma_sms_param_teleserv_id_t* pts;
	int id;

	pts=(struct cdma_sms_param_teleserv_id_t*)ce->phdrs[param_id_teleservice_id];
	if(!pts) {
		SYSLOG(LOG_ERR,"###sms## teleservice parameter not found");
		goto err;
	}
	else {
		cdma_get_bit_data((char*)&pts->id,sizeof(pts->id),NULL,&id,sizeof(pts->id)*8);

		SYSLOG(LOG_DEBUG,"###sms## teleservice id received (id=0x%04x)",id);

		if(id!=cdma_sms_tele_id_cmt_95) {
			SYSLOG(LOG_ERROR,"###sms## teleservice id not supported (id=0x%04x)",id);
			goto err;
		}
	}

	/* parse user_data sub-parameter in bearer parameter */
	phdr=ce->sphdrs[user_data];
	if(!phdr) {
		SYSLOG(LOG_ERR,"###sms## user data not found in bearer parameter");
		goto err;
	}
	else {
		/* get parameter payload */
		buf=phdr->param;

		/* cdma does not support concat */
		ce->udh_concat=NULL;
		ce->total_udhl=0;

		/* set ud information */
		ce->ud=buf;
		ce->ud_msg=buf;

		/* set unknown yet */
		int msg_encoding;
		int msg_type;
		int num_fields;

		if(cdma_decode_subparam_userdata(phdr,&msg_encoding,&msg_type,&num_fields,NULL,0)<0) {
			SYSLOG(LOG_ERR,"###sms## malformed user data found");
			goto err;
		}

		/* get septet or octet length */
		ce->ud_msg_len=num_fields;
		/* get encoding scheme */
		ce->code_from=cdma_get_sms_msg_type_by_encoding(msg_encoding);
		if(ce->code_from==dcs_unknown) {
			SYSLOG(LOG_ERR,"###sms## message encoding type not supported (msg_encoding=%d)",msg_encoding);
			goto err;
		}
	}

	/* set processed flag */
	ce->processed=1;

fini:
	return 0;

err:
	/* set broken flag */
	ce->broken=1;

	return -1;
}

int _qmi_sms_get_last_sms_type_in_pool(enum sms_msg_type_t* sms_type)
{
	struct sms_body_rchain_t* ce;

	ce=_rchain;

	/* nothing to process */
	if(!ce)
		return -1;

	*sms_type=ce->sms_type;

	return 0;
}

int _qmi_sms_cook_last_in_pool()
{
	struct sms_body_rchain_t* ce;

	ce=_rchain;

	/* nothing to process */
	if(!ce)
		goto fini;

	/* bypass if processed */
	if(ce->processed)
		goto fini;

	int pdu_idx=0;

	/* get SMSC address */
	ce->smsc=(struct gsm_address_fields_t*)(ce->buf+pdu_idx);
	ce->smsc_len=sizeof(ce->smsc->address_length)+ce->smsc->address_length;
	pdu_idx+=ce->smsc_len;

	/* get TP-MTI, TP-MMS, TP-LP, TP-SRI, TPUDHI, TP-RP */
	ce->type=(struct gsm_sms_deliver_type_t*)(ce->buf+pdu_idx);
	pdu_idx+=sizeof(*(ce->type));

	/* get TP-Originating-Address */
	ce->oa=(struct gsm_address_fields_t*)(ce->buf+pdu_idx);
	ce->oa_len=sizeof(*(ce->oa))+(ce->oa->address_length+1)/2;
	pdu_idx+=ce->oa_len;

	/* get TP-Protocol-Identifier */
	ce->pid=(unsigned char*)(ce->buf+pdu_idx);
	pdu_idx+=sizeof(*(ce->pid));

	/* get TP-Data-Coding-Scheme */
	ce->dcs=(unsigned char*)(ce->buf+pdu_idx);
	pdu_idx+=sizeof(*(ce->dcs));


	/* convert dcs to code scheme */
	enum sms_code_type_t code_from;
	int group;
	int param;
	int dcs;

	/* parse dcs */
	dcs=*ce->dcs;
	group=(dcs&0xf0)>>4;
	param=dcs&0x0f;

	/*  3GPP DCS - Any reserved codings shall be assumed to be the GSM 7 bit default alphabet (the same as codepoint 00000000) by a receiving entity */
	code_from=dcs_7bit_coding;

	switch(group) {
		case 0:
		case 1:
		case 2:
		case 3: {
			int charset;
			/*
				General Data Coding indication
				Bits 5..0 indicate the following:
				Bit 5, if set to 0, indicates the text is uncompressed
				Bit 5, if set to 1, indicates the text is compressed using the compression algorithm defined
				in 3GPP TS 23.042 [13]
				Bit 4, if set to 0, indicates that bits 1 to 0 are reserved and have no message class
				meaning
				Bit 4, if set to 1, indicates that bits 1 to 0 have a message class meaning::
				Bit 1 Bit 0 Message Class
				0 0 Class 0
				0 1 Class 1 Default meaning: ME-specific.
				1 0 Class 2 (U)SIM specific message
				1 1 Class 3 Default meaning: TE specific (see 3GPP TS 27.005 [8])
				Bits 3 and 2 indicate the character set being used, as follows :
				Bit 3 Bit2 Character set:
				0 0 GSM 7 bit default alphabet
				0 1 8 bit data
				1 0 UCS2 (16bit) [10]
				1 1 Reserved
				NOTE: The special case of bits 7..
			*/

			/* get charset */
			charset=(param>>2)&0x03;
			/* assume 7bit if reserved */
			if(charset==0x03)
				charset=dcs_7bit_coding;

			code_from=charset;
			break;
		}

		case 4:
		case 5:
		case 6:
		case 7: {
			/*
				Message Marked for Automatic Deletion Group
				This group can be used by the SM originator to mark the message ( stored in the ME or
				(U)SIM ) for deletion after reading irrespective of the message class.
				The way the ME will process this deletion should be manufacturer specific but shall be
				done without the intervention of the End User or the targeted application. The mobile
				manufacturer may optionally provide a means for the user to prevent this automatic
				deletion.
				Bit 5..0 are coded exactly the same as Group 00xx
			*/
			break;
		}

		case 8:
		case 9:
		case 10:
		case 11: {
			/*
				Reserved coding groups
			*/
			break;
		}

		case 12: {
			/*
				Message Waiting Indication Group: Discard Message
				The specification for this group is exactly the same as for Group 1101, except that:
				- after presenting an indication and storing the status, the ME may discard the contents
				of the message.
				The ME shall be able to receive, process and acknowledge messages in this group,
				irrespective of memory availability for other types of short message.
			*/

			break;
		}

		case 13: {
			/*
				Message Waiting Indication Group: Store Message
				This Group defines an indication to be provided to the user about the status of types of
				message waiting on systems connected to the GSM/UMTS PLMN. The ME should present
				this indication as an icon on the screen, or other MMI indication. The ME shall update the
				contents of the Message Waiting Indication Status on the SIM (see 3GPP TS 51.011 [18])
				or USIM (see 3GPP TS 31.102 [17]) when present or otherwise should store the status in
				the ME. In case there are multiple records of EFMWIS this information shall be stored within
				the first record. The contents of the Message Waiting Indication Status should control the
				ETSI
				3GPP TS 23.038 version 8.3.0 Release 8 9 ETSI TS 123 038 V8.3.0 (2010-01)
				ME indicator. For each indication supported, the mobile may provide storage for the
				Origination Address. The ME may take note of the Origination Address for messages in
				this group and group 1100.
				Text included in the user data is coded in the GSM 7 bit default alphabet.
				Where a message is received with bits 7..4 set to 1101, the mobile shall store the text of
				the SMS message in addition to setting the indication. The indication setting should take
				place irrespective of memory availability to store the short message.
				Bits 3 indicates Indication Sense:
				Bit 3
				0 Set Indication Inactive
				1 Set Indication Active
				Bit 2 is reserved, and set to 0
				Bit 1 Bit 0 Indication Type:
				0 0 Voicemail Message Waiting
				0 1 Fax Message Waiting
				1 0 Electronic Mail Message Waiting
				1 1 Other Message Waiting*
				* Mobile manufacturers may implement the "Other Message Waiting" indication as an
				additional indication without specifying the meaning.
			*/
			break;
		}

		case 14: {
			/*
				Message Waiting Indication Group: Store Message
				The coding of bits 3..0 and functionality of this feature are the same as for the Message
				Waiting Indication Group above, (bits 7..4 set to 1101) with the exception that the text
				included in the user data is coded in the uncompressed UCS2 character set.
			*/
			break;
		}

		case 15: {
			/*
				Data coding/message class
				Bit 3 is reserved, set to 0.
				Bit 2 Message coding:
				0 GSM 7 bit default alphabet
				1 8-bit data
				Bit 1 Bit 0 Message Class:
				0 0 Class 0
				0 1 Class 1 default meaning: ME-specific.
				1 0 Class 2 (U)SIM-specific message.
				1 1 Class 3 default meaning: TE specific (see 3GPP TS 27.005 [8])
			*/

			code_from=(param>>2)&0x01;
			break;
		}
	}

	SYSLOG(LOG_OPERATION,"###sms## dcs=0x%02x, code_from=%d",dcs,code_from);
	ce->code_from=code_from;


	/* get TP-Service-Centre-Time-Stamp */
	ce->scts=(struct gsm_timestamp_t*)(ce->buf+pdu_idx);
	pdu_idx+=sizeof(*(ce->scts));
	/* get TP-User-Data-Length */
	ce->udl=(unsigned char*)(ce->buf+pdu_idx);
	pdu_idx+=sizeof(*(ce->udl));

	/* get ud and its information */
	ce->ud=ce->buf+pdu_idx;
	ce->udh_concat=NULL;
	ce->ud_msg=ce->buf+pdu_idx;
	ce->ud_msg_len=*ce->udl;
	ce->total_udhl=0;

	if(ce->type->udhi && ce->ud_msg_len) {
		struct gsm_user_data_header_t* udh;
		struct gsm_user_data_sub_header_t* udsh;

		/* get udh */
		udh=(struct gsm_user_data_header_t*)(ce->buf+pdu_idx);
		pdu_idx+=sizeof(*udh);

		/* search concat header */
		int pdu_idx_udh;
		pdu_idx_udh=0;
		while(pdu_idx_udh+sizeof(*udsh)<=udh->udhl) {
			udsh=(struct gsm_user_data_sub_header_t*)(ce->buf+pdu_idx+pdu_idx_udh);

			if(udsh->iei==0x00)
				ce->udh_concat=(struct gsm_user_data_concat_t*)(udsh+1);

			pdu_idx_udh+=sizeof(*udsh)+udsh->iedl;
		}

		pdu_idx+=udh->udhl;

		/* get msg */
		ce->total_udhl=sizeof(*udh)+udh->udhl;
		ce->ud_msg=ce->buf+pdu_idx;

		/* septet count for 7 bit encoding */
		if(ce->code_from==dcs_7bit_coding)
			ce->ud_msg_len=*ce->udl-((pdu_idx_udh+sizeof(*udh))*8+6)/7;
		else
			ce->ud_msg_len=*ce->udl-(pdu_idx_udh+sizeof(*udh));
	}

	ce->processed=1;

fini:
	return 0;
}


int _qmi_sms_read(enum sms_msg_type_t sms_type,unsigned char storage_type,unsigned int msg_idx,unsigned char* sms_on_ims,unsigned char* tag_type,unsigned char* format,char* buf,int* buf_len)
{
	#ifdef CONFIG_CDMA_SMS_TEST_FAKEDATA
/*
	char cdma_dat[]={
		0x00,0x00,0x02,0x10,0x05,0x02,0x09,0x03,
		0xa8,0x45,0x85,0x11,0xd0,0x66,0xa1,0xc0,
		0x06,0x01,0xfc,0x08,0xa0,0x00,0x03,0x1a,
		0x56,0x58,0x01,0x8e,0x4d,0x00,0x28,0x00,
		0x18,0x30,0x10,0x0b,0x01,0x8e,0xcc,0x62,
		0xb5,0x9e,0xeb,0x81,0xc8,0x81,0x61,0x34,
		0xde,0x8b,0x55,0xb0,0xd9,0x70,0x3c,0x16,
		0x2b,0x21,0x9a,0xd6,0x6b,0xb6,0x1f,0x2a,
		0x05,0x84,0xc3,0x62,0x35,0x5e,0xcb,0x75,
		0xc0,0xe0,0x40,0xb4,0x9e,0x6b,0x41,0xae,
		0xd8,0x6c,0xb8,0x1e,0x0b,0x15,0x90,0xcd,
		0x6b,0x35,0xdf,0x0b,0x91,0x02,0xc2,0x61,
		0xb5,0x1e,0xab,0x61,0xba,0xe0,0x70,0x24,
		0x5e,0x4b,0x35,0xa4,0xd7,0x6c,0x36,0x5c,
		0x0b,0x01,0x8e,0xcc,0x62,0xb5,0x9e,0xeb,
		0x81,0xc8,0x81,0x61,0x34,0xde,0x8b,0x55,
		0xb0,0xd9,0x70,0x3c,0x16,0x2b,0x21,0x9a,
		0xd6,0x6b,0xb6,0x1f,0x2a,0x05,0x84,0xc3,
		0x62,0x35,0x5e,0xcb,0x75,0xc0,0xe0,0x40,
		0xb4,0x9e,0x6b,0x41,0xae,0xd8,0x6c,0xb8,
		0x1e,0x0b,0x15,0x90,0xcd,0x6b,0x35,0xdf,
		0x0b,0x90,0x03,0x06,0x14,0x10,0x09,0x00,
		0x34,0x27,0x08,0x01,0x40
	};
*/

	char cdma_dat1[]={
		0x00,0x00,0x02,0x10,0x05,0x02,0x09,0x03,0xA8,0x45,0x85,0x11,0xD0,0x66,0xA1,0xC0,
		0x06,0x01,0xFC,0x08,0x44,0x00,0x03,0x1B,0x26,0x88,0x01,0x32,0x49,0xB0,0x28,0x00,
		0x18,0x08,0x10,0x13,0x01,0x80,0xC0,0x60,0x34,0x1E,0x0B,0x01,0x80,0xC0,0x60,0x34,
		0x1E,0x0B,0x01,0x80,0xC0,0x60,0x34,0x1E,0x0B,0x01,0x80,0xC0,0x60,0x34,0x1E,0x0B,
		0x01,0x80,0xC0,0x60,0x34,0x1E,0x0B,0x01,0x80,0xC0,0x60,0x34,0x18,0x08,0x03,0x06,
		0x14,0x10,0x09,0x02,0x00,0x34,0x08,0x01,0x40

	};

	char cdma_dat2[]={
		0x00,0x00,0x02,0x10,0x05,0x02,0x09,0x03,0xA8,0x45,0x85,0x11,0xD0,0x66,0xA1,0xC0,
		0x06,0x01,0xFC,0x08,0xA0,0x00,0x03,0x1B,0x2E,0xC8,0x01,0x8E,0x4D,0x00,0x28,0x00,
		0x18,0x10,0x10,0x0B,0x01,0x80,0xC0,0x60,0x34,0x1E,0x0B,0x01,0x80,0xC0,0x60,0x34,
		0x1E,0x0B,0x01,0x80,0xC0,0x60,0x34,0x1E,0x0B,0x01,0x80,0xC0,0x60,0x34,0x1E,0x0B,
		0x01,0x80,0xC0,0x60,0x34,0x1E,0x0B,0x01,0x80,0xC0,0x60,0x34,0x1E,0x0B,0x01,0x80,
		0xC0,0x60,0x34,0x1E,0x0B,0x01,0x80,0xC0,0x60,0x34,0x1E,0x0B,0x01,0x80,0xC0,0x60,
		0x34,0x1E,0x0B,0x01,0x80,0xC0,0x60,0x34,0x1E,0x0B,0x01,0x80,0xC0,0x60,0x34,0x1E,
		0x0B,0x01,0x80,0xC0,0x60,0x34,0x1E,0x0B,0x01,0x80,0xC0,0x60,0x34,0x1E,0x0B,0x01,
		0x80,0xC0,0x60,0x34,0x1E,0x0B,0x01,0x80,0xC0,0x60,0x34,0x1E,0x0B,0x01,0x80,0xC0,
		0x60,0x34,0x1E,0x0B,0x01,0x80,0xC0,0x60,0x34,0x1E,0x0B,0x01,0x80,0xC0,0x60,0x34,
		0x1E,0x0B,0x01,0x80,0xC0,0x60,0x34,0x1E,0x0B,0x00,0x03,0x06,0x14,0x10,0x09,0x02,
		0x06,0x27,0x08,0x01,0x40
	};

	static int i=0;

	if((i++&0x01) == 0) {
		memcpy(buf,cdma_dat1,sizeof(cdma_dat1));
		*buf_len=sizeof(cdma_dat1);
	}
	else {
		memcpy(buf,cdma_dat2,sizeof(cdma_dat2));
		*buf_len=sizeof(cdma_dat2);
	}

	return 0;
	#endif

	struct qmi_easy_req_t er;
	int rc;

	const struct qmitlv_t* tlv;

	struct qmi_wms_raw_read_resp* resp;
	int msg_mode;

	struct qmi_wms_raw_read_req_storage_type req;

	/* init req */
	rc=_qmi_easy_req_init(&er,QMIWMS,QMI_WMS_RAW_READ);
	if(rc<0)
		goto err;

	/* build tlvs */

	memset(&req,0,sizeof(req));
	req.storage_type=storage_type;
	req.storage_index=msg_idx;
	qmimsg_add_tlv(er.msg,QMI_WMS_RAW_READ_REQ_TYPE_STORAGE_TYPE,sizeof(req),&req);

	/* get msg mode */
	msg_mode=_qmi_sms_get_msg_mode(sms_type);
	if(msg_mode<0) {
		SYSLOG(LOG_ERROR,"###sms### [read] incorrect sms type detected (sms_type=%d)",sms_type);
		goto err;
	}
	qmimsg_add_tlv(er.msg,QMI_WMS_RAW_READ_REQ_TYPE_MSG_MODE,sizeof(char),(char*)&msg_mode);

	if(sms_on_ims)
		qmimsg_add_tlv(er.msg,QMI_WMS_RAW_READ_REQ_TYPE_SMS_ON_IMS,sizeof(*sms_on_ims),sms_on_ims);

	/* init. tlv */
	rc=_qmi_easy_req_do(&er,0,0,NULL,QMIMGR_GENERIC_RESP_TIMEOUT);
	if(rc<0)
		goto err;

	/* get tlv */
	tlv=_get_tlv(er.rmsg,QMI_WMS_RAW_READ_RESP_TYPE,sizeof(*resp));
	if(!tlv) {
		SYSLOG(LOG_ERROR,"###sms### failed to get QMI_WMS_RAW_READ_RESP_TYPE (msg_idx=%d)",msg_idx);
		goto err;
	}

	resp=tlv->v;

	if(*buf_len<resp->len) {
		SYSLOG(LOG_ERROR,"###sms### insufficent buffer size (psize=%d,buf_len=%d)",resp->len,*buf_len);
		goto err;
	}

	if(tag_type)
		*tag_type=resp->tag_type;
	if(format)
		*format=resp->format;

	*buf_len=resp->len;

	memcpy(buf,resp->data,resp->len);

	_dump(LOG_DUMP,__FUNCTION__,resp->data,resp->len);


	_qmi_easy_req_fini(&er);
	return 0;

err:
	_qmi_easy_req_fini(&er);
	return -1;
}

int _qmi_sms_get_route(struct qmi_wms_route_t* r_buf, int r_cnt)
{
	struct qmi_easy_req_t er;
	int rc;
	const struct qmitlv_t* tlv;

	struct qmi_wms_route_list* resp;

/* init req */
	rc=_qmi_easy_req_init(&er,QMIWMS,QMI_WMS_GET_ROUTES);
	if(rc<0)
		goto err;

	/* init. tlv */
	rc=_qmi_easy_req_do(&er,0,0,NULL,QMIMGR_GENERIC_RESP_TIMEOUT);
	if(rc<0)
		goto err;

	tlv=_get_tlv(er.rmsg,QMI_WMS_GET_ROUTES_RESP_TYPE_ROUTE_LIST,sizeof(*resp));
	if(!tlv) {
		SYSLOG(LOG_ERROR,"###sms### failed to get QMI_WMS_GET_ROUTES_RESP_TYPE_ROUTE_LIST");
		goto err;
	}

	resp=tlv->v;

	/* return routes */
	int min;
	min=(r_cnt<resp->n_routes)?r_cnt:resp->n_routes;
	memcpy(r_buf,resp->n,sizeof(*r_buf)*min);

	_qmi_easy_req_fini(&er);
	return min;

err:
	_qmi_easy_req_fini(&er);
	return -1;
}

int _qmi_sms_set_route(struct qmi_wms_route_t* r_buf, int r_cnt)
{
	struct qmi_easy_req_t er;
	int rc;

	struct qmi_wms_route_list* req;
	int req_len;

	if(!r_cnt)
		goto fini;

	/* build req */
	req_len=sizeof(*req)+(sizeof(*r_buf)*r_cnt);
	req=(struct qmi_wms_route_list*)alloca(req_len);

	req->n_routes=r_cnt;
	memcpy(req->n,r_buf,sizeof(*req->n)*r_cnt);

	/* init req */
	rc=_qmi_easy_req_init(&er,QMIWMS,QMI_WMS_SET_ROUTES);
	if(rc<0)
		goto err;

	qmimsg_add_tlv(er.msg,QMI_WMS_SET_ROUTES_REQ_TYPE_ROUTE_LIST,req_len,req);

	/* init. tlv */
	rc=_qmi_easy_req_do(&er,0,0,NULL,QMIMGR_GENERIC_RESP_TIMEOUT);
	if(rc<0)
		goto err;

	_qmi_easy_req_fini(&er);

fini:
	return 0;

err:
	_qmi_easy_req_fini(&er);
	return -1;
}

/* apply nv settings */
int _qmi_sms_init_nv_route()
{

	struct qmi_wms_route_t r[SMS_ROUTE_MAX_COUNT];
	int c;
	int i;

	struct qmi_wms_route_t* r_ptr;

	/* get route */
	c=_qmi_sms_get_route(r,SMS_ROUTE_MAX_COUNT);
	if(c<0) {
		SYSLOG(LOG_ERROR,"###sms### failed to get sms routes");
		goto err;
	}

	SYSLOG(LOG_DEBUG,"###sms### route list");
	for(i=0;i<c;i++) {
		r_ptr=&r[i];
		SYSLOG(LOG_DEBUG,"###sms### idx=%d message_type=%d, message_class=%d, route_memory=%d,route_value=%d",i,r_ptr->message_type, r_ptr->message_class,r_ptr->route_memory,r_ptr->route_value);
	}

	/* set route - currently only NV memory */
	for(i=0;i<c;i++) {
		r[i].route_memory=0x01;
	}

	/* set route */
	if(_qmi_sms_set_route(r,c)<0) {
		SYSLOG(LOG_ERROR,"###sms### failed to set sms routes (c=%d)",c);
		goto err;
	}

	return 0;
err:
	return -1;
}

int _qmi_sms_change_tag(enum sms_msg_type_t sms_type,unsigned char storage_type,unsigned int storage_index,unsigned char tag_type)
{
	struct qmi_easy_req_t er;
	int rc;

	struct qmi_wms_modify_tag_req req;
	int msg_mode;

	/* get sms format - cdma or umts? */
	msg_mode=_qmi_sms_get_msg_mode(sms_type);
	if(msg_mode<0) {
		SYSLOG(LOG_ERROR,"###sms### [chgtag] incorrect sms type detected (sms_type=%d)",sms_type);
		goto err;
	}

	/* init req */
	rc=_qmi_easy_req_init(&er,QMIWMS,QMI_WMS_MODIFY_TAG);
	if(rc<0)
		goto err;

	/* build req - QMI_WMS_MODIFY_TAG_REQ_TYPE */
	memset(&req,0,sizeof(req));
	req.storage_type=storage_type;
	req.storage_index=storage_index;
	req.tag_type=tag_type;
	qmimsg_add_tlv(er.msg,QMI_WMS_MODIFY_TAG_REQ_TYPE,sizeof(req),&req);

	/* build req - QMI_WMS_MODIFY_TAG_REQ_TYPE_MSG_MODE */
	qmimsg_add_tlv(er.msg,QMI_WMS_MODIFY_TAG_REQ_TYPE_MSG_MODE,sizeof(char),&msg_mode);

	/* init. tlv */
	rc=_qmi_easy_req_do(&er,0,0,NULL,QMIMGR_GENERIC_RESP_TIMEOUT);
	if(rc<0)
		goto err;

	_qmi_easy_req_fini(&er);
	return 0;

err:
	_qmi_easy_req_fini(&er);
	return -1;
}

void _qmi_sms_collect_garbage()
{
	struct sms_body_rchain_t* ce;
	struct sms_body_rchain_t* pce;

	clock_t cur;

	struct gsm_user_data_concat_t* udh_concat;

	/* get current tick */
	cur=_get_current_sec();


	pce=_rchain;
	while((ce=pce)!=NULL) {
		pce=(struct sms_body_rchain_t*)ce->list.prev;

		if(ce->broken) {
			SYSLOG(LOG_ERROR,"###sms### delete broken message");
		}
		else {
			/* bypass if not older than expiry time */
			if(cur-ce->queue_ts<SMS_INBOX_MEMORY_EXPIRY_SEC)
				continue;

			udh_concat=ce->udh_concat;
			if((ce->sms_type==smt_umts) && udh_concat)
				SYSLOG(LOG_ERROR,"###sms### delete expired message (ref=%d,idx=%d,max_idx=%d)",udh_concat->ref,udh_concat->seq_no,udh_concat->max_no);
			else
				SYSLOG(LOG_ERROR,"###sms### delete expired message");
		}

		/* remove from read-chain pool */
		if(ce==_rchain)
			_rchain=pce;
		_qmi_sms_del_from_pool(ce);

	}
}

int _qmi_sms_read_and_add_to_pool(enum sms_msg_type_t sms_type, unsigned char storage_type,unsigned int msg_idx,unsigned char tag_type)
{
	/* tp-delivery + two of addresses (unknown, so random number 512 ) + UDL (140) */
	int buf_len=sizeof(struct sms_tp_deliver_t) + 512 + GSM_SMS_UD_MAX_LEN;
	char buf[buf_len];

	struct sms_body_rchain_t* ce=NULL;

	clock_t cur;

	/* get current tick */
	cur=_get_current_sec();

	/* get body */
	if(_qmi_sms_read(sms_type,storage_type,msg_idx,NULL,&tag_type,NULL,buf,&buf_len)<0) {
		SYSLOG(LOG_ERROR,"###sms### failed to add msg(#%d) to pool",msg_idx);
		goto err;
	}

	/* alloc a new chain element */
	ce=_malloc(sizeof(*ce));
	if(!ce) {
		SYSLOG(LOG_ERROR,"###sms### failed to allocate a new chain element");
		goto err;
	}

	/* alloc buffer */
	ce->buf=malloc(buf_len);
	if(!ce->buf) {
		SYSLOG(LOG_ERROR,"###sms### failed to allocate buffer for a new chain element (buf_len=%d)",buf_len);
		goto err;
	}

	/* set queue timestamp */
	ce->queue_ts=cur;
	ce->sms_type=sms_type;

	/* setup chain element */
	memcpy(ce->buf,buf,buf_len);
	ce->buf_len=buf_len;

	SYSLOG(LOG_DEBUG,"###sms### add message to read-chain (ce=%p)",ce);

	/* update tail */
	linkedlist_init(&ce->list);
	if(_rchain)
		_rchain=(struct sms_body_rchain_t*)linkedlist_add(&_rchain->list,&ce->list);
	else
		_rchain=ce;

	return 0;

err:
	_qmi_sms_del_from_pool(ce);

	return -1;
}

int _qmi_sms_readall()
{
	unsigned int msg_idx[SMS_READ_POOL_COUNT];
	unsigned char tag_type[SMS_READ_POOL_COUNT];
	int cnt;

	int t;
	int i;

	int stat;

	int retry;

	int tag_mod_failure;

	tag_mod_failure=0;

	enum sms_msg_type_t sms_type;

	for(sms_type=smt_cdma;sms_type<=smt_umts;sms_type++) {

		t=0;
		
		do {

			#ifdef CONFIG_CDMA_SMS_TEST_FAKEDATA
			/* pretend to have a message for the first read */
			if(!t)
				cnt=2;

			msg_idx[0]=0;
			msg_idx[1]=1;
			stat=0;
			#else

			/* get list */
			cnt=SMS_READ_POOL_COUNT;
			retry=0;
			stat=_qmi_sms_get_list_messages(sms_type,QMI_WMS_STORAGE_TYPE_NV,QMI_WMS_TAG_TYPE_MT_NOT_READ,msg_idx,tag_type,&cnt);
			while((stat<0) && (retry++<SMS_INBOX_READ_RETRY)) {
				SYSLOG(LOG_OPERATION,"###sms### retry to get the list message from NV #%d/%d",retry,SMS_INBOX_READ_RETRY);

				cnt=SMS_READ_POOL_COUNT;
				stat=_qmi_sms_get_list_messages(sms_type,QMI_WMS_STORAGE_TYPE_NV,QMI_WMS_TAG_TYPE_MT_NOT_READ,msg_idx,tag_type,&cnt);
			}
			#endif

			/* bypass if error */
			if(stat<0) {
				SYSLOG(LOG_ERR,"###sms### no new message available");
				goto err;
			}

			t+=cnt;

			if(cnt)
				SYSLOG(LOG_OPERATION,"###sms### read %d new message(s)",cnt);

			for(i=0;i<cnt;i++) {

				#ifdef CONFIG_CDMA_SMS_TEST_FAKEDATA
				#else
				SYSLOG(LOG_OPERATION,"###sms### tag as read (idx=%d,tag_type=%d)",msg_idx[i],tag_type[i]);
				stat=_qmi_sms_change_tag(sms_type,QMI_WMS_STORAGE_TYPE_NV,msg_idx[i],QMI_WMS_TAG_TYPE_MT_READ);
				if(stat<0) {
					SYSLOG(LOG_ERROR,"###sms### failed to tag as read (idx=%d,tag_type=%d)",msg_idx[i],tag_type[i]);
					tag_mod_failure=1;
				}
				#endif

				SYSLOG(LOG_OPERATION,"###sms### read new message from NV (idx=%d,tag_type=%d)",msg_idx[i],tag_type[i]);

				/* add a message to pool - retry */
				retry=0;
				stat=_qmi_sms_read_and_add_to_pool(sms_type,QMI_WMS_STORAGE_TYPE_NV,msg_idx[i],QMI_WMS_TAG_TYPE_MT_NOT_READ);
				while((stat<0) && (retry++<SMS_INBOX_READ_RETRY)) {
					SYSLOG(LOG_OPERATION,"###sms### retry to read new message from NV (idx=%d) #%d/%d",msg_idx[i],retry,SMS_INBOX_READ_RETRY);
					stat=_qmi_sms_read_and_add_to_pool(sms_type,QMI_WMS_STORAGE_TYPE_NV,msg_idx[i],QMI_WMS_TAG_TYPE_MT_NOT_READ);
				}

				/* bypass if error */
				if(stat<0) {
					SYSLOG(LOG_OPERATION,"###sms### failed to read new message from NV (idx=%d)",msg_idx[i]);
					continue;
				}

				enum sms_msg_type_t sms_type;

				/* get sms type */
				if(_qmi_sms_get_last_sms_type_in_pool(&sms_type)<0)
					continue;

				switch(sms_type) {
					case smt_umts:
						/* process the last message in the pool */
						_qmi_sms_cook_last_in_pool();
						break;

					case smt_cdma:
						/* process the last message in the pool */
						_qmi_sms_cook_last_in_pool_cdma();
						break;

					default:
						SYSLOG(LOG_ERR,"###sms### unknown sms type found from NV (idx=%d)",msg_idx[i]);
						break;
				}

				/* convert the last message in the pool to file */
				_qmi_sms_eat_last_in_pool();
			}

			/* workaround for unknown failure in modifying tags - do not repeat reading the messages */
			if(tag_mod_failure)
				break;

		} while(cnt);

		if(t)
			SYSLOG(LOG_OPERATION,"###sms### total received new message is %d",t);

		/* delete all read messages */
		{
			unsigned char storage_type;
			unsigned char tag_type;

			SYSLOG(LOG_OPERATION,"###sms### delete all read message(s) from NV (t=%d)",t);
			storage_type=QMI_WMS_STORAGE_TYPE_NV;
			tag_type=QMI_WMS_TAG_TYPE_MT_READ;

			_qmi_sms_delete(sms_type,&storage_type,&tag_type,NULL);
		}

	err:
		while(0);
	}

	/* do garbage collection - TODO: better do this periodically */
	_qmi_sms_collect_garbage();

	return 0;
}


char pdu_decode_address_c_from_i_cdma(int dtmf)
{
	const char c_tbl[]="01234567890*#    ";

	return c_tbl[dtmf&0x0f];
}

int pdu_encode_address_c_to_i_cdma(char ch)
{
	int dtmf;
	switch(ch) {
		case '0':
			dtmf = 0x0A;
			break;
		case '*':
			dtmf = 0x0B;
			break;
		case '#':
			dtmf = 0x0C;
			break;

		default:
			dtmf = ch -'0';
			break;
	}

	return dtmf;
}

#define __size_in_byte(size_in_bit) (((size_in_bit)+8-1)/8)

int cdma_param_header_preserve(struct cdma_sms_param_hdr_t** phdr,char** buf,int* buf_len)
{
	*phdr=(struct cdma_sms_param_hdr_t*)*buf;

	/* increase buffer */
	(*buf)+=sizeof(**phdr);
	*buf_len-=sizeof(**phdr);

	return *buf_len;
}

int cdma_param_header_set(struct cdma_sms_param_hdr_t* phdr,int id,int len)
{
	/* bypass if null */
	if(!phdr)
		return -1;

	phdr->param_id=id;
	phdr->param_len=len;

	return sizeof(*phdr)+len;
}

const char* cdma_decode_address(struct cdma_sms_param_hdr_t* phdr)
{
	static char outbuf[SMS_ADDR_BUF_MAX_LEN];
	int outbuf_len=sizeof(outbuf);

	int param_bit_off=0;

	int dig_mode;
	int num_mode;
	int num_type;
	int num_plan;

	char* buf;
	int buf_len;

	/* set null termination */
	*outbuf=0;

	if(!phdr) {
		SYSLOG(LOG_ERR,"###sms### [cdma] address sub-parameter not found");
		goto fini;
	}

	buf=phdr->param;
	buf_len=phdr->param_len;

	/* param - digital mode */
	if(cdma_get_bit_data(buf,buf_len,&param_bit_off,&dig_mode,1)<0) {
		SYSLOG(LOG_ERR,"###sms### [cdma] digital mode flag not found");
		goto fini;
	}

	/* param - number mode */
	if(cdma_get_bit_data(buf,buf_len,&param_bit_off,&num_mode,1)<0) {
		SYSLOG(LOG_ERR,"###sms### [cdma] number mode flag not found");
		goto fini;
	}

	if(dig_mode==cdma_sms_digit_mode_8_bit) {
		/* param - number type */
		if(cdma_get_bit_data(buf,buf_len,&param_bit_off,&num_type,3)<0) {
			SYSLOG(LOG_ERR,"###sms### [cdma] number type not found");
			goto fini;
		}

		if(num_mode==cdma_sms_number_mode_none_data_network) {
			/* param - number plan */
			if(cdma_get_bit_data(buf,buf_len,&param_bit_off,&num_plan,4)<0) {
				SYSLOG(LOG_ERR,"###sms### [cdma] number plan not found");
				goto fini;
			}
		}
	}

	/* param - number fields */
	int num_fields;
	if(cdma_get_bit_data(buf,buf_len,&param_bit_off,&num_fields,8)<0) {
		SYSLOG(LOG_ERR,"###sms### [cdma] number fields not found");
		goto fini;
	}

	/* check outbuf length */
	if(outbuf_len-1<num_fields) {
		SYSLOG(LOG_ERR,"###sms### [cdma] insufficient outbuf length (num_fields=%d,outbuf_len=%d)",num_fields,outbuf_len);
		goto fini;
	}

	/* param - address */
	int num;
	char* p;
	int i;

	p=outbuf;
	for(i=0;i<num_fields;i++) {

		if(cdma_get_bit_data(buf,buf_len,&param_bit_off,&num,(dig_mode==cdma_sms_digit_mode_8_bit)?8:4)<0) {
			SYSLOG(LOG_ERR,"###sms### [cdma] address fields not found");
			goto fini;
		}

		*p++=(dig_mode==cdma_sms_digit_mode_8_bit)?(char)num:pdu_decode_address_c_from_i_cdma(num);
	}
	*p++=0;

fini:
	return outbuf;
}

int cdma_encode_address(char* buf,int buf_len,int param_id,int dig_mode,int num_mode,int num_type,int num_plan,const char* address)
{
	struct cdma_sms_param_hdr_t* phdr;
	int param_bit_off=0;

	int num_fields;
	const char* addr_buf;
	int num;
	int plus;

	int i;

	/* start param header */
	if(cdma_param_header_preserve(&phdr,&buf,&buf_len)<0) {
		SYSLOG(LOG_ERR,"###sms### [cdma] insufficient buffer size - cannot add parameter header");
		goto err;
	}

	{
		/* param - digital mode */
		if(cdma_set_bit_data(buf,buf_len,&param_bit_off,dig_mode,1)<0) {
			SYSLOG(LOG_ERR,"###sms### [cdma] insufficient buffer size - cannot add digital mode flag");
			goto err;
		}

		/* param - number mode */
		if(cdma_set_bit_data(buf,buf_len,&param_bit_off,num_mode,1)<0) {
			SYSLOG(LOG_ERR,"###sms### [cdma] insufficient buffer size - cannot add number mode flag");
			goto err;
		}

		if(dig_mode==cdma_sms_digit_mode_8_bit) {
			/* param - number type */
			if(cdma_set_bit_data(buf,buf_len,&param_bit_off,num_type,3)<0) {
				SYSLOG(LOG_ERR,"###sms### [cdma] insufficient buffer size - cannot add number type flag");
				goto err;
			}

			if(num_mode==cdma_sms_number_mode_none_data_network) {
				/* param - number plan */
				if(cdma_set_bit_data(buf,buf_len,&param_bit_off,num_plan,4)<0) {
					SYSLOG(LOG_ERR,"###sms### [cdma] insufficient buffer size - cannot add number plan");
					goto err;
				}
			}
		}

		/* get plus sign */
		addr_buf=address;
		plus=(*addr_buf=='+');
		if(plus)
			addr_buf++;

		/* param - number fields */
		num_fields=strlen(addr_buf);
		if(cdma_set_bit_data(buf,buf_len,&param_bit_off,num_fields,8)<0) {
			SYSLOG(LOG_ERR,"###sms### [cdma] insufficient buffer size - cannot add number fields");
			goto err;
		}

		/* param - address */
		for(i=0;i<num_fields;i++) {
			num=(dig_mode==cdma_sms_digit_mode_8_bit)?addr_buf[i]:pdu_encode_address_c_to_i_cdma(addr_buf[i]);

			if(cdma_set_bit_data(buf,buf_len,&param_bit_off,num,(dig_mode==cdma_sms_digit_mode_8_bit)?8:4)<0) {
				SYSLOG(LOG_ERR,"###sms### [cdma] insufficient buffer size - cannot add address");
				goto err;
			}

		}
	}

	return cdma_param_header_set(phdr,param_id,__size_in_byte(param_bit_off));
err:
	return -1;
}

enum sms_msg_type_t cdma_get_sms_msg_type_by_encoding(int msg_encoding)
{
	enum sms_msg_type_t cdma_encoding_to_sms_msg_type_table[]={
		[cdma_bd_sub_msg_enc_8bit_oct]=dcs_8bit_coding,
		[cdma_bd_sub_msg_enc_ext_pm]=dcs_unknown,
		[cdma_bd_sub_msg_enc_7bit_acsii]=dcs_7bit_ascii_coding,
		[cdma_bd_sub_msg_enc_ia5]=dcs_7bit_ascii_coding,
		[cdma_bd_sub_msg_enc_unicode]=dcs_ucs2_coding,
		[cdma_bd_sub_msg_enc_jis]=dcs_8bit_coding,
		[cdma_bd_sub_msg_enc_korean]=dcs_8bit_coding,
		[cdma_bd_sub_msg_enc_latin_heb]=dcs_8bit_coding,
		[cdma_bd_sub_msg_enc_latin]=dcs_8bit_coding,
		[cdma_bd_sub_msg_enc_7bit_gsm]=dcs_7bit_coding,
		[cdma_bd_sub_msg_enc_ext_gsm]=dcs_unknown,
	};

	if(msg_encoding<0 || msg_encoding>=cdma_bd_sub_msg_enc_count)
		return dcs_unknown;

	return cdma_encoding_to_sms_msg_type_table[msg_encoding];
}

int cdma_get_bit_per_char_by_encoding(int msg_encoding)
{
	int bit_per_char;

	switch (msg_encoding)
	{
		case cdma_bd_sub_msg_enc_7bit_acsii:
		case cdma_bd_sub_msg_enc_ia5:
		case cdma_bd_sub_msg_enc_7bit_gsm:
			bit_per_char = 7;
			break;

		case cdma_bd_sub_msg_enc_8bit_oct:
		case cdma_bd_sub_msg_enc_latin_heb:
		case cdma_bd_sub_msg_enc_latin:
			bit_per_char = 8;
			break;

		case cdma_bd_sub_msg_enc_unicode:
			bit_per_char = 16;
			break;

		case cdma_bd_sub_msg_enc_jis:
		case cdma_bd_sub_msg_enc_korean:
			bit_per_char = 8;
			break;

		case cdma_bd_sub_msg_enc_ext_gsm:
		case cdma_bd_sub_msg_enc_ext_pm:
		default:
			goto err;
	}

	return bit_per_char;
err:
	return -1;
}

const char* cdma_decode_subparam_timestamp(struct cdma_sms_param_hdr_t* phdr)
{
	struct cdma_sms_subparam_timestamp_t* ts;
	static char outbuf[SMS_TIMESTAMP_BUF_MAX_LEN];

	*outbuf=0;

	/* get timestamp */
	ts=(struct cdma_sms_subparam_timestamp_t*)phdr;

	/* check buffer length */
	if(phdr->param_len<sizeof(*ts)-sizeof(*phdr)) {
		SYSLOG(LOG_ERR,"###sms### [cdma] timestamp field not found");
		goto fini;
	}

	snprintf(outbuf,sizeof(outbuf),"%04d/%02d/%02d,%02d:%02d:%02d",
		cdma_decode_bcd_octet(ts->year)+2000,
		cdma_decode_bcd_octet(ts->month),
		cdma_decode_bcd_octet(ts->day),
		cdma_decode_bcd_octet(ts->hours),
		cdma_decode_bcd_octet(ts->minutes),
		cdma_decode_bcd_octet(ts->seconds)
	);

fini:
	return outbuf;
}

int cdma_decode_subparam_userdata(struct cdma_sms_param_hdr_t* sphdr,int* msg_encoding,int* msg_type,int* num_fields,char* chari,int chari_len)
{
/*
	* 3gpp2 subparameter structure - User Data

		Field Length (bits)
		SUBPARAMETER_ID 8
		SUBPARAM_LEN 8
		MSG_ENCODING 5
		MESSAGE_TYPE 0 or 8
		NUM_FIELDS 8
		NUM_FIELDS occurrences of the following field:
		CHARi Variable - see [15]
		The subparameter ends with the following field:
		RESERVED 0-7
*/

	int bit_per_char;
	int i;

	int param_bit_off=0;

	char* buf;
	int buf_len;

	if(!sphdr) {
		SYSLOG(LOG_ERR,"###sms### [cdma] user data subparameter not found");
		goto err;
	}

	buf=sphdr->param;
	buf_len=sphdr->param_len;

	/* subparam - MSG_ENCODING */
	if(cdma_get_bit_data(buf,buf_len,&param_bit_off,msg_encoding,5)<0) {
		SYSLOG(LOG_ERR,"###sms### [cdma] message encoding field not found");
		goto err;
	}

	/* subparam - MESSAGE_TYPE */
	if(*msg_encoding==cdma_bd_sub_msg_enc_ext_pm || *msg_encoding==cdma_bd_sub_msg_enc_ext_gsm) {
		if(cdma_get_bit_data(buf,buf_len,&param_bit_off,msg_type,8)<0) {
			SYSLOG(LOG_ERR,"###sms### [cdma] message type field not found");
			goto err;
		}
	}

	/* subparam - NUM_FIELDS */
	if(cdma_get_bit_data(buf,buf_len,&param_bit_off,num_fields,8)<0) {
		SYSLOG(LOG_ERR,"###sms### [cdma] number count field not found");
		goto err;
	}

	/* bypass if not buffer is given */
	if(!chari) {
		goto fini;
	}

	/* get bit per char */
	bit_per_char=cdma_get_bit_per_char_by_encoding(*msg_encoding);
	if(bit_per_char<0) {
		SYSLOG(LOG_ERR,"###sms### [cdma] message encoding type not supported (msg_encoding=%d)",*msg_encoding);
		goto err;
	}

	/* check buffer length */
	if(chari_len<*num_fields) {
		SYSLOG(LOG_ERR,"###sms### [cdma] insufficient buffer size for chari (num_fields=%d,chari_len=%d)",*num_fields,chari_len);
		goto err;
	}

	/* copy chari */
	int val;
	for(i=0;i<(*num_fields)*__size_in_byte(bit_per_char);i++) {
		if(cdma_get_bit_data(buf,buf_len,&param_bit_off,&val, (bit_per_char<8)?bit_per_char:8 )<0) {
			SYSLOG(LOG_ERR,"###sms### [cdma] number fields not found (i=%d,num_fields=%d)",i,*num_fields);
			goto err;
		}

		chari[i]=(char)val;
	}

fini:
	return 0;

err:
	return -1;
}

int cdma_encode_subparam_userdata(char* buf,int buf_len,int msg_encoding,int msg_type,int num_fields,const char* chari)
{
/*
	* 3gpp2 subparameter structure - User Data

		Field Length (bits)
		SUBPARAMETER_ID 8
		SUBPARAM_LEN 8
		MSG_ENCODING 5
		MESSAGE_TYPE 0 or 8
		NUM_FIELDS 8
		NUM_FIELDS occurrences of the following field:
		CHARi Variable - see [15]
		The subparameter ends with the following field:
		RESERVED 0-7
*/

	int bit_per_char;
	int i;

	int param_bit_off=0;
	struct cdma_sms_param_hdr_t* sphdr;

	/* start sub-param header */
	if(cdma_param_header_preserve(&sphdr,&buf,&buf_len)<0) {
		SYSLOG(LOG_ERR,"###sms### [cdma] insufficient buffer size for user data user-param header");
		goto err;
	}

	{
		/* subparam - MSG_ENCODING */
		if(cdma_set_bit_data(buf,buf_len,&param_bit_off,msg_encoding,5)<0) {
			SYSLOG(LOG_ERR,"###sms### [cdma] insufficient buffer size for message encoding field");
			goto err;
		}

		/* subparam - MESSAGE_TYPE */
		if(msg_encoding==cdma_bd_sub_msg_enc_ext_pm || msg_encoding==cdma_bd_sub_msg_enc_ext_gsm) {
			if(cdma_set_bit_data(buf,buf_len,&param_bit_off,msg_type,8)<0) {
				SYSLOG(LOG_ERR,"###sms### [cdma] insufficient buffer size for message type field");
				goto err;
			}
		}

		/* subparam - NUM_FIELDS */
		if(cdma_set_bit_data(buf,buf_len,&param_bit_off,num_fields,8)<0) {
			SYSLOG(LOG_ERR,"###sms### [cdma] insufficient buffer size for number count field");
			goto err;
		}

		/* get bit per char */
		bit_per_char=cdma_get_bit_per_char_by_encoding(msg_encoding);
		if(bit_per_char<0) {
			SYSLOG(LOG_ERR,"###sms### [cdma] message encoding type not supported (msg_encoding=%d)",msg_encoding);
			goto err;
		}

		for(i=0;i<num_fields*__size_in_byte(bit_per_char);i++) {
			if(cdma_set_bit_data(buf,buf_len,&param_bit_off,chari[i], (bit_per_char<8)?bit_per_char:8)<0) {
				SYSLOG(LOG_ERR,"###sms### [cdma] insufficient buffer size for number fields (i=%d,num_fields=%d)",i,num_fields);
				goto err;
			}
		}
	}

	return cdma_param_header_set(sphdr,user_data,__size_in_byte(param_bit_off));

err:
	return -1;
}

int cdma_encode_subparam_msgid(char* buf,int buf_len,int msg_type,unsigned short msg_id,int header_ind)
{
/*
	* 3gpp2 subparameter structure - Message Identifier

		Field Length (bits)
		SUBPARAMETER_ID 8
		SUBPARAM_LEN 8
		MESSAGE_TYPE 4
		MESSAGE_ID 16
		HEADER_IND 1
		RESERVED 3
*/

	struct cdma_sms_param_hdr_t* sphdr;
	int param_bit_off=0;

	/* start param header */
	if(cdma_param_header_preserve(&sphdr,&buf,&buf_len)<0)
		goto err;

	{
		/* subparam - MESSAGE_TYPE */
		if(cdma_set_bit_data(buf,buf_len,&param_bit_off,msg_type,4)<0)
			goto err;

		/* subparam - MESSAGE_ID */
		if(cdma_set_bit_data(buf,buf_len,&param_bit_off,msg_id,16)<0)
			goto err;

		/* subparam - HEADER_IND */
		if(cdma_set_bit_data(buf,buf_len,&param_bit_off,header_ind,1)<0)
			goto err;

		/* subparam - RESERVED */
		if(cdma_set_bit_data(buf,buf_len,&param_bit_off,0,3)<0)
			goto err;
	}

	/* set sub-parameter header */
	return cdma_param_header_set(sphdr,message_identifier,__size_in_byte(param_bit_off));

err:
	return -1;
}

int _qmi_sms_send_cdma(enum sms_msg_type_t sms_type, const char* smsc,const char* dest,enum sms_code_type_t code_to,const char* text,int text_len)
{
	struct qmi_easy_req_t er;
	const struct qmitlv_t* tlv;

	char* p;
	int l;
	int tl;
	int r;

	int rc;

	char* buf;

	int i;

	iconv_t _cd=NULL;

	struct cdma_sms_trans_layer_hdr_t* thdr;
	struct sms_body_schain_t chain[GSM_MAX_UD_COUNT];

	struct qmi_wms_raw_send_req_raw_message_data* tlv_raw_msg;

	/* resp variables */
	struct qmi_wms_raw_send_resp_message_id* msg_id;

	int tlv_raw_msg_len;

	int chain_idx;

	struct sms_body_schain_t* chain_ptr;

	/* init. local members */
	memset(chain,0,sizeof(chain));
	chain_idx=0;

	int format;

	/* get sms format - cdma or umts? */
	format=_qmi_format_table[sms_type];
	if(format<0) {
		SYSLOG(LOG_ERROR,"###sms### unknown sms format %d",format);
		goto err;
	}

	if(code_to<dcs_7bit_coding || code_to>dcs_7bit_ascii_coding) {
		SYSLOG(LOG_ERROR,"###sms### unknown code detected (code_to=%d)",code_to);
		goto err;
	}

	/* gsm encoding not supported yet - use ascii encoding for cdma */
	if(code_to==dcs_7bit_coding)
		code_to=dcs_7bit_ascii_coding;

	/* convert charset */
	_cd=iconv_open(_sms_code_to_table[code_to],_input_coding_scheme);
	if(!_cd || (_cd==(void*)-1)) {
		SYSLOG(LOG_ERROR,"###sms### failed to open iconv (to=%s,from=%s)",_sms_code_to_table[code_to],_input_coding_scheme);
		goto err;
	}


	char* inbuf;
	size_t inbytesleft;

	int msg_seq=0;

	int chain_cnt;

	/* set inbuf */
	inbuf=(char*)text;
	inbytesleft=text_len;


	while(1) {
		/* alloc */
		tlv_raw_msg_len=sizeof(*tlv_raw_msg)+160+200; /* CDMA_UD_MAX_LEN + extra */;
		tlv_raw_msg=malloc(tlv_raw_msg_len);

		/* maintain sms chain */
		chain_ptr=&chain[chain_idx++];
		chain_ptr->tlv_raw_msg=tlv_raw_msg;

		/* set format */
		tlv_raw_msg->format=(unsigned char)format;

		/* get buffer pointer and buffer length for cdma layer 3 */
		p=tlv_raw_msg->raw_message;
		tl=tlv_raw_msg_len-sizeof(*tlv_raw_msg);
		l=0;

		/* set msg type - p2p */
		thdr=(struct cdma_sms_trans_layer_hdr_t*)(p+l);
		thdr->msg_type=sms_msg_p2p;
		l+=sizeof(*thdr);

		/* set teleservice identifier */
		{
			struct cdma_sms_param_teleserv_id_t* param_ts;

			if(tl-l<sizeof(*param_ts)) {
				SYSLOG(LOG_ERROR,"###sms### [cdma] too small cdma buffer size (tl=%d,l=%d)",tl,l);
				goto err;
			}

			param_ts=(struct cdma_sms_param_teleserv_id_t*)(p+l);
			param_ts->phdr.param_id=param_id_teleservice_id;
			param_ts->phdr.param_len=sizeof(*param_ts)-sizeof(param_ts->phdr);

			cdma_set_bit_data((char*)&param_ts->id,sizeof(param_ts->id),NULL,cdma_sms_tele_id_cmt_95,sizeof(param_ts->id)*8);
			l+=sizeof(*param_ts);
		}

		/* set destination address */
		{
			buf=p+l;

			r=cdma_encode_address(
				buf,
				tl-l,
				param_id_dest_addr,
				cdma_sms_digit_mode_8_bit,
				cdma_sms_number_mode_none_data_network,
				cdma_sms_number_international,
				cdma_sms_number_plan_isdn,
				dest
			);

			if(r<0) {
				SYSLOG(LOG_ERROR,"###sms### [cdma] failed to convert to cdma address (dest=%s)",dest);
				goto err;
			}

			l+=r;
		}

		/* set bearer data */
		{
			struct cdma_sms_param_hdr_t* phdr;
			int r;

			static unsigned short msg_id=0;

			char* sp;
			int sl;
			int stl;

			/* get sub-pointer and sub-length */
			sp=p+l;
			sl=0;
			stl=tl-l;

			/* start param header */
			if(cdma_param_header_preserve(&phdr,&sp,&stl)<0) {
				SYSLOG(LOG_ERROR,"###sms### [cdma] too small buffer for bearer");
				goto err;
			}


			char textbuf[CDMA_UD_MAX_LEN*8/7];
			char* outbuf=textbuf;
			size_t outbytesleft;
			int outmaxlen;

			if(code_to==dcs_7bit_coding || code_to==dcs_7bit_ascii_coding) {
				outmaxlen=CDMA_UD_MAX_LEN*8/7;
			}
			else {
				outmaxlen=CDMA_UD_MAX_LEN;
			}

			outbytesleft=outmaxlen;

			/* sub-parameter - message_identifier */
			buf=sp+sl;
			r=cdma_encode_subparam_msgid(buf,stl-sl,submit,++msg_id,0); /* msg_type, msg_id, header_ind  */
			if(r<0) {
				SYSLOG(LOG_ERROR,"###sms### [cdma] failed to add message_identifier subparam");
				goto err;
			}
			sl+=r;

			/* convert user-input text */
			if( (iconv(_cd,&inbuf,&inbytesleft,&outbuf,&outbytesleft)==(size_t)-1) && (outbytesleft==outmaxlen) ) {
				SYSLOG(LOG_ERROR,"###sms### [cdma] failed to convert (from=%s,to=%s)",_input_coding_scheme,_sms_code_to_table[code_to]);
				goto err;
			}

			/* get length of converted text */
			int conv_text_len;
			conv_text_len=outmaxlen-outbytesleft;

			int num_fields;

			num_fields=(code_to==dcs_ucs2_coding)?conv_text_len/2:conv_text_len;

			/* get cdma msg encoding */
			int _sms_cdma_code_to_table[]={
				[dcs_7bit_coding]=cdma_bd_sub_msg_enc_7bit_acsii,
				[dcs_8bit_coding]=cdma_bd_sub_msg_enc_8bit_oct,
				[dcs_ucs2_coding]=cdma_bd_sub_msg_enc_unicode,
				[dcs_7bit_ascii_coding]=cdma_bd_sub_msg_enc_7bit_acsii,
			};
			int msg_encoding;
			msg_encoding=_sms_cdma_code_to_table[code_to];

			/* get gsm ext msg type - dcs (not supported) */
			int dcs;
			dcs=0;

			/* sub-parameter - user_data */
			buf=sp+sl;

			r=cdma_encode_subparam_userdata(buf,stl-sl,msg_encoding,dcs,num_fields,textbuf); /* msg_type, msg_id, header_ind  */
			if(r<0) {
				SYSLOG(LOG_ERROR,"###sms### [cdma] failed to add userdata subparam");
				goto err;
			}
			sl+=r;

			SYSLOG(LOG_ERROR,"###sms### [cdma] message preparation #%d - inbytesleft=%d,text_len=%d,conv_text_len=%d,num_fields=%d",msg_seq+1,inbytesleft,text_len,conv_text_len,num_fields);

			cdma_param_header_set(phdr,param_id_bearer_data,sl);

			/* increase length */
			l+=sizeof(*phdr)+sl;

			/* set raw text length */
			tlv_raw_msg->len=l;
		}

		msg_seq++;

		if(!inbytesleft) {
			SYSLOG(LOG_COMM,"###sms### [cdma] message preparation done (dest=%s)",dest);
			break;
		}

		if(!(msg_seq<GSM_MAX_UD_COUNT)) {
			SYSLOG(LOG_ERR,"###sms### [cdma] big too message (max concatenation (%d) reached",GSM_MAX_UD_COUNT);
			break;
		}
	}


	chain_cnt=chain_idx;
	for(i=0;i<chain_cnt;i++) {

		/* get tvl */
		chain_ptr=&chain[i];
		tlv_raw_msg=chain_ptr->tlv_raw_msg;

		/* debug dump */
		SYSLOG(LOG_ERROR,"###sms### [cdma] send message to qmi wms #%d/%d",i+1,chain_cnt);

		_dump(LOG_DUMP,__FUNCTION__,tlv_raw_msg->raw_message,tlv_raw_msg->len);

		/* init */
		rc=_qmi_easy_req_init(&er,QMIWMS,QMI_WMS_RAW_SEND);
		if(rc<0)
			goto err;

		/* req */
		tlv_raw_msg_len=sizeof(*tlv_raw_msg)+tlv_raw_msg->len;
		rc=_qmi_easy_req_do(&er,QMI_WMS_RAW_SEND_REQ_TYPE_RAW_MESSAGE_DATA,tlv_raw_msg_len,tlv_raw_msg,QMIMGR_LONG_RESP_TIMEOUT);
		if(rc<0)
			goto err;

		/* get tlv - QMI_WMS_RAW_SEND_RESP_TYPE_MESSAGE_ID */
		tlv=_get_tlv(er.rmsg,QMI_WMS_RAW_SEND_RESP_TYPE_MESSAGE_ID,sizeof(*msg_id));
		if(!tlv) {
			SYSLOG(LOG_ERROR,"###sms### failed to get message id");
			goto err;
		}
		msg_id=tlv->v;
		//SYSLOG(LOG_DEBUG,"###sms### message id = %d",msg_id->message_id);

		/* fini req */
		_qmi_easy_req_fini(&er);
	}



	rc=0;
fini:
	if(_cd)
		iconv_close(_cd);

	/* free sms body chain */
	for(i=0;i<chain_idx;i++) {
		chain_ptr=&chain[i];
		if(chain_ptr->tlv_raw_msg)
			free(chain_ptr->tlv_raw_msg);
	}

	return rc;

err:
	rc=-1;
	goto fini;
}

int _qmi_sms_send(enum sms_msg_type_t sms_type,const char* smsc,const char* dest,enum sms_code_type_t code_to,const char* text,int text_len)
{

	struct qmi_easy_req_t er;
	const struct qmitlv_t* tlv;
	int rc;

	int i;

	iconv_t _cd=NULL;

	struct sms_body_schain_t chain[GSM_MAX_UD_COUNT];
	int chain_idx=0;
	int chain_cnt;
	struct sms_body_schain_t* chain_ptr;
	struct gsm_user_data_concat_t* udh_concat;

	/* req variables */
	struct qmi_wms_raw_send_req_raw_message_data* tlv_raw_msg;
	int tlv_raw_msg_len;
	/* resp variables */
	struct qmi_wms_raw_send_resp_message_id* msg_id;

	int smsc_at; /* smsc address type */
	int dest_at; /* destination address type */

	int smsc_len; /* smsc tpp address length */
	int dest_len; /* destination tpp address length */
	int tpp_len; /* total tpp length */

	struct sms_tp_sumbit_t* tpp;

	char* pdu; /* pdu */
	int pdu_idx; /* index of pdu */

	/* init */
	memset(chain,0,sizeof(chain));

	/* calculate length of smsc and dest address */
	smsc_at=0x80 | ((*smsc=='+')?type_of_number_international:type_of_number_national) | numbering_plan_ident_isdntelephonenumberingplan;
	smsc_len=pdu_encode_address(NULL,0,smsc_at,smsc,0);

	dest_at=0x80 | ((*dest=='+')?type_of_number_international:type_of_number_national) | numbering_plan_ident_isdntelephonenumberingplan;
	dest_len=pdu_encode_address(NULL,0,dest_at,dest,1);

	/*
		get actual total length of transport protocol header

		(structure length of tpp) - (length of all variable members) + (actual length of all variable members)
	*/
	tpp_len=sizeof(*tpp)-(sizeof(tpp->smsc)+sizeof(tpp->da)+sizeof(tpp->vp))+(smsc_len+dest_len+sizeof(tpp->vp.relative));



#if 0
	/*
		SMSC : +61418706700
		Receiver : 0413237592
		Alphabet size : 8 bit
		1234567812345678
	*/

	/*
		AT+CMGF=0
		AT+CMGW=27
		07911614786007F011000A9240313257290000AA1031D98C56B3DD7031D98C56B3DD70 (ctrl-z)

		* 7 bits
		     +61 41 87 06 70 0              04 13 23 75 92
		07 91 16 14 78 60 07 F0 11 00 0A 92 40 31 32 57 29 00 00 AA / 10 31 D9 8C 56 B3 DD 70 31 D9 8C 56 B3 DD 70


		* 8 bits
		     +61 41 87 06 70 0              04 13 23 75 92
		07 91 16 14 78 60 07 F0 11 00 0A 92 40 31 32 57 29 00 04 AA / 10 31 32 33 34 35 36 37 38 31 32 33 34 35 36 37 38

		* 16 bits
		     +61 41 87 06 70 0              04 13 23 75 92
		07 91 16 14 78 60 07 F0 11 00 0A 92 40 31 32 57 29 00 04 AA / 10 31 32 33 34 35 36 37 38 31 32 33 34 35 36 37 38

	*/
#endif

	int format;

	/* get sms format - cdma or umts? */
	format=_qmi_format_table[sms_type];
	if(format<0) {
		SYSLOG(LOG_ERROR,"###sms### unknown sms format %d",format);
		goto err;
	}

	if(code_to<dcs_7bit_coding || code_to>dcs_ucs2_coding) {
		SYSLOG(LOG_ERROR,"###sms### unknown code detected (code_to=%d)",code_to);
		goto err;
	}

	/* convert charset */
	_cd=iconv_open(_sms_code_to_table[code_to],_input_coding_scheme);
	if(!_cd || (_cd==(void*)-1)) {
		SYSLOG(LOG_ERROR,"###sms### failed to open iconv (to=%s,from=%s)",_sms_code_to_table[code_to],_input_coding_scheme);
		goto err;
	}

	char* inbuf;
	size_t inbytesleft;

	/* set inbuf */
	inbuf=(char*)text;
	inbytesleft=text_len;

	int msg_seq;
	int udhi;

	msg_seq=0;
	udhi=0;

	static unsigned char ref=0;
	ref++;

	int msg_prep_idx=0;

	/* build transport layer protocol */
	while(1) {
		/*
			calculate total TLV length of raw text
			(length of raw text header) + tpp actual length + maximum SMS UD (user-data) size
		*/
		tlv_raw_msg_len=sizeof(*tlv_raw_msg)+tpp_len+GSM_SMS_UD_MAX_LEN;
		tlv_raw_msg=malloc(tlv_raw_msg_len);
		if(!tlv_raw_msg) {
			SYSLOG(LOG_ERROR,"###sms### failed to allocate memory for sms (qmi_tlv_hdr=%d,smsc_len=%d,tpp=%d,UDL_MAX=%d)",sizeof(*tlv_raw_msg),smsc_len,sizeof(*tpp),GSM_SMS_UD_MAX_LEN);
			goto err;
		}

		/* maintain sms chain */
		chain_ptr=&chain[chain_idx++];
		chain_ptr->tlv_raw_msg=tlv_raw_msg;

		/* set format */
		tlv_raw_msg->format=(unsigned char)format;

		/* get buffer pointer and buffer length for pdu */
		pdu=tlv_raw_msg->raw_message;
		pdu_idx=0;

		/* copy smsc */
		pdu_idx+=pdu_encode_address(pdu,tlv_raw_msg_len-pdu_idx,smsc_at,smsc,0);

		struct gsm_sms_submit_type_t* st;
		unsigned char* mr;
		unsigned char* pid;
		unsigned char* dcs;
		unsigned char* vp;
		unsigned char* udl;

		/* TODO: correct mr */
		static unsigned char last_mr=0;

		/* TPP - 1 octet : TP-MTI, TP-RD, TP-VPF TP-SRR, TP-UDHI, TP-RP */
		st=(struct gsm_sms_submit_type_t*)(pdu+pdu_idx);
		memset(st,0,sizeof(*st));
		st->mti=SMS_SUBMIT;
		st->rd = 0;
		st->vpf = RELATIVE_VPF_FORMAT;
		st->srr = 0;
		st->rp = 0;
		pdu_idx+=sizeof(*st);

		/* TPP - 1 octet : TP-Message-Reference */
		mr=(unsigned char*)(pdu+pdu_idx);
		last_mr++;
		*mr=last_mr;
		pdu_idx+=sizeof(*mr);

		/* TPP - variable octet : TP-Destination-Address */
		pdu_idx+=pdu_encode_address(pdu+pdu_idx,tlv_raw_msg_len-pdu_idx,dest_at,dest,1);

		/* TPP - 1 octet : TP-Protocol-Identifier */
		pid=(unsigned char*)(pdu+pdu_idx);
		*pid=0;
		pdu_idx+=sizeof(*pid);

		/* TPP - 1 octet : TP-Data-Coding-Scheme */
		dcs=(unsigned char*)(pdu+pdu_idx);
		/*
			Bits 3 and 2 indicate the character set being used, as follows:
			Bit 3 Bit 2 Character set:
			0 0 GSM 7 bit default alphabet
			0 1 8 bit data
			1 0 UCS2 (16 bit) [10]
			1 1 Reserved
		*/
		*dcs=code_to<<2;
		pdu_idx+=sizeof(*dcs);

		/* TPP - 1 octet : TP-Validity-Period (relative) */
		vp=(unsigned char*)(pdu+pdu_idx);
		/* TODO: use a proper value */
		*vp=0xAA; /* default vp : (170 - 166) * 1 day */
		pdu_idx+=sizeof(*vp);

		/* TPP - 1 octet : TP-User-Data-Length */
		udl=(unsigned char*)(pdu+pdu_idx);
		pdu_idx+=sizeof(*udl);

		/* TPP - variable (0-140) octet TP-User-Data */
		unsigned char* ud=(unsigned char*)(pdu+pdu_idx);

		int udl_max_length;
		static char pre_ud[(GSM_SMS_UD_MAX_LEN*8)/7];

		/* use more buffer since 7 bit can be packed */
		if(code_to==dcs_7bit_coding)
			udl_max_length=(GSM_SMS_UD_MAX_LEN*8)/7;
		else
			udl_max_length=GSM_SMS_UD_MAX_LEN;

		size_t outbytesleft;
		char* outbuf;

		/* convert only to get some idea of body length */
		if(msg_seq++==0) {
			/* set outbuf */
			outbytesleft=udl_max_length;
			outbuf=pre_ud;

			if( (iconv(_cd,&inbuf,&inbytesleft,&outbuf,&outbytesleft)==(size_t)-1) && (outbytesleft==udl_max_length) ) {
				SYSLOG(LOG_ERROR,"###sms### failed to convert #2 (from=%s,to=%s)",_input_coding_scheme,_sms_code_to_table[code_to]);
				goto err;
			}

			/* use concatenation if it does not fit */
			if(inbytesleft)
				udhi=1;

			/* reset */
			iconv(_cd,NULL,NULL,&outbuf,&outbytesleft);

			/* re-init */
			inbuf=(char*)text;
			inbytesleft=text_len;
		}

		/* set udhi */
		st->udhi=udhi?1:0;

		chain_ptr->udh_concat=NULL;

		/* build udh in ud */
		int udh_len;
		udh_len=0;
		if(udhi) {
			struct gsm_user_data_header_t* udh;
			struct gsm_user_data_sub_header_t* udsh;

			/* get addresses of User-Data-Header and User-Data-Header for concatenation */
			udh=(struct gsm_user_data_header_t*)ud;
			udsh=(struct gsm_user_data_sub_header_t*)(udh+1);
			udh_concat=(struct gsm_user_data_concat_t*)(udsh+1);

			/* TTP - User-Data-Header */
			udh->udhl=sizeof(*udsh)+sizeof(*udh_concat);
			udsh->iei=0x00; /* Concatenated short messages, 8-bit reference number - SMS Control */
			udsh->iedl=sizeof(*udh_concat);

			/* store udh concat pointer - set concat information later - TTP - User-Data-Header for concatenation */
			chain_ptr->udh_concat=udh_concat;

			udh_len=sizeof(*udh)+sizeof(*udsh)+sizeof(*udh_concat);

			pdu_idx+=udh_len;
		}


		/* calculate pad bit for 7 bit encoding */
		int pad_bits;
		pad_bits=0;
		if(udh_len && (code_to==dcs_7bit_coding)) {
			pad_bits = (udh_len*8)%7;
			if(pad_bits)
				pad_bits=7-pad_bits;
		}

		/* calculate space with udh */
		int udl_max_length_with_udh;
		if(code_to==dcs_7bit_coding)
			udl_max_length_with_udh=((GSM_SMS_UD_MAX_LEN-udh_len)*8-pad_bits)/7;
		else
			udl_max_length_with_udh=GSM_SMS_UD_MAX_LEN-udh_len;

		/* set outbuf */
		outbytesleft=udl_max_length_with_udh;
		outbuf=pre_ud;

		/* start real conversation */
		if( (iconv(_cd,&inbuf,&inbytesleft,&outbuf,&outbytesleft)==(size_t)-1) && (outbytesleft==udl_max_length_with_udh) ) {
			SYSLOG(LOG_ERROR,"###sms### failed to convert (from=%s,to=%s)",_sms_code_to_table[code_to],_input_coding_scheme);
			goto err;
		}

		/* copy converted code into buffer */
		int insize;
		char* intext;
		int outUsed;

		insize=udl_max_length_with_udh-outbytesleft;
		intext=pre_ud;

		int ud_len;

		msg_prep_idx++;
		SYSLOG(LOG_DEBUG,"###sms### prepare to send (udhi=%d,cnt=%d,ref=%d,idx=%d)",udhi,insize,ref,msg_prep_idx);

		ud_len=GSM_SMS_UD_MAX_LEN-udh_len;

		/* copy encoded text into raw packet */
		if(code_to==dcs_7bit_coding) {
			/* pack GSM-7 bit into septets */
			if(!SMS_GSMEncode(insize,intext,pad_bits,ud_len,ud+udh_len,&outUsed)) {
				SYSLOG(LOG_ERROR,"###sms### failed to pack GSM-7 bit into septets (from=%s,to=%s)",_sms_code_to_table[code_to],_input_coding_scheme);
				goto err;
			}
			pdu_idx+=outUsed;

			*udl=(((udh_len*8)+6)/7)+insize;
		}
		else {
			memcpy(ud+udh_len,intext,insize);
			pdu_idx+=insize;

			*udl=udh_len+insize;
		}

		/* set raw text length */
		tlv_raw_msg->len=pdu_idx;

		if(!inbytesleft) {
			SYSLOG(LOG_COMM,"###sms### message preparation done (dest=%s)",dest);
			break;
		}

		if(!(msg_seq<GSM_MAX_UD_COUNT)) {
			SYSLOG(LOG_ERR,"###sms### big too message (max concatenation (%d) reached",GSM_MAX_UD_COUNT);
			break;
		}
	}


	chain_cnt=chain_idx;
	for(i=0;i<chain_cnt;i++) {

		/* get tvl */
		chain_ptr=&chain[i];
		tlv_raw_msg=chain_ptr->tlv_raw_msg;

		if(!tlv_raw_msg)
			break;

		/* set TTP - User-Data-Header for concatenation */
		udh_concat=chain_ptr->udh_concat;
		if(udh_concat) {
			/* TTP - User-Data-Header for concatenation */
			udh_concat->ref=ref;
			udh_concat->max_no=chain_cnt;
			udh_concat->seq_no=i+1;
		}

		if(udh_concat) {
			SYSLOG(LOG_DEBUG,"###sms### send multi-body message (dest=%s,ref=%d,seq=%d,max_seq=%d)",dest,ref,i+1,chain_cnt);
		}
		else {
			SYSLOG(LOG_DEBUG,"###sms### send a single message (dest=%s)",dest);
		}

		/* debug dump */
		_dump(LOG_DUMP,__FUNCTION__,tlv_raw_msg->raw_message,tlv_raw_msg->len);

		/* init */
		rc=_qmi_easy_req_init(&er,QMIWMS,QMI_WMS_RAW_SEND);
		if(rc<0)
			goto err;

#if 0
		/* add sms on ims */
		{
			char sms_on_ims=0x01;
			qmimsg_add_tlv(er.msg,0x13,sizeof(sms_on_ims),&sms_on_ims);
		}
#endif		
		
		/* req */
		tlv_raw_msg_len=sizeof(*tlv_raw_msg)+tlv_raw_msg->len;
		rc=_qmi_easy_req_do(&er,QMI_WMS_RAW_SEND_REQ_TYPE_RAW_MESSAGE_DATA,tlv_raw_msg_len,tlv_raw_msg,QMIMGR_LONG_RESP_TIMEOUT);
		if(rc<0)
			goto err;

		/* get tlv - QMI_WMS_RAW_SEND_RESP_TYPE_MESSAGE_ID */
		tlv=_get_tlv(er.rmsg,QMI_WMS_RAW_SEND_RESP_TYPE_MESSAGE_ID,sizeof(*msg_id));
		if(!tlv) {
			SYSLOG(LOG_ERROR,"###sms### failed to get message id");
			goto err;
		}
		msg_id=tlv->v;
		//SYSLOG(LOG_DEBUG,"###sms### message id = %d",msg_id->message_id);

		/* fini req */
		_qmi_easy_req_fini(&er);
	}

	SYSLOG(LOG_DEBUG,"###sms### message sent (dest=%s)",dest);

	rc=0;

fini:
	if(_cd)
		iconv_close(_cd);

	/* free sms body chain */
	for(i=0;i<chain_idx;i++) {
		chain_ptr=&chain[i];
		if(chain_ptr->tlv_raw_msg)
			free(chain_ptr->tlv_raw_msg);
	}

	return rc;

err:
	rc=-1;
	goto fini;
}

void qmimgr_callback_on_schedule_sms_readall(struct funcschedule_element_t* element)
{
	SYSLOG(LOG_DEBUG,"###sms### start to read new messages");

	/* update sms cfg */
	qmimgr_update_sms_cfg();
	/* read all sms */
	_qmi_sms_readall();
}

void qmimgr_schedule_sms_readall()
{
	funcschedule_cancel(sched,schedule_key_sms_queue);
	funcschedule_add(sched,schedule_key_sms_queue,QMIMGR_SMS_QUEUE_PERIOD,qmimgr_callback_on_schedule_sms_readall,NULL);
}

void qmimgr_callback_on_schedule_sms_spool(struct funcschedule_element_t* element)
{
	/* check spool */
	int cnt_spool=0;
	struct dirent** de_spool=NULL;

	/* scan dir spool dir */
	cnt_spool=scandir(SMS_INBOX_SPOOL_DIR, &de_spool, _dirscan_on_inbox_filter, NULL);
	if(cnt_spool<0) {
		SYSLOG(LOG_ERR,"###sms### failed to scan inbox spool directory (inbox=%s)",SMS_INBOX_SPOOL_DIR);
		goto fini;
	}

	/*
		Although this hard-coded status string is completely wrong, this is how SMS of AT port manager is setup.
		For backward compatibility, we have to do this.
	*/
	if(cnt_spool) {
		SYSLOG(LOG_OPERATION,"###sms### notify new message to sms.template (cnt=%d)",cnt_spool);
		_set_fmt_db_ex("sms.received_message.status","Total:xxx read:x unread:%d sent:x unsent:x",cnt_spool);
	}


fini:
	_destroy_de(de_spool,cnt_spool);

	/* reschedule sms */
	funcschedule_cancel(sched,schedule_key_sms_spool);
	funcschedule_add(sched,schedule_key_sms_spool,QMIMGR_SPOOL_SCHEDULE_PERIOD,qmimgr_callback_on_schedule_sms_spool,NULL);
}

void qmimgr_callback_on_schedule_sms_qmi(struct funcschedule_element_t* element)
{
	int rc;

	int addr_type;
	char addr[256];

	static int smsc_to_read=1;

	/* read smsc */
	{
		if(!element)
			smsc_to_read=1;

		if(smsc_to_read) {
			rc=_qmi_sms_get_smsc(&addr_type,addr,sizeof(addr));
			if(rc<0) {
				SYSLOG(LOG_DEBUG,"###sms### failed to get smsc (rc=%d)",rc);
				goto smsc_fini;
			}

			/* store smsc address */
			_set_str_db("sms.smsc_addr",addr,-1);

			smsc_to_read=0;
		}

		smsc_fini:
				(void)(0);
	}

	/* reschedule sms */
	funcschedule_cancel(sched,schedule_key_sms_generic);
	funcschedule_add(sched,schedule_key_sms_generic,QMIMGR_GENERIC_SCHEDULE_PERIOD,qmimgr_callback_on_schedule_sms_qmi,NULL);
}

void qmimgr_update_sms_cfg()
{
	const char* input_coding_scheme;

	/* get sms source code type */
	input_coding_scheme=strdupa(_get_str_db_ex("smstools.conf.input_coding_scheme",SMS_ENCODING_BROWSER_DEFAULT,0));
	SYSLOG(LOG_OPERATION,"###sms### source encoding scheme = %s",input_coding_scheme);

	/* TODO: read actual sms mode from module */
	_qmi_sms_init_coding_scheme(SMS_ENCODING_BROWSER_DEFAULT);
}

void qmimgr_on_db_command_sms(const char* db_var, int db_var_idx,const char* db_cmd, int db_cmd_idx)
{
	int msg_sent;

	const char* coding_scheme;
	enum sms_code_type_t code_type;

	/* get sms coding scheme */
	coding_scheme=_get_str_db_ex("smstools.conf.coding_scheme",SMS_ENCODING_SMS_DEFAULT,0);
	if(!strcmp(coding_scheme,"GSM8"))
		code_type=dcs_8bit_coding;
	else if(!strcmp(coding_scheme,"UCS2"))
		code_type=dcs_ucs2_coding;
	else
		code_type=dcs_7bit_coding;

	SYSLOG(LOG_OPERATION,"###sms### DCS from RDB is %d",code_type);

	/* update sms cfg */
	qmimgr_update_sms_cfg();

	/* switch command */
	switch(db_cmd_idx) {
		/* TODO: in DIAG mode, storage type has to be checked before */
		case DB_COMMAND_VAL_SENDDIAG:

			SYSLOG(LOG_OPERATION,"got DB_COMMAND_VAL_SENDDIAG");
			code_type=dcs_7bit_coding;

		case DB_COMMAND_VAL_SEND: {

			if(db_cmd_idx==DB_COMMAND_VAL_SEND)
				SYSLOG(LOG_OPERATION,"got DB_COMMAND_VAL_SEND");

			char* smsc;
			int smsc_addr_type;

			const char* dest;
			char* text=NULL;
			const char* text_fname;
			char* text_raw_fname;
			int len;
			int special_chars;

			_qmi_sms_update_sms_type_based_on_firmware();

			/*

				* workaround for MC7354 SWI9X15C_06.03.32.02

				MC7354 SWI9X15C_06.03.32.02 does not send QMI_WMS_SERVICE_READY_IND after unlocking SIM PIN
				In result, SMS service appears unavailable with SIM PIN enabled

				To workaround, this missing indiciation, we need to poll the SMS service availability even if the indication is
				subscribed with QMI_WMS_INDICATION_REGISTER.
			*/
			_qmi_sms_update_sms_type(NULL);
			
			/* allocate for smsc */
			smsc=alloca(SMS_ADDR_BUF_MAX_LEN);
			if(!smsc) {
				SYSLOG(LOG_ERROR,"###sms### failed to allocate memory for smsc (len=%d)",SMS_ADDR_BUF_MAX_LEN);
				goto err_send;
			}

			/* a bit wrong but for backward compatibility from Simple AT command */
			special_chars=_get_int_db("sms.has_special_chars",0);
			if(special_chars) {
				SYSLOG(LOG_DEBUG,"###sms### override to UCS2 based on [wwan.0.sms.has_special_chars] - special_chars=%d",special_chars);
				code_type=dcs_ucs2_coding;
			}


			/*  get dest address */
			dest=strdupa(_get_str_db("sms.cmd.param.to",""));
			if(!*dest) {
				SYSLOG(LOG_ERROR,"###sms### failed to get destination address of sms [sms.cmd.param.to]");
				goto err_send;
			}


			#ifdef QMIMGR_CONFIG_SMS_FILE_MODE
			/* get file name */
			text_fname=_get_str_db("sms.cmd.param.filename","");
			if(!*text_fname) {
				SYSLOG(LOG_ERROR,"###sms### failed to get sms context file sms [sms.cmd.param.filename]");
				goto err_send;
			}

			/* get raw file name */
			len=strlen(text_fname)+sizeof(SMS_INBOX_FILE_EXT);
			text_raw_fname=alloca(len);
			snprintf(text_raw_fname,len,"%s%s",text_fname,SMS_INBOX_FILE_EXT);

			/* read file */
			len=sms_create_memory_from_file(text_raw_fname,&text);
			if(len<0) {
				SYSLOG(LOG_ERR,"###sms### failed to read sms file - fname=%s,err_send=%s",text_fname,strerror(errno));
				goto err_send;
			}
			#else
			/* get text */
			text=strdupa(_get_str_db("sms.cmd.param.message",""));
			len=strlen(text);
			#endif

			SYSLOG(LOG_OPERATION,"###sms### got sms request (dest=%s,code_type=%d,fname=%s)",dest,code_type,text_raw_fname);
			SYSLOG(LOG_OPERATION,"###sms### _sms_type=0x%02x",_sms_type);
			SYSLOG(LOG_OPERATION,"###sms### _sms_service_ready[smt_umts]=0x%02x",_sms_service_ready[smt_umts]);
			SYSLOG(LOG_OPERATION,"###sms### _sms_service_ready[smt_cdma]=0x%02x",_sms_service_ready[smt_cdma]);
			
			/* filter if no service is availble */
			if(!_sms_service_ready[smt_umts] && !_sms_service_ready[smt_cdma]) {
				SYSLOG(LOG_ERR,"###sms### no sms service ready");
				goto err_send;
			}
			
			msg_sent=0;
			
			/* send umts */
			if(!msg_sent && _sms_service_ready[smt_umts] && (_sms_type==smt_umts)) {
				SYSLOG(LOG_OPERATION,"###sms### use 3gpp protocol");

				/* get smsc */
				if(_qmi_sms_get_smsc(&smsc_addr_type,smsc,SMS_ADDR_BUF_MAX_LEN)<0) {
					SYSLOG(LOG_ERROR,"###sms### failed in _qmi_sms_get_smsc()");
					goto fini_umts;
				}

				SYSLOG(LOG_OPERATION,"###sms### smsc_addr_type=%d",smsc_addr_type);
				SYSLOG(LOG_OPERATION,"###sms### smsc=%s",smsc);

				/* send sms */
				if(_qmi_sms_send(smt_umts,smsc,dest,code_type,text,len)<0) {
					SYSLOG(LOG_ERR,"###sms### failed to send sms - len=%d",len);
					goto fini_umts;
				}

				/* set sent flag */
				msg_sent=1;
			}
		fini_umts:

			/* send cdma */
			if(!msg_sent && _sms_service_ready[smt_cdma] && (_sms_type==smt_cdma)) {

				SYSLOG(LOG_OPERATION,"###sms### use 3gpp2 protocol");

				/* set sms */
				if(_qmi_sms_send_cdma(smt_cdma,smsc,dest,code_type,text,len)<0) {
					SYSLOG(LOG_ERR,"###sms### failed to send sms - len=%d",len);
					goto fini_cdma;
				}

				/* set sent flag */
				msg_sent=1;
			}
		fini_cdma:
			
			/* go to err if not sent */
			if(!msg_sent)
				goto err_send;
			
			#ifdef QMIMGR_CONFIG_SMS_FILE_MODE
			sms_destroy_memory(&text);
			#endif

			/* set rdb success result */
			_set_str_db("sms.cmd.send.status","[done] send",-1);
			break;
		
		err_send:
			#ifdef QMIMGR_CONFIG_SMS_FILE_MODE
			sms_destroy_memory(&text);
			#endif

			/* set rdb failure result */
			_set_str_db("sms.cmd.send.status","[error] send sms failed",-1);
			break;
		}

		case DB_COMMAND_VAL_READALL: {
			SYSLOG(LOG_OPERATION,"got DB_COMMAND_VAL_READALL");

			if(_qmi_sms_readall(SMS_ENCODING_BROWSER_DEFAULT)<0) {
				SYSLOG(LOG_ERROR,"failed from _qmi_sms_readall()");
				goto err_readall;
			}


			/* set rdb success result */
			_set_str_db("sms.cmd.send.status","[done] readall",-1);
			break;

		err_readall:
			/* set rdb failure result */
			_set_str_db("sms.cmd.send.status","[error] readall failed",-1);
			break;
		}

		case DB_COMMAND_VAL_DELETE: {
			char* msg_ids;
			char* msg_id;

			char* p;

			unsigned char storage_type;
			unsigned int msg_id_no;

			SYSLOG(LOG_OPERATION,"got DB_COMMAND_VAL_DELETE");

			/* get message ids */
			msg_ids=strdupa(_get_str_db("sms.cmd.param.message_id",""));
			if(!*msg_ids) {
				SYSLOG(LOG_ERR,"###sms### failed to get message list");
				goto err_delete;
			}

			storage_type=QMI_WMS_STORAGE_TYPE_NV;

			/* delete each message */
			msg_id = strtok_r(msg_ids, " ", &p);
			while(msg_id) {
				msg_id_no=atoi(msg_id);

				if((_qmi_sms_delete(smt_umts,&storage_type,NULL,&msg_id_no)<0) && (_qmi_sms_delete(smt_cdma,&storage_type,NULL,&msg_id_no)<0)) {
					SYSLOG(LOG_ERR,"###sms### failed to delete message (msg_id=%u)",msg_id_no);
					goto err_delete;
				}

				msg_id = strtok_r(NULL, " ", &p);
			}

			/* set rdb success result */
			_set_str_db("sms.cmd.send.status","[done] delete",-1);
			break;

		err_delete:
			/* set rdb failure result */
			_set_str_db("sms.cmd.send.status","[error] failed to delete messages",-1);
			break;
		}

		case DB_COMMAND_VAL_DELALL: {
			unsigned char storage_type;

			SYSLOG(LOG_OPERATION,"got DB_COMMAND_VAL_DELALL");

			/* delete all messages */
			storage_type=QMI_WMS_STORAGE_TYPE_NV;
			if( (_qmi_sms_delete(smt_umts,&storage_type,NULL,NULL)<0) && (_qmi_sms_delete(smt_cdma,&storage_type,NULL,NULL)<0)) {
				SYSLOG(LOG_ERR,"###sms### failed to delete all messages");
				goto err_delall;
			}

			/* set rdb success result */
			_set_str_db("sms.cmd.send.status","[done] delall",-1);
			break;

		err_delall:
			/* set rdb failure result */
			_set_str_db("sms.cmd.send.status","[error] readall failed",-1);
			break;
		}

		case DB_COMMAND_VAL_SETSMSC: {
			const char* dest;
			int addr_type;

			SYSLOG(LOG_OPERATION,"got DB_COMMAND_VAL_SETSMSC");

			dest=strdupa(_get_str_db("sms.cmd.param.to",""));
			if(!*dest) {
				SYSLOG(LOG_ERROR,"###sms### failed to get destination address of sms [sms.cmd.param.to]");
				goto err_setsmsc;
			}

			/* get address type */
			addr_type=(*dest=='+')?145:129;
			SYSLOG(LOG_OPERATION,"###sms### address type = %d",addr_type);

			/* set smsc */
			if(_qmi_sms_set_smsc(addr_type,dest)<0) {
				SYSLOG(LOG_ERROR,"###sms### failed to set smsc");
				goto err_setsmsc;
			}

			/* update smsc */
			qmimgr_callback_on_schedule_sms_qmi(NULL);

			/* set rdb success result */
			_set_str_db("sms.cmd.send.status","[done] setsmsc",-1);
			break;

		err_setsmsc:
			/* set rdb failure result */
			_set_str_db("sms.cmd.send.status","[error] failed to set smsc",-1);
			break;
		}

		default:
			SYSLOG(LOG_ERROR,"unknown command %s(%d) in %s(%d)",db_cmd,db_cmd_idx,db_var,db_var_idx);
			//_set_str_db("profile.cmd.status","[error] unknown command",-1);
			break;
	}

	// clear command - backward compatibabity
	_set_reset_db(db_var);
}
