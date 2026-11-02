/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    lora_at.h
  * @author  MCD Application Team
  * @brief   Header for driver at.c module
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __LORA_AT_H__
#define __LORA_AT_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "LmHandler.h"
#include "stm32_adv_trace.h"
#include "lw_types.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/*
 * AT Command Id errors. Note that they are in sync with ATError_description static array
 * in command.c
 */
typedef enum eATEerror
{
  AT_OK = 0,
  AT_ERROR,
  AT_PARAM_ERROR,
  AT_BUSY_ERROR,
  AT_TEST_PARAM_OVERFLOW,
  AT_NO_NET_JOINED,
  AT_RX_ERROR,
  AT_NO_CLASS_B_ENABLE,
  AT_DUTYCYCLE_RESTRICTED,
  AT_CRYPTO_ERROR,
  AT_MAX,
} ATEerror_t;

/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* External variables --------------------------------------------------------*/
/* USER CODE BEGIN EV */

/* USER CODE END EV */

/* Exported macro ------------------------------------------------------------*/
/* AT printf */
#define AT_PRINTF(...)     do{  UTIL_ADV_TRACE_COND_FSend(VLEVEL_OFF, T_REG_OFF, TS_OFF, __VA_ARGS__);}while(0)
#define AT_PPRINTF(...)    do{ } while( UTIL_ADV_TRACE_OK \
                               != UTIL_ADV_TRACE_COND_FSend(VLEVEL_ALWAYS, T_REG_OFF, TS_OFF, __VA_ARGS__) ) /*Polling Mode*/

/* AT Command strings. Commands start with AT */
/* General commands */
#define AT_VER        "+VER"
#define AT_RESET      "+RESET"

/* Keys, IDs and EUIs management commands */
#define AT_NWKKEY     "+NWKKEY"
#define AT_APPKEY     "+APPKEY"
#define AT_NWKSKEY    "+NWKSKEY"
#define AT_APPSKEY    "+APPSKEY"
#define AT_DADDR      "+DADDR"
#define AT_DEUI       "+DEUI"
#define AT_NWKID      "+NWKID"

/* LoRaWAN join and send data commands */
#define AT_LINKC      "+LINKC"

/* LoRaWAN network management commands */
#define AT_ADR        "+ADR"
#define AT_DR         "+DR"
#define AT_CLASS      "+CLASS"
#define AT_JN1DL      "+JN1DL"
#define AT_JN2DL      "+JN2DL"
#define AT_RX2DR      "+RX2DR"
#define AT_RX2FQ      "+RX2FQ"
#define AT_PGSLOT     "+PGSLOT"

/* Radio tests commands */
#define AT_TTH        "+TTH"
#define AT_TOFF       "+TOFF"

/* Radio access commands */
#define AT_REGW       "+REGW"
#define AT_REGR       "+REGR"

/* USER CODE BEGIN EM */
// INSET commands
#define AT_WDT		   "+WDT"
#define AT_LOG		   "+LOG"
#define AT_CH		   "+CH"
#define AT_MODE		   "+MODE"
#define AT_POWER	   "+POWER"
#define AT_LW		   "+LW"
#define AT_PORT		   "+PORT"
#define AT_RETRY	   "+RETRY"
#define AT_DELAY	   "+DELAY"
#define AT_RXWIN1	   "+RXWIN1"
#define AT_KEY		   "+KEY"
#define AT_ID		   "+ID"
#define AT_MSGHEX	   "+MSGHEX"
#define AT_CMSGHEX	   "+CMSGHEX"
#define AT_REPT		   "+REPT"
#define AT_TEMP		   "+TEMP"
#define AT_RTC		   "+RTC"
#define AT_EEPROM	   "+EEPROM"
#define AT_TEST		   "+TEST"
#define AT_LOWPOWER	   "+LOWPOWER"
#define AT_UART		   "+UART"
/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
/**
  * @brief  Return AT_OK in all cases
  * @param  param string of the AT command - unused
  * @retval AT_OK
  */
