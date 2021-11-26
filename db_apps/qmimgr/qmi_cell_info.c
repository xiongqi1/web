#include "rdb_ops.h"

#include "qmidef.h"
#include "qmimsg.h"
#include "qmi_cell_info.h"


int _set_str_db(const char* dbvar,const char* dbval,int dbval_len);
int _set_int_db(const char* dbvar,int dbval,const char* suffix);
int _get_int_db(const char* dbvar,int defval);

/*
 * RDB Variables:
 *
 * wwan.0.cellinfo.timer	- Time between measurements, 0 = off
 *
 * wwan.0.cell_measurement.qty - Number of cells in current measurement set
 * wwan.0.cell_measurement.serving_system - LTE | UMTS | GSM
 * wwan.0.cell_measurement.[n] <type>,<channel>,<cell_id>,<signal_strength>,<signal_quality>
 */

void post_cell_info_to_rdb(int n,  char type, short channel, unsigned short cell_id, float ss, float sq)
{
	char name [64];
	char value [64];

	snprintf(name, sizeof(name), "cell_measurement.%d", n++);
	snprintf(value, sizeof(value), "%c,%d,%d,%0.1f,%0.1f", type, channel, cell_id, ss, sq);

	_set_str_db(name,value,-1);
}

void process_qmi_cell_info(struct qmimsg_t* msg)
{
	const struct qmitlv_t* tlv;

	int i = 0;
	int j = 0;
	int n = 0;

	char buf[32];
	unsigned short new_serving_cell_id = 0xFFFF, old_serving_cell_id = _get_int_db("system_network_status.PCID", 0xFFFF);


	/*
	 * The Rf Qualififctaion screen will display only the first 4 cells in the cell_measurement list
	 * Therefore the order in which we process TLVs is important.
	 * For now we assume that serving system cells are most interesting and then,
	 * LTE cells more interesting than UMTS and UMTS is more interesting than GSM.
	 *
	 */

	/* When attached to and LTE serving system we can expect the following TLVs:-
	 *
	 * 0x13 QMI_NAS_GET_CELL_LOCATION_INFO_RESP_TYPE_LTE_INFO - LTE Intra Frequency
	 * 0x14 QMI_NAS_GET_CELL_LOCATION_INFO_RESP_TYPE_LTE_INFO_INTERFREQUENCY - LTE Inter Frequency
	 * 0x15 QMI_NAS_GET_CELL_LOCATION_INFO_RESP_TYPE_LTE_INFO_GSM - Neighbouring GSM
	 * 0x16 QMI_NAS_GET_CELL_LOCATION_INFO_RESP_TYPE_LTE_INFO_WCDMA - Neighbouring WCDMA
	 */

	tlv=_get_tlv(msg, QMI_NAS_GET_CELL_LOCATION_INFO_RESP_TYPE_LTE_INFO_INTRAFREQUNCY, sizeof(struct cell_info_lte_intrafrequency));
	if(tlv) {
		struct cell_info_lte_intrafrequency* resp=(struct cell_info_lte_intrafrequency*)tlv->v;

		SYSLOG(LOG_COMM,"UE Idle = %d", resp->ue_in_idle);
		SYSLOG(LOG_COMM,"PLMN = %d %d %d %d %d ", resp->plmn[0] & 0x0F, resp->plmn[0] >> 4, resp->plmn[1] & 0x0F, resp->plmn[2] & 0x0F, resp->plmn[2] >> 4);
		SYSLOG(LOG_COMM,"TAC = %d", resp->tac);
		SYSLOG(LOG_COMM,"GCID = %d", resp->global_cell_id);
		SYSLOG(LOG_COMM,"EARFCN = %d", resp->earfcn);
		SYSLOG(LOG_COMM,"SERVING_CELL_ID = %d", resp->serving_cell_id);
		SYSLOG(LOG_COMM,"Cell reselection priority = %d", resp->cell_resel_priority);
		SYSLOG(LOG_COMM,"Non Intra Search Threshold = %d", resp->s_non_intra_search);
		SYSLOG(LOG_COMM,"Serving Cell Threshold low = %d", resp->thresh_serving_low);
		SYSLOG(LOG_COMM,"Intra Serach Threshold = %d", resp->s_intra_search);

		new_serving_cell_id = resp->serving_cell_id;
		if (new_serving_cell_id != old_serving_cell_id) {
			if ( new_serving_cell_id >= 0 && new_serving_cell_id <= 503) {
				_set_int_db("system_network_status.PCID", resp->serving_cell_id, NULL);
				_set_int_db("system_network_status.pci", resp->serving_cell_id, NULL);
				snprintf(buf, sizeof(buf), "%d,%d,%d", resp->global_cell_id, resp->serving_cell_id, resp->earfcn);
				_set_str_db("system_network_status.eci_pci_earfcn", buf, -1);
			}
			else {
				_set_reset_db("system_network_status.PCID");
				_set_reset_db("system_network_status.pci");
				_set_reset_db("system_network_status.eci_pci_earfcn");
			}
		}

		SYSLOG(LOG_COMM,"N LTE Cells = %d", resp->cells_len);

		struct cell_info_lte_set* lte_set = (struct cell_info_lte_set*) (resp + 1);
		for (i = 0; i < resp->cells_len; i++)
		{
			SYSLOG(LOG_COMM,"Neighbour [%d] PCI = %d", i, lte_set->pci);
			SYSLOG(LOG_COMM,"Neighbour [%d] RSRQ = %d", i, lte_set->rsrq);
			SYSLOG(LOG_COMM,"Neighbour [%d] RSRP = %d", i, lte_set->rsrp);
			SYSLOG(LOG_COMM,"Neighbour [%d] RSSI = %d", i, lte_set->rssi);
			SYSLOG(LOG_COMM,"Neighbour [%d] SRXLVL = %d", i, lte_set->srxlev);

			post_cell_info_to_rdb(n++, 'E',  resp->earfcn, lte_set->pci, (float)lte_set->rsrp/10, (float)lte_set->rsrq/10);

			lte_set++;
		}
		_set_str_db ("wwan.0.cell_measurement.serving_system", "LTE", -1);
	}
	else {
		if (old_serving_cell_id != 0xFFFF) {
			_set_reset_db("system_network_status.PCID");
			_set_reset_db("system_network_status.pci");
			_set_reset_db("system_network_status.eci_pci_earfcn");
		}
	}

	tlv=_get_tlv(msg, QMI_NAS_GET_CELL_LOCATION_INFO_RESP_TYPE_LTE_INFO_INTERFREQUENCY, sizeof(struct cell_info_lte_neighbours_hdr));
	if(tlv) {

		struct cell_info_lte_neighbours_hdr* resp=(struct cell_info_lte_neighbours_hdr*)tlv->v;
		SYSLOG(LOG_COMM,"UE Idle = %d", resp->ue_in_idle);
		SYSLOG(LOG_COMM,"N LTE Freqs = %d", resp->freqs_len);

		/*
		 * The QCM doco is unclear, there is a set of cells for each frequency
		 */
		struct cell_info_lte_inter_freq_set* freq_set = (struct cell_info_lte_inter_freq_set*) (resp + 1);
		for (i = 0; i < resp->freqs_len; i++)
		{
			SYSLOG(LOG_COMM,"Neighbour [%d] EARFCN = %d", i, freq_set->earfcn);
			SYSLOG(LOG_COMM,"Neighbour [%d] Cell reselection priority = %d", i, freq_set->cell_resel_priority);
			SYSLOG(LOG_COMM,"Neighbour [%d] Threshold high = %d", i, freq_set->threshX_high);
			SYSLOG(LOG_COMM,"Neighbour [%d] Threshold low = %d", i, freq_set->threshX_low);
			//freq_set++;

			//unsigned char * p = (unsigned char*) freq_set+1;
			//unsigned char cells_len = *p;
			//SYSLOG(LOG_COMM,"N LTE Cells = %d", cells_len);
			//struct cell_info_lte_set* cells_set = (struct cell_info_lte_set*) (p + 1);

			SYSLOG(LOG_COMM,"N Cells = %d", freq_set->cells_len);
			struct cell_info_lte_set* cells_set = (struct cell_info_lte_set*) (freq_set + 1);
			for (j = 0; j < freq_set->cells_len; j++)
			{
				SYSLOG(LOG_COMM,"Neighbour [%d.%d] PCI = %d", i, j, cells_set->pci);
				SYSLOG(LOG_COMM,"Neighbour [%d.%d] RSRQ = %d", i, j,  cells_set->rsrq);
				SYSLOG(LOG_COMM,"Neighbour [%d.%d] RSRP = %d", i, j, cells_set->rsrp);
				SYSLOG(LOG_COMM,"Neighbour [%d.%d] RSSI = %d", i, j, cells_set->rssi);
				SYSLOG(LOG_COMM,"Neighbour [%d.%d] SRXLVL = %d", i, j, cells_set->srxlev);

				post_cell_info_to_rdb(n++, 'E',  freq_set->earfcn, cells_set->pci, (float)cells_set->rsrp/10, (float)cells_set->rsrq/10);

				cells_set++;
			}
			freq_set = (struct cell_info_lte_inter_freq_set *)cells_set;


		}
	}

	tlv=_get_tlv(msg, QMI_NAS_GET_CELL_LOCATION_INFO_RESP_TYPE_LTE_INFO_WCDMA, sizeof(struct cell_info_lte_neighbours_hdr));
	if(tlv) {

		struct cell_info_lte_neighbours_hdr* resp=(struct cell_info_lte_neighbours_hdr*)tlv->v;
		SYSLOG(LOG_COMM,"UE Idle = %d", resp->ue_in_idle);
		SYSLOG(LOG_COMM,"N WCDMA Freqs = %d", resp->freqs_len);


		struct cell_info_lte_neighbours_wcdma_freqs* freq_set = (struct cell_info_lte_neighbours_wcdma_freqs*) (resp + 1);
		for (i = 0; i < resp->freqs_len; i++)
		{
			SYSLOG(LOG_COMM,"Neighbour [%d] UARFCN = %d", i, freq_set->uarfcn);
			SYSLOG(LOG_COMM,"Neighbour [%d] Cell reselection priority = %d", i, freq_set->cell_resel_priority);
			SYSLOG(LOG_COMM,"Neighbour [%d] Threshold high = %d", i, freq_set->threshX_high);
			SYSLOG(LOG_COMM,"Neighbour [%d] Threshold low = %d", i, freq_set->threshX_low);

			SYSLOG(LOG_COMM,"N Cells = %d", freq_set->cells_len);
			struct cell_info_lte_neighbours_wcdma_cells* cells_set = (struct cell_info_lte_neighbours_wcdma_cells*) (freq_set + 1);
			for (j = 0; j < freq_set->cells_len; j++)
			{
				SYSLOG(LOG_COMM,"Neighbour [%d.%d] PSC = %d", i, j, cells_set->psc);
				SYSLOG(LOG_COMM,"Neighbour [%d.%d] RSCP = %d", i, j, cells_set->cpich_rscp);
				SYSLOG(LOG_COMM,"Neighbour [%d.%d] ECNO= %d", i, j, cells_set->cpich_ecno);
				SYSLOG(LOG_COMM,"Neighbour [%d.%d] SRXLVL = %d", i, j, cells_set->srxlev);

				post_cell_info_to_rdb(n++, 'U',  freq_set->uarfcn, cells_set->psc, (float)cells_set->cpich_rscp/10, (float)cells_set->cpich_ecno/10);

				cells_set++;
			}
			freq_set = (struct cell_info_lte_neighbours_wcdma_freqs *)cells_set;
		}
	}

	tlv=_get_tlv(msg, QMI_NAS_GET_CELL_LOCATION_INFO_RESP_TYPE_LTE_INFO_GSM, sizeof(struct cell_info_lte_neighbours_hdr));
	if(tlv) {

		struct cell_info_lte_neighbours_hdr* resp=(struct cell_info_lte_neighbours_hdr*)tlv->v;
		SYSLOG(LOG_COMM,"UE Idle = %d", resp->ue_in_idle);
		SYSLOG(LOG_COMM,"N GSM Freqs = %d", resp->freqs_len);


		struct cell_info_lte_neighbours_gsm_freqs* freq_set = (struct cell_info_lte_neighbours_gsm_freqs*) (resp + 1);
		for (i = 0; i < resp->freqs_len; i++)
		{
			SYSLOG(LOG_COMM,"Neighbour [%d] Cell reselection priority = %d", i, freq_set->cell_resel_priority);
			SYSLOG(LOG_COMM,"Neighbour [%d] Threshold high = %d", i, freq_set->thresh_gsm_high);
			SYSLOG(LOG_COMM,"Neighbour [%d] Threshold low = %d", i, freq_set->thresh_gsm_low);
			SYSLOG(LOG_COMM,"Neighbour [%d] NCC Permitted = %d", i, freq_set->ncc_permitted);


			SYSLOG(LOG_COMM,"N Cells = %d", freq_set->cells_len);
			struct cell_info_lte_neighbours_gsm_cells* cells_set = (struct cell_info_lte_neighbours_gsm_cells*) (freq_set + 1);
			for (j = 0; j < freq_set->cells_len; j++)
			{
				SYSLOG(LOG_COMM,"Neighbour [%d.%d] ARFCN = %d", i, j, cells_set->arfcn);
				SYSLOG(LOG_COMM,"Neighbour [%d.%d] BAND_1900 = %d", i, j, cells_set->band_1900);
				SYSLOG(LOG_COMM,"Neighbour [%d.%d] CELL_ID_VALID= %d", i, j, cells_set->cell_id_valid);
				SYSLOG(LOG_COMM,"Neighbour [%d.%d] BSIC_ID= %d", i, j, cells_set->bsic_id);
				SYSLOG(LOG_COMM,"Neighbour [%d.%d] RSSI= %d", i, j, cells_set->rssi);
				SYSLOG(LOG_COMM,"Neighbour [%d.%d] SRXLVL = %d", i, j, cells_set->srxlev);

				post_cell_info_to_rdb(n++, 'G',  cells_set->arfcn, cells_set->bsic_id, (float)cells_set->rssi/10, 0);

				cells_set++;
			}
			freq_set = (struct cell_info_lte_neighbours_gsm_freqs *)cells_set;
		}
	}

	/* When attached to a UMTS serving system we can expect the following TLVs:-
	 *
	 * 0x11 QMI_NAS_GET_CELL_LOCATION_INFO_RESP_TYPE_UMTS_INFO - Intra Frequency
	 * 0x17 QMI_NAS_GET_CELL_LOCATION_INFO_RESP_TYPE_UMTS_CELL_ID - Cell ID
	 * 0x18 QMI_NAS_GET_CELL_LOCATION_INFO_RESP_TYPE_UMTS_INFO_LTE - LTE Neighbour Cell Info
	 * 0x1C - WCDMA Extended Cell Info
	 * 0x1D - GSM Neighbour Extended Cell Info
	 */
	tlv=_get_tlv(msg,QMI_NAS_GET_CELL_LOCATION_INFO_RESP_TYPE_UMTS_INFO,sizeof(struct cell_info_umts));
	if(tlv) {
		struct cell_info_umts* resp=(struct cell_info_umts*)tlv->v;

		SYSLOG(LOG_COMM,"Cell ID = %d", resp->cell_id);
		//PLMN SYSLOG(LOG_COMM,"Cell ID = %d", resp->cell_id);
		SYSLOG(LOG_COMM,"LAC = %d", resp->lac);
		SYSLOG(LOG_COMM,"UARFCN = %d", resp->uarfcn);
		SYSLOG(LOG_COMM,"PSC = %d", resp->psc);
		SYSLOG(LOG_COMM,"RSCP = %d", resp->rscp);
		SYSLOG(LOG_COMM,"ECIO = %d", resp->ecio);

		SYSLOG(LOG_COMM,"n = %d", resp->umts_inst);

		#if 0
		/* traditional RDB variables of cell id and LAC - compatiable with other port managers (AT port manager and CnS manager) */
		_set_int_db("system_network_status.CellID", resp->cell_id, NULL);
		_set_int_db("system_network_status.LAC", resp->lac, NULL);
		_set_int_db("system_network_status.PSCs0", resp->psc, NULL);
		_set_int_db("system_network_status.RSCPs0", resp->rscp, NULL);
		_set_int_db("system_network_status.ECIOs0", resp->ecio, NULL);
		#endif


		struct cell_info_monitored_set* umts_set = (struct cell_info_monitored_set*) (resp + 1);
		for (i = 0; i < resp->umts_inst; i++)
		{
			SYSLOG(LOG_COMM,"UMTS Neighbour [%d] UARFCN = %d", i, umts_set->uarfcn);
			SYSLOG(LOG_COMM,"UMTS Neighbour [%d] PSC = %d", i, umts_set->psc);
			SYSLOG(LOG_COMM,"UMTS Neighbour [%d] RSCP = %d", i, umts_set->rscp);
			SYSLOG(LOG_COMM,"UMTS Neighbour [%d] ECIO = %d", i, umts_set->ecio);

			post_cell_info_to_rdb(n++, 'U',  umts_set->uarfcn, umts_set->psc, (float)umts_set->rscp, (float)umts_set->ecio);

			umts_set++;
		}
		unsigned char * p = (unsigned char*) umts_set;
		unsigned char geran_inst = *p;
		SYSLOG(LOG_COMM,"n = %d", geran_inst);
		struct cell_info_geran_set* geran_set = (struct cell_info_geran_set*) (p + 1);
		for (i = 0; i < geran_inst; i++)
		{
			SYSLOG(LOG_COMM,"GSM Neighbour [%d] ARFCN = %d", i, geran_set->arfcn);
			SYSLOG(LOG_COMM,"GSM Neighbour [%d] NCC = %d", i, geran_set->bsic_ncc);
			SYSLOG(LOG_COMM,"GSM Neighbour [%d] BCC = %d", i, geran_set->bsic_bcc);
			SYSLOG(LOG_COMM,"GSM Neighbour [%d] RSSI = %d", i, geran_set->rssi);

			/*
			 * TODO:
			 * Convert geran_set->bsic_ncc + geran_set->bsic_bcc to bsic_id as in LTE set
			 */
			post_cell_info_to_rdb(n++, 'G',  geran_set->arfcn, 0, (float)geran_set->rssi, 0);

			umts_set++;
		}

		_set_str_db ("cell_measurement.serving_system", "UMTS", -1);
	}

	tlv=_get_tlv(msg, QMI_NAS_GET_CELL_LOCATION_INFO_RESP_TYPE_UMTS_INFO_LTE, sizeof(struct cell_info_umts_neighbours_lte));
	if(tlv) {

		struct cell_info_umts_neighbours_lte* resp=(struct cell_info_umts_neighbours_lte*)tlv->v;
		SYSLOG(LOG_COMM,"WCDMA RRC State = %d", resp->wcdma_rrc_state);
		SYSLOG(LOG_COMM,"N LTE Cells = %d", resp->umts_lte_nbr_cell_len);

		struct cell_info_umts_lte_set* cells_set = (struct cell_info_umts_lte_set*) (resp + 1);
		for (i = 0; i < resp->umts_lte_nbr_cell_len; i++)
		{
			SYSLOG(LOG_COMM,"Neighbour [%d] EARFCN = %d", i, cells_set->earfcn);
			SYSLOG(LOG_COMM,"Neighbour [%d] PCI = %d", i, cells_set->pci);
			SYSLOG(LOG_COMM,"Neighbour [%d] RSRQ = %f", i, cells_set->rsrq);
			SYSLOG(LOG_COMM,"Neighbour [%d] RSRP = %f", i, cells_set->rsrp);
			SYSLOG(LOG_COMM,"Neighbour [%d] SRXLVL = %d", i, cells_set->srxlev);
			SYSLOG(LOG_COMM,"Neighbour [%d] IS_TDD = %d", i, cells_set->cell_is_tdd);

			post_cell_info_to_rdb(n++, 'E',  cells_set->earfcn, cells_set->pci, cells_set->rsrp, cells_set->rsrq);

			cells_set++;
		}
	}


	_set_int_db("cell_measurement.qty",n,NULL);
}

