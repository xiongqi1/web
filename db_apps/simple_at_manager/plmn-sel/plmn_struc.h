
#ifndef __PLMN_STRUC_H__
#define __PLMN_STRUC_H__

#define SHIFT_L16(x,b)	( ((x)&0xffff) << ((b)*16))
#define SHIFT_R16(x,b) ( ((x)>>((b)*16)) & 0xffff )

#define MCCMNC(mcc,mnc)	(SHIFT_L16(mcc,1) | SHIFT_L16(mnc,0))
#define MCC(mccmnc)	(SHIFT_R16(mccmnc,1))
#define MNC(mccmnc)	(SHIFT_R16(mccmnc,0))

enum bl_reasons_t {
	e_bl_reason_none,
	e_bl_reason_imsi_reg_failure,
	e_bl_reason_signal_failure,
	e_bl_reason_pdp_attach_failure,
	e_bl_reason_count
};

struct plmn_t {

	// common
	int mcc;
	int mnc;
	int mcc_digit;
	int mnc_digit;

	// AT+COPS=?
	int act;
	char lname[16+1];
	char sname[8+1];
	int stat;
	
	// AT^SNMON="INS",2
	int dbm;

	// csv
	int rank;
	int best;

	char country[128];
	char network[128];

/* TT#5825 Update PRL list importer to only expect 6 columns in imported CSV file 
	int priority;
	char tier[32];
	int act_genmask;
	char opcode[32]; */

	// dynamic info
	int black_plmn;
	enum bl_reasons_t bl_reason;
};


struct rbtplmn_t;
struct llplmn_t;

struct plmn_t* rbtplmn_find(struct rbtplmn_t* o,struct plmn_t* plmn);

int rbtplmn_del(struct rbtplmn_t* o,struct plmn_t* plmn);
struct plmn_t* rbtplmn_add(struct rbtplmn_t* o,struct plmn_t* plmn,int copy);

void rbtplmn_destroy(struct rbtplmn_t* o);
struct rbtplmn_t* rbtplmn_create();

struct plmn_t* rbtplmn_get_next(struct rbtplmn_t* o);
struct plmn_t* rbtplmn_get_first(struct rbtplmn_t* o);

struct plmn_t* llplmn_find(struct llplmn_t* o,struct plmn_t* plmn);

int llplmn_del(struct llplmn_t* o,struct plmn_t* plmn);
struct plmn_t* llplmn_add(struct llplmn_t* o,struct plmn_t* plmn,int copy);

void llplmn_destroy(struct llplmn_t* o);
struct llplmn_t* llplmn_create();

struct plmn_t* llplmn_get_next(struct llplmn_t* o);
struct plmn_t* llplmn_get_first(struct llplmn_t* o);

void llplmn_sort(struct llplmn_t* o);

#endif