ATEerror_t AT_return_ok(const char *param);

/**
  * @brief  Return AT_ERROR in all cases
  * @param  param string of the AT command - unused
  * @retval AT_ERROR
  */
ATEerror_t AT_return_error(const char *param);

/* --------------- Application events --------------- */
/**
  * @brief  Event callback on join
  * @param params
  */
void AT_event_join(LmHandlerJoinParams_t *params);

/**
  * @brief  Event callback on received data
  * @param appData
  * @param params
  */
void AT_event_receive(LmHandlerAppData_t *appData, LmHandlerRxParams_t *params);

/**
  * @brief  Event callback on confirmed acknowledge
  * @param  params
  */
void AT_event_confirm(LmHandlerTxParams_t *params);

/**
  * @brief  Event callback on class updated
  * @param  deviceClass
  */
void AT_event_ClassUpdate(DeviceClass_t deviceClass);

/**
  * @brief  Event callback on beacon status updated
  * @param  params
  */
void AT_event_Beacon(LmHandlerBeaconParams_t *params);

void AT_event_OnNvmDataChange(LmHandlerNvmContextStates_t state);

void AT_event_OnStoreContextRequest(void *nvm, uint32_t nvm_size);

void AT_event_OnRestoreContextRequest(void *nvm, uint32_t nvm_size);

/* --------------- General commands --------------- */
/**
  * @brief  Print the version of the AT_Slave FW
  * @param  param String parameter
  * @retval AT_OK
  */
ATEerror_t AT_run();
/**
  * @brief  Print the version of the AT_Slave FW
  * @param  param String parameter
  * @retval AT_OK
  */
ATEerror_t AT_version_get(const char *param);

/**
  * @brief  Trig a reset of the MCU
  * @param  param string of the AT command - unused
  * @retval AT_OK
  */
ATEerror_t AT_reset(const char *param);

/* --------------- Keys, IDs and EUIs management commands --------------- */
/**
  * @brief  Print Network Key
  * @param  param string of the AT command
  * @retval AT_OK
  */
ATEerror_t AT_NwkKey_get(const char *param);

/**
  * @brief  Set Network Key
  * @param  param string of the AT command
  * @retval AT_OK if OK, or an appropriate AT_xxx error code
  */
ATEerror_t AT_NwkKey_set(const char *param);

/**
  * @brief  Print Application Key
  * @param  param string of the AT command
  * @retval AT_OK
  */
ATEerror_t AT_AppKey_get(const char *param);

/**
  * @brief  Set Application Key
  * @param  param string of the AT command
  * @retval AT_OK if OK, or an appropriate AT_xxx error code
  */
ATEerror_t AT_AppKey_set(const char *param);

/**
  * @brief  Print Network Session Key
  * @param  param string of the AT command
  * @retval AT_OK
  */
ATEerror_t AT_NwkSKey_get(const char *param);

/**
  * @brief  Set Network Session Key
  * @param  param String pointing to provided DevAddr
  * @retval AT_OK if OK, or an appropriate AT_xxx error code
  */
ATEerror_t AT_NwkSKey_set(const char *param);

/**
  * @brief  Print Application Session Key
  * @param  param string of the AT command
  * @retval AT_OK
  */
ATEerror_t AT_AppSKey_get(const char *param);

/**
  * @brief  Set Application Session Key
  * @param  param String pointing to provided DevAddr
  * @retval AT_OK if OK, or an appropriate AT_xxx error code
  */
ATEerror_t AT_AppSKey_set(const char *param);

/**
  * @brief  Print the DevAddr
  * @param  param String pointing to provided DevAddr
  * @retval AT_OK
  */
ATEerror_t AT_DevAddr_get(const char *param);

/**
  * @brief  Set DevAddr
  * @param  param String pointing to provided DevAddr
  * @retval AT_OK if OK, or an appropriate AT_xxx error code
  */
