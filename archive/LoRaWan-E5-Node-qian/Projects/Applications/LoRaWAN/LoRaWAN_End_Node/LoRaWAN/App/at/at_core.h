/*
 * at_core.h - AT command core processor
 * Line-based command queue and dispatcher
 */
#ifndef AT_CORE_H
#define AT_CORE_H

#include <stdint.h>
#include <stdbool.h>
#include "at_ops.h"

/*
 * Initialize AT processor
 * ops: operation callbacks (may contain NULLs for unimplemented ops)
 * write: function to write response strings (called with response text and length)
 */
void at_init(const at_ops_t *ops, void (*write)(const char *str, uint16_t len));

/*
 * Feed input byte from UART
 * Skips 0x00 and 0xFF bytes
 * Queues complete lines (ending with \n or \r) for processing
 * Called from interrupt or input task
 */
void at_rx_byte(uint8_t b);

/*
 * Process one queued command line (if any)
 * Must be called from main loop, never from interrupt
 * Executes handlers and writes responses
 */
void at_process(void);

/*
 * Notify completion of async message send
 * Called by higher layer when MSGHEX/CMSGHEX send completes
 * success: true for success, false for error
 * Outputs "+MSGHEX: Done" or "+MSGHEX: ERROR" etc.
 */
void at_msg_done(bool success);

/*
 * Abort async message send (called on send_msg error)
 * Clears busy state without printing anything
 */
void at_msg_abort(void);

/*
 * Set inter-character receive timeout (ms). 0 = disabled. Default 1000.
 */
void at_set_rx_timeout(uint32_t ms);

/*
 * Called by higher layer when the RX timer expires.
 * Discards a partial line and sets timeout_pending; at_process() then
 * writes "+AT: ERROR(-4)". Only sets flags; never calls handlers or writes.
 */
void at_rx_timeout(void);

/*
 * Internal functions used by at_cmds
 */
void at_msg_start(const char *prefix);
bool at_is_msg_sending(void);

#endif /* AT_CORE_H */
