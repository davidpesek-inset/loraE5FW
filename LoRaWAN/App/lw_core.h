/*
 * lw_core.h
 *
 *  Created on: 30. 10. 2026
 *      Author: pesek
 *
 *  LoRaWAN stack state: NVM context, LWABP / TEST mode, ABP activation.
 */

#ifndef LORAWAN_APP_LW_CORE_H_
#define LORAWAN_APP_LW_CORE_H_

#include "lw_types.h"

LW_Status_t lw_nvm_get(LoRaMacNvmData_t** nvm);
LW_Status_t lw_nvm_store(uint8_t force);
LW_Status_t lw_mode_set(uint8_t mode);
LW_Status_t lw_abp_deactivate(void);

#endif /* LORAWAN_APP_LW_CORE_H_ */
