/*
** Copyright (c) 2007-2017 by Silicon Laboratories
**
** $Id: si3217x_revc_intf.c 7011 2018-02-22 20:01:48Z nizajerk $
**
** SI3217X_RevC_Intf.c
** SI3217X RevC ProSLIC interface implementation file
**
** Author(s):
** cdp
**
** Distributed by:
** Silicon Laboratories, Inc
**
** This file contains proprietary information.
** No dissemination allowed without prior written permission from
** Silicon Laboratories, Inc.
**
**
*/

#include "si_voice_datatypes.h"
#include "si_voice_ctrl.h"
#include "si_voice_timer_intf.h"
#include "proslic.h"
#include "si3217x.h"
#include "si3217x_revc_intf.h"
#include "si3217x_common_registers.h"
#include "si3217x_revc_registers.h"
#include "proslic_api_config.h"

#define WriteReg        pProslic->deviceId->ctrlInterface->WriteRegister_fptr
#define ReadReg         pProslic->deviceId->ctrlInterface->ReadRegister_fptr
#define pProHW          pProslic->deviceId->ctrlInterface->hCtrl
#define Reset           pProslic->deviceId->ctrlInterface->Reset_fptr
#define Delay           pProslic->deviceId->ctrlInterface->Delay_fptr
#define pProTimer       pProslic->deviceId->ctrlInterface->hTimer
#define WriteRAM        pProslic->deviceId->ctrlInterface->WriteRAM_fptr
#define ReadRAM         pProslic->deviceId->ctrlInterface->ReadRAM_fptr
#define TimeElapsed     pProslic->deviceId->ctrlInterface->timeElapsed_fptr
#define getTime         pProslic->deviceId->ctrlInterface->getTime_fptr


extern Si3217x_General_Cfg Si3217x_General_Configuration;
#ifdef SIVOICE_MULTI_BOM_SUPPORT
extern const proslicPatch SI3217X_PATCH_C_FLBK;
extern const proslicPatch SI3217X_PATCH_C_BKBT;
extern const proslicPatch SI3217X_PATCH_C_PBB;
extern const proslicPatch SI3217X_PATCH_C_LCQCUK;
extern Si3217x_General_Cfg Si3217x_General_Configuration_MultiBOM[];
extern int si3217x_genconf_multi_max_preset;
#else
extern const proslicPatch SI3217X_PATCH_C_DEFAULT;
#endif
/* Pulse Metering */
#ifndef DISABLE_PULSE_SETUP
extern Si3217x_PulseMeter_Cfg Si3217x_PulseMeter_Presets [];
#endif

#define SI3217X_COM_RAM_DCDC_DCFF_ENABLE SI3217X_COM_RAM_GENERIC_8
#define GCONF Si3217x_General_Configuration


/*
** Constants
*/
#define BIT20LSB                       1048576L
#define OITHRESH_OFFS                  900L
#define OITHRESH_SCALE                 100L
#define OVTHRESH_OFFS                  71000
#define OVTHRESH_SCALE                 3000L
#define UVTHRESH_OFFS                  4057L
#define UVTHRESH_SCALE                 187L
#define UVHYST_OFFS                    548L
#define UVHYST_SCALE                   47L

