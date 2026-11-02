/*
 * lw_options.h
 *
 *  Created on: 30. 10. 2026
 *      Author: pesek
 *
 *  Seeed behaviour options (AT+LW=CT, DUMRX, THLD), read by the stack through the weak hooks
 *  defined in lw_options.c.
 */

#ifndef LORAWAN_APP_LW_OPTIONS_H_
#define LORAWAN_APP_LW_OPTIONS_H_

#include "lw_types.h"

LW_Status_t lw_ct_set(bool ct);
LW_Status_t lw_dumrx_set(bool dumrx);
LW_Status_t lw_thld_set(int16_t thld);

#endif /* LORAWAN_APP_LW_OPTIONS_H_ */
