/**
 * @file    lw_core.h
 * @author  pesek
 * @date    30. 10. 2026
 * @brief   LoRaWAN stack state: NVM context, LWABP / TEST mode, ABP activation.
 */

#ifndef LORAWAN_APP_LW_CORE_H_
#define LORAWAN_APP_LW_CORE_H_

#include "lw_types.h"

/**
 * @brief Gets a pointer to the LoRaMac NVM context (MIB_NVM_CTXS).
 *
 * The context holds the whole LoRaWAN state: keys, DevAddr, frame counters, channels,
 * DR, TX power, RX delays, NbTrans... Changes written through the pointer act immediately,
 * but they are stored in flash only by lw_nvm_store().
 *
 * @param[out] nvm Pointer to the NVM context (owned by LoRaMac, valid for the whole run).
 * @retval LW_OK    Success.
 * @retval LW_ERROR MIB request refused.
 */
LW_Status_t lw_nvm_get(LoRaMacNvmData_t** nvm);

/**
 * @brief Stores the LoRaWAN NVM context to flash.
 *
 * Stored only when a setter changed the configuration (@c nvm_dirty) or @p force = 1.
 * LmHandlerNvmDataStore() itself writes the flash only when the context CRC changed;
 * every write erases a 2 KB page. FCntUp changes of the uplinks do not set @c nvm_dirty.
 *
 * Not possible in TEST mode: NvmDataMgmtStoreEnd() would restart LoRaMac (LoRaMacStart);
 * @c nvm_dirty stays set.
 *
 * @param force 1 = store even without a pending change, 0 = only when @c nvm_dirty is set.
 * @retval LW_OK         Stored, or nothing to store.
 * @retval LW_ERROR      TEST mode, or the store failed.
 * @retval LW_BUSY_ERROR LoRaMac busy (TX / RX windows in progress).
 */
LW_Status_t lw_nvm_store(uint8_t force);

/**
 * @brief Sets the working mode (AT+MODE).
 *
 * - 0 = LWABP: from TEST stops the radio test and reinitialises LoRaMac (LoRaWAN_Reconfigure()),
 *   then activates ABP (LoRaWAN_StartAbp()) when not activated yet. With a restored ABP session
 *   nothing is activated again.
 * - 1 = TEST: stops LoRaMac (LmHandlerHalt(), LoRaMacStop()), the radio is free for the RF tests.
 *   LmHandlerHalt() also refreshes the RAM backup of the NVM context.
 *
 * The first ABP activation goes through MLME_JOIN: LoRaMac is busy for a few ms afterwards.
 *
 * @param mode 0 = LWABP, 1 = TEST.
 * @retval LW_OK         Success (also when the mode does not change).
 * @retval LW_ERROR      Unknown mode.
 * @retval LW_BUSY_ERROR LWABP -> TEST while LoRaMac is busy.
 */
LW_Status_t lw_mode_set(uint8_t mode);

/**
 * @brief Gets the working mode (AT+MODE) from the stack state.
 *
 * TEST mode = LoRaMac stopped (LoRaMacIsStopped()). LoRaMac runs in every LWABP state, also
 * when not activated: LoRaWAN_Init() starts it after LmHandlerConfigure(), lw_mode_set(0)
 * starts it through LoRaWAN_StartAbp().
 *
 * @retval 0 LWABP.
 * @retval 1 TEST.
 */
uint8_t lw_mode_get(void);

/**
 * @brief Ends the ABP session in RAM (MIB_NETWORK_ACTIVATION = NONE).
 *
 * LmHandlerJoinStatus() then returns RESET, so the LmHandler setters refused after the activation
 * work again (LmHandlerSetKey, LmHandlerSetDevAddr, LmHandlerSetNetworkID, LmHandlerSetActiveRegion).
 * Activate again with lw_mode_set(0).
 *
 * Kept: keys, DevAddr, FCnt, channels, DR, TX power (the whole NVM context, only the activation changes).
 *
 * While deactivated:
 * - uplinks are refused (#LW_NOT_JOINED),
 * - LmHandlerIsBusy() would start a join by itself (not called by this application),
 * - an NVM store (lw_nvm_store(), dev_reset()) saves NONE: the module then boots not activated.
 *   @c nvm_dirty is not set here, but a store of other pending changes saves it too.
 *
 * @retval LW_OK         Deactivated, or not activated (nothing to do).
 * @retval LW_ERROR      TEST mode, or the MIB request was refused.
 * @retval LW_BUSY_ERROR LoRaMac busy.
 */
LW_Status_t lw_abp_deactivate(void);

#endif /* LORAWAN_APP_LW_CORE_H_ */