/*
** Function: Si3217x_RevC_GenParamUpdate
**
** Update general parameters
**
** Input Parameters:
** pProslic: pointer to PROSLIC object array
** fault: error code
**
** Return:
** error code
*/
int Si3217x_RevC_GenParamUpdate(proslicChanType_ptr pProslic,initSeqType seq)
{
  uInt8 data;
  uInt8 oi_sense_scale, oi_sense_shift;
  ramData ram_data, scratch_calculation;

  switch(seq)
  {
    case INIT_SEQ_PRE_CAL:
      /*
      ** Force pwrsave off and disable AUTO-tracking - set to user configured state after cal
      */
      WriteReg(pProHW, pProslic->channel,SI3217X_COM_REG_ENHANCE,0);
      WriteReg(pProHW, pProslic->channel,SI3217X_COM_REG_AUTO,0x2F);

      /*
      ** General Parameter Updates
      */

      WriteRAM(pProHW, pProslic->channel,SI3217X_COM_RAM_P_TH_HVIC,
               Si3217x_General_Configuration.p_th_hvic);
      WriteRAM(pProHW, pProslic->channel,SI3217X_COM_RAM_COEF_P_HVIC,
               Si3217x_General_Configuration.coef_p_hvic);
      WriteRAM(pProHW, pProslic->channel,SI3217X_COM_RAM_BAT_HYST,
               Si3217x_General_Configuration.bat_hyst);
      WriteRAM(pProHW, pProslic->channel,SI3217X_COM_RAM_VBATH_EXPECT,
               Si3217x_General_Configuration.vbath_expect);
      WriteRAM(pProHW, pProslic->channel,SI3217X_COM_RAM_VBATR_EXPECT,
               Si3217x_General_Configuration.vbatr_expect);

      WriteRAM(pProHW, pProslic->channel,SI3217X_COM_RAM_PWRSAVE_TIMER,
               Si3217x_General_Configuration.pwrsave_timer);
      WriteRAM(pProHW, pProslic->channel,SI3217X_COM_RAM_OFFHOOK_THRESH,
               Si3217x_General_Configuration.pwrsave_ofhk_thresh);
      WriteRAM(pProHW, pProslic->channel,SI3217X_COM_RAM_VBAT_TRACK_MIN,
               Si3217x_General_Configuration.vbat_track_min);
      WriteRAM(pProHW, pProslic->channel,SI3217X_COM_RAM_VBAT_TRACK_MIN_RNG,
               Si3217x_General_Configuration.vbat_track_min_rng);

      WriteRAM(pProHW, pProslic->channel,SI3217X_REVC_RAM_THERM_DBI,
               Si3217x_General_Configuration.therm_dbi);
      WriteRAM(pProHW, pProslic->channel,SI3217X_REVC_RAM_VOV_DCDC_SLOPE,
               Si3217x_General_Configuration.vov_dcdc_slope);
      WriteRAM(pProHW, pProslic->channel,SI3217X_REVC_RAM_VOV_DCDC_OS,
               Si3217x_General_Configuration.vov_dcdc_os);
      WriteRAM(pProHW, pProslic->channel,SI3217X_REVC_RAM_VOV_RING_BAT_MAX,
               Si3217x_General_Configuration.vov_ring_bat_max);
      WriteRAM(pProHW, pProslic->channel,SI3217X_REVC_RAM_DCDC_VERR,
               Si3217x_General_Configuration.dcdc_verr);
      WriteRAM(pProHW, pProslic->channel,SI3217X_REVC_RAM_DCDC_VERR_HYST,
               Si3217x_General_Configuration.dcdc_verr_hyst);

      WriteRAM(pProHW, pProslic->channel,SI3217X_COM_RAM_PD_UVLO,
               Si3217x_General_Configuration.pd_uvlo);
      WriteRAM(pProHW, pProslic->channel,SI3217X_COM_RAM_PD_OVLO,
               Si3217x_General_Configuration.pd_ovlo);
      WriteRAM(pProHW, pProslic->channel,SI3217X_COM_RAM_PD_OCLO,
               Si3217x_General_Configuration.pd_oclo);
      WriteRAM(pProHW, pProslic->channel,SI3217X_COM_RAM_PD_SWDRV,
               Si3217x_General_Configuration.pd_swdrv);

      WriteRAM(pProHW, pProslic->channel,SI3217X_COM_RAM_DCDC_UVPOL,
               Si3217x_General_Configuration.dcdc_uvpol);

      WriteRAM(pProHW, pProslic->channel,SI3217X_COM_RAM_DCDC_RNGTYPE,
               Si3217x_General_Configuration.dcdc_rngtype);

      WriteRAM(pProHW, pProslic->channel,SI3217X_COM_RAM_DCDC_ANA_TOFF,
               Si3217x_General_Configuration.dcdc_ana_toff);
      WriteRAM(pProHW, pProslic->channel,SI3217X_COM_RAM_DCDC_ANA_TONMIN,
               Si3217x_General_Configuration.dcdc_ana_tonmin);
      WriteRAM(pProHW, pProslic->channel,SI3217X_COM_RAM_DCDC_ANA_TONMAX,
               Si3217x_General_Configuration.dcdc_ana_tonmax);

      if( Si3217x_General_Configuration.bom_option == BO_DCDC_LCFB)
      {
        WriteRAM(pProHW, pProslic->channel,SI3217X_COM_RAM_DCDC_OIMASK,
               0xE00000);
      }

      /*
      ** Hardcoded RAM
      */
      oi_sense_shift = 0;

      if( Si3217x_General_Configuration.bom_option == BO_DCDC_LCQC_3W )
      {
        oi_sense_scale =
          2; /* OITHRESH should be twice as large as user input when 0.2 ohm Rsense is used -LG, 07/18/14 */
      }else if( Si3217x_General_Configuration.bom_option == BO_DCDC_LCFB ) /* rsense = 0.05, so we need to divide by 2 */
      {
        oi_sense_scale = 1;
        oi_sense_shift = 1; /* What to shift the calculated value by 1 = 1 bit, 0 = no change */
      }
      else
      {
        oi_sense_scale = 1;
      }

      scratch_calculation = (oi_sense_scale*GCONF.i_oithresh) >> oi_sense_shift;
      ram_data = ( scratch_calculation > OITHRESH_OFFS)?
                 (scratch_calculation - OITHRESH_OFFS)/OITHRESH_SCALE:0L;
      ram_data *= BIT20LSB;
      WriteRAM(pProHW, pProslic->channel,SI3217X_COM_RAM_DCDC_OITHRESH,ram_data);

      scratch_calculation = (oi_sense_scale*GCONF.i_oithresh_lo) >> oi_sense_shift;
      ram_data = (scratch_calculation > OITHRESH_OFFS)?
                 (scratch_calculation - OITHRESH_OFFS)/OITHRESH_SCALE:0L;
      ram_data *= BIT20LSB;
      WriteRAM(pProHW, pProslic->channel,SI3217X_REVC_RAM_DCDC_OITHRESH_LO,ram_data);

      scratch_calculation = (oi_sense_scale*GCONF.i_oithresh_hi) >> oi_sense_shift;
      ram_data = (scratch_calculation > OITHRESH_OFFS)?
                 (scratch_calculation - OITHRESH_OFFS)/OITHRESH_SCALE:0L;
      ram_data *= BIT20LSB;
      WriteRAM(pProHW, pProslic->channel,SI3217X_REVC_RAM_DCDC_OITHRESH_HI,ram_data);

      ram_data = (GCONF.v_ovthresh > OVTHRESH_OFFS)?(GCONF.v_ovthresh -
                 OVTHRESH_OFFS)/OVTHRESH_SCALE:0L;
      ram_data *= BIT20LSB;
      WriteRAM(pProHW, pProslic->channel,SI3217X_COM_RAM_DCDC_OVTHRESH,ram_data);

      ram_data = (GCONF.v_uvthresh > UVTHRESH_OFFS)?(GCONF.v_uvthresh -
                 UVTHRESH_OFFS)/UVTHRESH_SCALE:0L;
      ram_data *= BIT20LSB;
      WriteRAM(pProHW, pProslic->channel,SI3217X_COM_RAM_DCDC_UVTHRESH,ram_data);

      ram_data = (GCONF.v_uvhyst > UVHYST_OFFS)?(GCONF.v_uvhyst -
                 UVHYST_OFFS)/UVHYST_SCALE:0L;
      ram_data *= BIT20LSB;
      WriteRAM(pProHW, pProslic->channel,SI3217X_COM_RAM_DCDC_UVHYST,ram_data);

      /* Set default audio gain based on PM bom */
      if(Si3217x_General_Configuration.pm_bom == BO_PM_BOM)
      {
        WriteRAM(pProHW, pProslic->channel,SI3217X_COM_RAM_SCALE_KAUDIO,BOM_KAUDIO_PM);
        WriteRAM(pProHW, pProslic->channel,SI3217X_COM_RAM_AC_ADC_GAIN,
                 BOM_AC_ADC_GAIN_PM);
      }
      else
      {
        WriteRAM(pProHW, pProslic->channel,SI3217X_COM_RAM_SCALE_KAUDIO,
                 BOM_KAUDIO_NO_PM);
        WriteRAM(pProHW, pProslic->channel,SI3217X_COM_RAM_AC_ADC_GAIN,
                 BOM_AC_ADC_GAIN_NO_PM);
      }

      /*
      ** Hardcoded changes to default settings
      */
      data = ReadReg(pProHW, pProslic->channel,SI3217X_COM_REG_GPIO_CFG1);
      data &= 0xF9;  /* Clear DIR for GPIO 1&2 */
      data |= 0x60;  /* Set ANA mode for GPIO 1&2 */
      WriteReg(pProHW,pProslic->channel,SI3217X_COM_REG_GPIO_CFG1,
               data);          /* coarse sensors analog mode */
      WriteReg(pProHW,pProslic->channel,SI3217X_COM_REG_PDN,
               0x80);                /* madc powered in open state */
      WriteRAM(pProHW,pProslic->channel,SI3217X_COM_RAM_TXACHPF_A1_1,
               0x71EB851L); /* Fix HPF corner */
      WriteRAM(pProHW,pProslic->channel,SI3217X_COM_RAM_PD_REF_OSC,
               0x200000L);    /* PLL freerun workaround */
      WriteRAM(pProHW,pProslic->channel,SI3217X_COM_RAM_ILOOPLPF,
               0x4EDDB9L);      /* 20pps pulse dialing enhancement */
      WriteRAM(pProHW,pProslic->channel,SI3217X_COM_RAM_ILONGLPF,
               0x806D6L);       /* 20pps pulse dialing enhancement */
      WriteRAM(pProHW,pProslic->channel,SI3217X_COM_RAM_VDIFFLPF,
               0x10038DL);      /* 20pps pulse dialing enhancement */
      WriteRAM(pProHW, pProslic->channel,SI3217X_COM_RAM_DCDC_VREF_CTRL,0x0L);
      WriteRAM(pProHW, pProslic->channel,SI3217X_COM_RAM_VCM_TH,0x106240L);
      WriteRAM(pProHW, pProslic->channel,SI3217X_COM_RAM_VCMLPF,0x10059FL);
      WriteRAM(pProHW, pProslic->channel,SI3217X_COM_RAM_CM_SPEEDUP_TIMER,0x0F0000);
      WriteRAM(pProHW, pProslic->channel,SI3217X_COM_RAM_VCM_HYST,0x206280L);

      /* Prevent Ref Osc from powering down in PLL Freerun mode (pd_ref_osc) */
      ram_data = ReadRAM(pProHW, pProslic->channel,SI3217X_COM_RAM_PWRSAVE_CTRL_LO);
      WriteRAM(pProHW, pProslic->channel,SI3217X_COM_RAM_PWRSAVE_CTRL_LO,
               ram_data&0x07FFFFFFL); /* clear b27 */
      break;


    case INIT_SEQ_POST_CAL:
      WriteReg(pProHW, pProslic->channel,SI3217X_COM_REG_ENHANCE,
               Si3217x_General_Configuration.enhance&0x1F);
      WriteReg(pProHW, pProslic->channel,SI3217X_COM_REG_AUTO,
               Si3217x_General_Configuration.auto_reg);
      if(Si3217x_General_Configuration.zcal_en)
      {
        WriteReg(pProHW,pProslic->channel, SI3217X_COM_REG_ZCAL_EN, 0x04);
      }
      break;

    default:
      break;
  }
  return RC_NONE;
}