ATEerror_t AT_DevAddr_set(const char *param);

/**
  * @brief  Print Device EUI
  * @param  param string of the AT command - unused
  * @retval AT_OK
  */
ATEerror_t AT_DevEUI_get(const char *param);

/**
  * @brief  Set Device EUI
  * @param  param string of the AT command
  * @retval AT_OK if OK
  */
ATEerror_t AT_DevEUI_set(const char *param);

/**
  * @brief  Print the Network ID
  * @param  param String pointing to provided parameter
  * @retval AT_OK if OK, or an appropriate AT_xxx error code
  */
ATEerror_t AT_NetworkID_get(const char *param);

/**
  * @brief  Set the Network ID
  * @param  param String pointing to provided parameter
  * @retval AT_OK if OK, or an appropriate AT_xxx error code
  */
ATEerror_t AT_NetworkID_set(const char *param);

/* --------------- LoRaWAN join and send data commands --------------- */
/**
  * @brief  Piggyback a Link Check Request to the next uplink
  * @param  param String parameter
  * @retval AT_OK if OK, or an appropriate AT_xxx error code
  */
ATEerror_t AT_Link_Check(const char *param);

/* --------------- LoRaWAN network management commands --------------- */
/**
  * @brief  Print Adaptive Data Rate setting
  * @param  param String pointing to provided ADR setting
  * @retval AT_OK if OK, or an appropriate AT_xxx error code
  */
ATEerror_t AT_ADR_get(const char *param);

/**
  * @brief  Set Adaptive Data Rate setting
  * @param  param String pointing to provided ADR setting
  * @retval AT_OK if OK, or an appropriate AT_xxx error code
  */
ATEerror_t AT_ADR_set(const char *param);

/**
  * @brief  Print Data Rate
  * @param  param String pointing to provided rate
  * @retval AT_OK if OK, or an appropriate AT_xxx error code
  */
ATEerror_t AT_DataRate_get(const char *param);

/**
  * @brief  Set Data Rate
  * @param  param String pointing to provided rate
  * @retval AT_OK if OK, or an appropriate AT_xxx error code
  */
ATEerror_t AT_DataRate_set(const char *param);

/**
  * @brief  Print the Device Class
  * @param  param String pointing to provided param
  * @retval AT_OK if OK, or an appropriate AT_xxx error code
  */
ATEerror_t AT_DeviceClass_get(const char *param);

/**
  * @brief  Set the Device Class
  * @param  param String pointing to provided param
  * @retval AT_OK if OK, or an appropriate AT_xxx error code
  */
ATEerror_t AT_DeviceClass_set(const char *param);

/**
  * @brief  Print the delay between the end of the Tx and the Join Rx Window 1
  * @param  param String pointing to provided param
  * @retval AT_OK if OK, or an appropriate AT_xxx error code
  */
ATEerror_t AT_JoinAcceptDelay1_get(const char *param);

/**
  * @brief  Set the delay between the end of the Tx and the Join Rx Window 1
  * @param  param String pointing to provided param
  * @retval AT_OK if OK, or an appropriate AT_xxx error code
  */
ATEerror_t AT_JoinAcceptDelay1_set(const char *param);

/**
  * @brief  Print the delay between the end of the Tx and the Join Rx Window 2
  * @param  param String pointing to provided param
  * @retval AT_OK if OK, or an appropriate AT_xxx error code
  */
ATEerror_t AT_JoinAcceptDelay2_get(const char *param);

/**
  * @brief  Set the delay between the end of the Tx and the Join Rx Window 2
  * @param  param String pointing to provided param
  * @retval AT_OK if OK, or an appropriate AT_xxx error code
  */
ATEerror_t AT_JoinAcceptDelay2_set(const char *param);

/**
  * @brief  Print Rx2 window data rate
  * @param  param String pointing to parameter
  * @retval AT_OK if OK, or an appropriate AT_xxx error code
  */
