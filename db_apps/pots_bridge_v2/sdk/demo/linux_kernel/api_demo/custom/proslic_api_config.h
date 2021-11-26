/*
* Copyright 2007-2013 by Silicon Laboratories
*
* $Id: proslic_api_config.h 5156 2015-10-06 18:23:43Z nizajerk $
*
*
* Author(s): 
* laj
*
* Distributed by: 
* Silicon Laboratories, Inc
*
* This file contains proprietary information.	 
* No dissemination allowed without prior written permission from
* Silicon Laboratories, Inc.
*
*
*/
#ifndef PROSLIC_API_CFG
#define PROSLIC_API_CFG

/** @defgroup PROSLIC_CFG ProSLIC Configuration Options
 * @{
 */

/** @defgroup CODE_OPTS Code feature options 
 * Select which options NOT to build - just uncomment out the undef to disable feature.
 * @{
 */
#define DISABLE_VERIFY_PATCH      /**< Disable patch load verification */
#undef DISABLE_VERIFY_PATCH 
#define DISABLE_DTMF_SETUP        /**< Disable the DTMF API */ 
/*#undef DISABLE_DTMF_SETUP */
#define DISABLE_FSK_SETUP         /**< DIsable the FSK setup API */
#undef DISABLE_FSK_SETUP 
#define DISABLE_TONE_SETUP        /**< Disable Tone setup API */
#undef DISABLE_TONE_SETUP 
#define DISABLE_RING_SETUP        /**< Disable Ring setup API */
#undef DISABLE_RING_SETUP  
#define DISABLE_DCFEED_SETUP      /**< Disable DC FEED setup API */
#undef DISABLE_DCFEED_SETUP 
#define DISABLE_GPIO_SETUP        /**< Disable GPIO setup API */
#undef DISABLE_GPIO_SETUP 
#define DISABLE_PCM_SETUP         /**< Disable PCM setup API */
#undef DISABLE_PCM_SETUP 
#define ENABLE_DEBUG              /**< Enable debug messages - function entry, etc. */
/*#undef ENABLE_DEBUG */ 
#define DISABLE_CI_SETUP          /**< Disable CI Setup */
#undef DISABLE_CI_SETUP 
#define DISABLE_ZSYNTH_SETUP      /**< Disable Zsyth/impedance setup */
#undef DISABLE_ZSYNTH_SETUP  
#define DISABLE_MALLOC            /**< Don't use MALLOC/FREE, instead assume user will statically allocate */
#undef DISABLE_MALLOC 
#define DISABLE_HPF_WIDEBAND      /**< Disable RX and TX HPF when in WIDEBAND mode */
#undef DISABLE_HPF_WIDEBAND
#define PROSLIC_OPTIMIZE_INTERRUPTS /**< Enable optimization for interrupts for dual channel devices.  This
                                         optimization assumes channel number is on even boundaries - that is
                                         0,2,4, are the first channel for each dual channel devices.
                                     */


#define SIVOICE_NEON_MWI_SUPPORT /**< Enable NEON Message Waiting Indicator support */

#define GCI_MODE  /**< Set if GCI vs. SPI/PCM mode is to be used */
#undef GCI_MODE  

#define ENABLE_HIRES_GAIN         /**<  Set for zsynth preset gains in dB*10 rather than dB */
#undef  ENABLE_HIRES_GAIN         

#define PRINT_TO_STRING 0         /**< Set this to 1 if printing to a string buffer vs. console - you may change/remove this*/


/** @defgroup MULTI_BOM Multiple Device/BOM Option Support
 * Assign patch structure names to macros used in device drivers
* @{ */
#define SIVOICE_MULTI_BOM_SUPPORT     /**< Enable Multiple General Configuration Support */
#undef SIVOICE_MULTI_BOM_SUPPORT 