/*
** Function: Si3217x_RevC_SelectPatch
**
** Select patch based on general parameters
**
** Input Parameters:
** pProslic: pointer to PROSLIC object array
** fault: error code
** patch:    Pointer to proslicPatch pointer
**
** Return:
** error code
*/
int Si3217x_RevC_SelectPatch(proslicChanType_ptr pProslic,
                             const proslicPatch **patch)
{

#ifdef SIVOICE_MULTI_BOM_SUPPORT
  if( (Si3217x_General_Configuration.bom_option == BO_DCDC_FLYBACK)
      || (Si3217x_General_Configuration.bom_option == BO_DCDC_LCUB)
      || (Si3217x_General_Configuration.bom_option == BO_DCDC_LCFB) )
  {
    *patch = &(SI3217X_PATCH_C_FLBK);
  }
  else if(Si3217x_General_Configuration.bom_option == BO_DCDC_BUCK_BOOST)
  {
    *patch = &(SI3217X_PATCH_C_BKBT);
  }
  else if(Si3217x_General_Configuration.bom_option == BO_DCDC_PMOS_BUCK_BOOST)
  {
    *patch = &(SI3217X_PATCH_C_PBB);
  }
  else if( (Si3217x_General_Configuration.bom_option == BO_DCDC_LCQC_3W)
           || (Si3217x_General_Configuration.bom_option == BO_DCDC_LCQC_6W) 
           || (Si3217x_General_Configuration.bom_option == BO_DCDC_LCQC_7P6W) )
  {
   *patch = &(SI3217X_PATCH_C_LCQCUK);  
  }
  else
  {
    DEBUG_PRINT(pProslic, "%sChannel %d : Invalid Patch\n", LOGPRINT_PREFIX,
                pProslic->channel);
    pProslic->channelEnable = 0;
    pProslic->error = RC_INVALID_PATCH;
    return RC_INVALID_PATCH;
  }
#else
  SILABS_UNREFERENCED_PARAMETER(pProslic);
  *patch = &(SI3217X_PATCH_C_DEFAULT);
#endif

  return RC_NONE;
}