ATEerror_t AT_Rx2DataRate_get(const char *param);

/**
  * @brief  Set Rx2 window data rate
  * @param  param String pointing to parameter
  * @retval AT_OK if OK, or an appropriate AT_xxx error code
  */
ATEerror_t AT_Rx2DataRate_set(const char *param);

/**
  * @brief  Print Rx2 window frequency
  * @param  param String pointing to parameter
  * @retval AT_OK if OK, or an appropriate AT_xxx error code
  */
ATEerror_t AT_Rx2Frequency_get(const char *param);

/**
  * @brief  Set Rx2 window frequency
  * @param  param String pointing to parameter
  * @retval AT_OK if OK, or an appropriate AT_xxx error code
  */
ATEerror_t AT_Rx2Frequency_set(const char *param);

/**
  * @brief  Get the unicast pingslot periodicity, Data rate and PsFrequency
  * @param  param String parameter
  * @retval AT_OK
  */
ATEerror_t AT_PingSlot_get(const char *param);

/**
  * @brief  Set the unicast pingslot periodicity
  * @param  param String parameter
  * @retval AT_OK
  */
ATEerror_t AT_PingSlot_set(const char *param);

/* --------------- Radio tests commands --------------- */
/**
  * @brief  Start Tx hopping
  * @param  param String parameter
  * @retval AT_OK
  */
ATEerror_t AT_test_tx_hopping(const char *param);

/**
  * @brief  stop Rx or Tx test
  * @param  param String parameter
  * @retval AT_OK
  */
ATEerror_t AT_test_stop(const char *param);

/* --------------- Radio access commands --------------- */
/**
  * @brief  Write Radio Register
  * @param  param String parameter
  * @retval AT_OK
  */
ATEerror_t AT_write_register(const char *param);

/**
  * @brief  Read Radio Register
  * @param  param String parameter
  * @retval AT_OK
  */
ATEerror_t AT_read_register(const char *param);

/* USER CODE BEGIN EFP */
// INSET commands
ATEerror_t AT_wdt_set(const char *param);
ATEerror_t AT_wdt_get(const char *param);
ATEerror_t AT_log_set(const char *param);
ATEerror_t AT_ch_set(const char *param);
void AT_ch_debug_print(uint8_t id);
ATEerror_t AT_ch_get(const char *param);
ATEerror_t AT_mode_set(const char *param);
ATEerror_t AT_mode_get(const char *param);
ATEerror_t AT_power_set(const char *param);
ATEerror_t AT_power_get(const char *param);
ATEerror_t AT_lw_set(const char *param);
ATEerror_t AT_lw_get(const char *param);
ATEerror_t AT_port_set(const char *param);
ATEerror_t AT_port_get(const char *param);
ATEerror_t AT_retry_set(const char *param);
ATEerror_t AT_delay_set(const char *param);
ATEerror_t AT_delay_get(const char *param);
ATEerror_t AT_rxwin1_set(const char *param);
ATEerror_t AT_key_set(const char *param);
ATEerror_t AT_id_set(const char *param);
ATEerror_t AT_msghex_set(const char *param);
ATEerror_t AT_cmsghex_set(const char *param);
void AT_msghex_result(LW_TxResult_t result, bool confirmed);
ATEerror_t AT_rept_set(const char *param);
ATEerror_t AT_temp_run(const char *param);
ATEerror_t AT_rtc_set(const char *param);
ATEerror_t AT_rtc_get(const char *param);
ATEerror_t AT_eeprom_set(const char *param);
ATEerror_t AT_test_set(const char *param);
ATEerror_t AT_lowpower_set(const char *param);
ATEerror_t AT_lowpower_run(const char *param);
ATEerror_t AT_uart_set(const char *param);
/* USER CODE END EFP */

#ifdef __cplusplus
}
#endif

#endif /* __LORA_AT_H__ */
