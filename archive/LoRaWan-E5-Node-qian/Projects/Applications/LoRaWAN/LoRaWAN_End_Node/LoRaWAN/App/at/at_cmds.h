/*
 * at_cmds.h - AT command table and handlers
 */
#ifndef AT_CMDS_H
#define AT_CMDS_H

#include "at_ops.h"
#include <stdint.h>

/* Internal state for AT commands */
typedef struct {
    bool log_debug;
    bool wdt;
    uint32_t baud;
    uint32_t uart_timeout_ms;
    bool public_network;
    bool adr;
    bool lw_dc;
    bool lw_ct;
    bool lw_ldro;
    bool lw_dumrx;
    int16_t lw_thld;
    uint8_t tx_power;
    uint8_t retry;
    uint8_t rept;
    uint8_t port;
} at_state_t;

/*
 * Dispatch a command line to appropriate handler
 * Returns 0 on success (or handler specific result)
 */
int at_cmd_dispatch(const at_ops_t *ops, const char *line,
                    void (*write)(const char *, uint16_t));

#endif /* AT_CMDS_H */