/*
** Function: Si3217x_RevC_ConverterSetup
**
** Description:
** Program revision specific settings before powering converter
**
** Specifically, from general parameters and knowledge that this
** is Rev C, setup dcff drive, gate drive polarity, and charge pump.
**
** Returns:
** int (error)
**
*/
int Si3217x_RevC_ConverterSetup(proslicChanType_ptr pProslic)
{
  ramData inv_on;
  ramData inv_off;

  /* Option to add a per-channel inversion for maximum flexibility */
  if(pProslic->dcdc_polarity_invert)
  {
    inv_on  = 0x0L;
    inv_off = 0x100000L;
  }
  else
  {
    inv_on  = 0x100000L;
    inv_off = 0x0L;
  }

  switch(Si3217x_General_Configuration.bom_option)
  {
    case BO_DCDC_FLYBACK:
    case BO_DCDC_LCQC_3W:
    case BO_DCDC_LCQC_6W:
    case BO_DCDC_LCQC_7P6W:
    case BO_DCDC_LCUB:
    case BO_DCDC_LCFB:
      /*
      ** RevC designs may or may not use gate driver, depending on the hardware
      */
      WriteRAM(pProHW, pProslic->channel,SI3217X_REVC_RAM_DCDC_LIFT_EN,
               0x0L);      /* dcff disabled */
      if(Si3217x_General_Configuration.gdrv_option == BO_GDRV_INSTALLED)
      {
        WriteRAM(pProHW, pProslic->channel,SI3217X_COM_RAM_DCDC_SWDRV_POL,
                 inv_on); /* inverted */
        WriteRAM(pProHW, pProslic->channel,SI3217X_REVC_RAM_DCDC_CPUMP,
                 0x0L);          /* Charge pump off */
      }
      else
      {
        WriteRAM(pProHW, pProslic->channel,SI3217X_COM_RAM_DCDC_SWDRV_POL,
                 inv_off);    /* non-inverted */
        WriteRAM(pProHW, pProslic->channel,SI3217X_REVC_RAM_DCDC_CPUMP,
                 0x100000L);   /* Charge pump on */
        Delay(pProTimer,20); /* Cpump settle */
      }
      break;

    case BO_DCDC_BUCK_BOOST:
      /*
      ** RevC buck-boost designs are identical to RevB - no gate drive,
      ** dcff enabled, non-inverting (charge pump off)
      */
      WriteRAM(pProHW, pProslic->channel,SI3217X_REVC_RAM_DCDC_CPUMP,0x0L);
      WriteRAM(pProHW, pProslic->channel,SI3217X_COM_RAM_DCDC_SWDRV_POL,inv_off);
      WriteRAM(pProHW, pProslic->channel,SI3217X_REVC_RAM_DCDC_LIFT_EN,
               0x100000L); /* dcff enabled */
      break;

    case BO_DCDC_PMOS_BUCK_BOOST:
      /*
      ** RevC design options
      **     *  No gate driver + no dcff drive (default revC hardware)
      **     *  Gate driver + no dcff drive    (VDC_9P0_24P0 w/ revB hardware)
      **     *  Gate driver + dcff drive       (VDC_3P0_6P0 w/ revB hardware) - deprecated
      */
      if(Si3217x_General_Configuration.gdrv_option == BO_GDRV_INSTALLED)
      {
        WriteRAM(pProHW, pProslic->channel,SI3217X_COM_RAM_DCDC_SWDRV_POL,
                 inv_off); /* non-inverted */
        WriteRAM(pProHW, pProslic->channel,SI3217X_REVC_RAM_DCDC_CPUMP,
                 0x0L);       /* Charge pump off */
        if(Si3217x_General_Configuration.vdc_range == VDC_3P0_6P0)
        {
          WriteRAM(pProHW, pProslic->channel,SI3217X_COM_RAM_DCDC_DCFF_ENABLE,
                   0x10000000L); /* dcff enabled */
        }
        else
        {
          WriteRAM(pProHW, pProslic->channel,SI3217X_REVC_RAM_DCDC_LIFT_EN,
                   0x0L);      /* dcff disabled */
        }
      }
      else
      {
        WriteRAM(pProHW, pProslic->channel,SI3217X_COM_RAM_DCDC_SWDRV_POL,
                 inv_on);  /* inverted */
        WriteRAM(pProHW, pProslic->channel,SI3217X_REVC_RAM_DCDC_CPUMP,
                 0x100000L);  /* Charge pump on */
        WriteRAM(pProHW, pProslic->channel,SI3217X_REVC_RAM_DCDC_LIFT_EN,
                 0x0L);      /* dcff disabled */
      }
      Delay(pProTimer,20); /* Cpump settle */

      break;

    default:
      return RC_DCDC_SETUP_ERR;
  }

  return RC_NONE;
}


/*
** Function: Si3217x_RevC_PulseMeterSetup
**
** Description:
** configure pulse metering
*/
#ifndef DISABLE_PULSE_SETUP
int Si3217x_RevC_PulseMeterSetup (proslicChanType *pProslic, int preset)
{
  uInt8 reg;
  WriteRAM(pProHW,pProslic->channel,SI3217X_COM_RAM_PM_AMP_THRESH,
           Si3217x_PulseMeter_Presets[preset].pm_amp_thresh);
  reg = (Si3217x_PulseMeter_Presets[preset].pmFreq<<1)|
        (Si3217x_PulseMeter_Presets[preset].pmAuto<<3);
  WriteRAM(pProHW,pProslic->channel,SI3217X_REVC_RAM_PM_ACTIVE,
           Si3217x_PulseMeter_Presets[preset].pmActive);
  WriteRAM(pProHW,pProslic->channel,SI3217X_REVC_RAM_PM_INACTIVE,
           Si3217x_PulseMeter_Presets[preset].pmInactive);
  WriteReg(pProHW,pProslic->channel,SI3217X_COM_REG_PMCON,reg);
  return RC_NONE;
}

#endif