#ifdef SIVOICE_MULTI_BOM_SUPPORT
#define SI3217X_PATCH_B_BKBT       si3217xPatchRevBBkbt  /**< Si3217x B Buck-boost DCDC Converter patch */
#define SI3217X_PATCH_B_PBB        si3217xPatchRevBBkbt  /**< Si3217x B PMOS Buck-boost DCDC Converter patch */
#define SI3217X_PATCH_C_FLBK       si3217xPatchRevCFlbk  /**< Si3217x C Flyback DCDC Converter patch */
#define SI3217X_PATCH_C_BKBT       si3217xPatchRevCFlbk  /**< Si3217x C Buck-boost DCDC Converter patch */
#define SI3217X_PATCH_C_PBB        si3217xPatchRevCFlbk  /**< Si3217x C PMOS Buck-boost DCDC Converter patch */
#define SI3217X_PATCH_C_LCQCUK     si3217xPatchRevCFlbk  /**< Si3217x C Quasi-CUK DCDC Converter patch */

#define SI3226X_PATCH_C_FLBK       si3226xPatchRevCFlbk    /**< Si3226x RevC Flyback DCDC Converter Patch */
#define SI3226X_PATCH_C_CUK        si3226xPatchRevCFlbk    /**< Si3226x RevC Full CUK DCDC Converter Patch */
#define SI3226X_PATCH_C_QCUK       si3226xPatchRevCFlbk    /**< Si3226x RevC Quasi-CUK DCDC Converter Patch */
#define SI3226X_PATCH_C_LCQCUK     si3226xPatchRevCFlbk    /**< Si3226x RevC Low-cost QCUK DCDC Converter Patch */
#define SI3226X_PATCH_C_TSS        si3226xPatchRevCTss     /**< Si3226x RevC TSS DCDC Converter Patch */
#define SI3226X_PATCH_C_TSS_ISO    si3226xPatchRevCTssIso  /**< Si3226x RevC TSS (isolated) DCDC Converter Patch */
#define SI3226X_PATCH_C_PBB        si3226xPatchRevCFlbk    /**< Si3226x PMOS Buck-boost DCDC Converter patch */   
#define SI3226x_PATCH_C_FIXRL      SI3226X_PATCH_C_TSS
#define SI3226X_PATCH_C_QSS        si3226xPatchRevCTss     /**< Si3226x RevC QSS DCDC Converter Patch */
#define SI3226X_PATCH_C_BB         si3226xPatchRevCFlbk    /**< Si3226x BJT Buck-boost DCDC Converter patch */   

#endif

/* Default patch names for backwards compatibility */
#define SI3217X_PATCH_B_DEFAULT    RevBPatch
#define SI3217X_PATCH_C_DEFAULT    RevCPatch
#define SI3226X_PATCH_C_DEFAULT    RevCPatch


/** @} MULTI_BOM */


#include "stdio.h"
#if (PRINT_TO_STRING)  
extern char outputBuffer[]; 
#define LOGPRINT(...) sprintf(&(outputBuffer[strlen(outputBuffer)]),__VA_ARGS__) 
#else 
#define LOGPRINT printf
#endif 

/** @defgroup PSTN_CFG PSTN Detection Options
* @{ */
#define PSTN_DET_ENABLE                  /**< Define to include Differential PSTN detection code */
#undef PSTN_DET_ENABLE

#define PSTN_DET_OPEN_FEMF_SETTLE   1500     /**< OPEN foreign voltage measurement settle time */
#define PSTN_DET_DIFF_SAMPLES       4        /**< Number of I/V samples averaged [1 to 16] */
#define PSTN_DET_MIN_ILOOP          700      /**< Minimum acceptable loop current */
#define PSTN_DET_MAX_FEMF           10000    /**< Maximum OPEN state foreign voltage */
#define PSTN_DET_POLL_RATE          10       /**< Rate of re-entrant code in ms */
#define PSTN_DET_DIFF_IV1_SETTLE    1000     /**< Settle time before first I/V measurment in ms */
#define PSTN_DET_DIFF_IV2_SETTLE    1000     /**< Settle time before first I/V measurment in ms */

/** @} PSTN_CFG */
#define SIVOICE_CFG_NEWTYPES_ONLY  1 /**< Set if not supporting Legacy types */

/**@} */
#endif
