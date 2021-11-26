#ifndef __QMI_CELL_INFO_H__
#define __QMI_CELL_INFO_H__

void process_qmi_cell_info(struct qmimsg_t* msg);

#define QMI_NAS_GET_CELL_LOCATION_INFO 0x0043

#define QMI_NAS_GET_CELL_LOCATION_INFO_RESP_TYPE_GERAN_INFO 0x10
struct cell_info_geran {
	unsigned short cell_id;
	struct {
		unsigned char raido_if;
		unsigned char active_band;
		unsigned char active_channel;
	} __packed interfaces[1];
} __packed;

#define QMI_NAS_GET_CELL_LOCATION_INFO_RESP_TYPE_UMTS_INFO 0x11
	struct cell_info_umts {
		unsigned short cell_id;
		unsigned char  plmn[3];
		unsigned short lac;
		unsigned short uarfcn;
		unsigned short psc;
		short rscp;
		short ecio;
		unsigned char  umts_inst;
	} __packed;

	struct cell_info_monitored_set{
		unsigned short uarfcn;
		unsigned short psc;
		short rscp;
		short ecio;
	} __packed;

	struct cell_info_geran_set{
		unsigned short arfcn;
		unsigned char bsic_ncc;
		unsigned char bsic_bcc;
		short rssi;
	} __packed;

#define QMI_NAS_GET_CELL_LOCATION_INFO_RESP_TYPE_LTE_INFO_INTRAFREQUNCY 0x13
	struct cell_info_lte_intrafrequency {
		unsigned char ue_in_idle;
		unsigned char  plmn[3];
		unsigned short tac;
		unsigned int global_cell_id;
		unsigned short earfcn;
		unsigned short serving_cell_id;
		unsigned char cell_resel_priority;
		unsigned char s_non_intra_search;
		unsigned char thresh_serving_low;
		unsigned char s_intra_search;
		unsigned char cells_len;
	} __packed;

	struct cell_info_lte_set{
		unsigned short pci;
		short rsrq;
		short rsrp;
		short rssi;
		short srxlev;
	} __packed;

#define QMI_NAS_GET_CELL_LOCATION_INFO_RESP_TYPE_LTE_INFO_INTERFREQUENCY 0x14
	struct cell_info_lte_neighbours_hdr {
		unsigned char ue_in_idle;
		unsigned char freqs_len;
	} __packed;

	struct cell_info_lte_inter_freq_set {
		unsigned short earfcn;
		unsigned char threshX_low;
		unsigned char threshX_high;
		unsigned char cell_resel_priority;
		unsigned char cells_len;
	} __packed;
		//cell_info_lte_set[]

#define QMI_NAS_GET_CELL_LOCATION_INFO_RESP_TYPE_LTE_INFO_GSM 0x15
	// cell_info_lte_neighbours_hdr

	struct cell_info_lte_neighbours_gsm_freqs {

		unsigned char cell_resel_priority;
		unsigned char thresh_gsm_high;
		unsigned char thresh_gsm_low;
		unsigned char ncc_permitted;
		unsigned char cells_len;
	} __packed;

	struct cell_info_lte_neighbours_gsm_cells {
		unsigned short arfcn;
		unsigned char band_1900;
		unsigned char cell_id_valid;
		unsigned char bsic_id;
		short rssi;
		short srxlev;
	} __packed;


#define QMI_NAS_GET_CELL_LOCATION_INFO_RESP_TYPE_LTE_INFO_WCDMA 0x16
	// cell_info_lte_neighbours_hdr

	struct cell_info_lte_neighbours_wcdma_freqs {
		unsigned short uarfcn;
		unsigned char cell_resel_priority;
		unsigned short threshX_high;
		unsigned short threshX_low;
		unsigned char cells_len;
	} __packed;

	// cells_len
	struct cell_info_lte_neighbours_wcdma_cells {
		unsigned short psc;
		short cpich_rscp;
		short cpich_ecno;
		short srxlev;
	} __packed;

#define QMI_NAS_GET_CELL_LOCATION_INFO_RESP_TYPE_UMTS_CELL_ID 0x17
//ToDo

#define QMI_NAS_GET_CELL_LOCATION_INFO_RESP_TYPE_UMTS_INFO_LTE 0x18
	struct cell_info_umts_neighbours_lte {
		unsigned int wcdma_rrc_state;
		unsigned char umts_lte_nbr_cell_len;
	} __packed;

	struct cell_info_umts_lte_set{
		unsigned short earfcn;
		unsigned short pci;
		float rsrp;
		float rsrq;
		short srxlev;
		unsigned char cell_is_tdd;
	} __packed;

#endif
