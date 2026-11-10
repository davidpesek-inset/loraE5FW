/**
 * @file    demo_funcs.h
 * @author  pesek
 * @date    10. 11. 2026
 * @brief   Demo and debug helper functions.
 */

#ifndef LORAWAN_APP_DEMO_FUNCS_H_
#define LORAWAN_APP_DEMO_FUNCS_H_

/**
 * @brief Prints the active configuration at log level VLEVEL_H (visible with AT+LOG=DEBUG).
 *
 * Values are read where they take effect: LoRaMac NVM context (stack parameters, channels, FCnt),
 * secure element (keys), UART / IWDG handles, trace module. Settings the application reads
 * directly from eeprom_vars / ram_vars (port, retry, THLD, CT, DUMRX, low power) come from there.
 * Prints the session keys in clear. Waits while the trace FIFO is full (about 2 s at 9600 Bd).
 */
void print_config(void);

#endif /* LORAWAN_APP_DEMO_FUNCS_H_ */
