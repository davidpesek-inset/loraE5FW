/*
 * at_ops.h - AT command layer operations interface
 * Pure C99, no HAL dependencies
 */
#ifndef AT_OPS_H
#define AT_OPS_H

#include <stdint.h>
#include <stdbool.h>

/* Operations structure: function pointers for LoRa stack callbacks */
typedef struct {
    int (*set_log)(bool debug);                  /* Set debug/quiet mode */
    int (*set_uart_timeout_ms)(uint32_t ms);    /* Set UART timeout */
    int (*set_baud)(uint32_t baud);             /* Set baud rate */
    int (*set_wdt)(bool enable);                /* Enable/disable watchdog */
    int (*reset)(void);                         /* System reset */
    int (*set_mode_lwabp)(void);                /* Set LoRaWAN mode */
    int (*set_public_network)(bool enable);     /* Public/private network */
    int (*set_dr)(uint8_t dr);                  /* Set data rate (0-5) */
    int (*set_channel)(uint8_t idx, uint32_t freq_hz); /* Set channel frequency */
    int (*set_class_a)(void);                   /* Set Class A */
    int (*set_adr)(bool enable);                /* Adaptive data rate */
    int (*set_tx_power_dbm)(int8_t power);      /* TX power in dBm */
    int (*set_dc)(bool enable);                 /* Duty cycle */
    int (*set_dumrx)(bool enable);              /* Dump RX */
    int (*set_ldro)(bool enable);               /* Low data rate optimization */
    int (*set_thld)(int16_t val);               /* Threshold */
    int (*set_port)(uint8_t port);              /* Application port */
    int (*set_rx_delay_ms)(uint8_t rx, uint32_t ms); /* RX delay (rx=1 or 2) */
    int (*set_retry)(uint8_t n);                /* Retry count */
    int (*set_rept)(uint8_t n);                 /* Repeat count */
    int (*set_nwkskey)(const uint8_t key[16]);  /* Network session key */
    int (*set_appskey)(const uint8_t key[16]);  /* Application session key */
    int (*set_devaddr)(uint32_t addr);          /* Device address */
    int (*get_counters)(uint32_t *ul, uint32_t *dl); /* Get UL/DL counters */
    int (*set_counters)(uint32_t ul, uint32_t dl);  /* Set UL/DL counters */
    int (*send_msg)(const uint8_t *data, uint8_t len, bool confirmed); /* Send message */
    void (*rx_timer_restart)(uint32_t ms);      /* Start/restart one-shot RX timer (NULL = inactive) */
    void (*rx_timer_stop)(void);                /* Stop RX timer (NULL = inactive) */
} at_ops_t;

#endif /* AT_OPS_H */
