/*
 * at_cmds.c - AT command handlers and dispatcher
 * Implements all commands from spec section 3
 */

#include "at_cmds.h"
#include "at_core.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

/* Global state */
static at_state_t g_state = {
    .log_debug = false,
    .wdt = false,
    .baud = 115200,
    .uart_timeout_ms = 0,
    .public_network = true,  /* Default ON per spec */
    .adr = false,
    .lw_dc = false,
    .lw_ct = false,
    .lw_ldro = false,
    .lw_dumrx = false,
    .lw_thld = 0,
    .tx_power = 14,
    .retry = 0,
    .rept = 0,
    .port = 10,
};

/* ============= Helper Functions ============= */

static void write_str(void (*write)(const char *, uint16_t), const char *str) {
    write(str, strlen(str));
}

/* Case-insensitive string compare (up to n chars) */
static int strcasecmp_n(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        unsigned char ca = tolower((unsigned char)a[i]);
        unsigned char cb = tolower((unsigned char)b[i]);
        if (ca != cb) return (int)ca - (int)cb;
        if (!a[i]) break;
    }
    return 0;
}

/* Case-insensitive string compare */
static int strcasecmp_full(const char *a, const char *b) {
    while (*a || *b) {
        unsigned char ca = tolower((unsigned char)*a);
        unsigned char cb = tolower((unsigned char)*b);
        if (ca != cb) return (int)ca - (int)cb;
        a++;
        b++;
    }
    return 0;
}

/* Parse unsigned integer, return 0 on success, -1 on error */
static int parse_uint(const char *str, uint32_t *out) {
    if (!str || !*str) return -1;
    uint32_t val = 0;
    for (const char *p = str; *p; p++) {
        if (*p < '0' || *p > '9') return -1;
        uint32_t digit = *p - '0';
        if (val > UINT32_MAX / 10 || (val == UINT32_MAX / 10 && digit > UINT32_MAX % 10)) {
            return -1;  /* Overflow */
        }
        val = val * 10 + digit;
    }
    *out = val;
    return 0;
}

/* Parse signed integer, return 0 on success, -1 on error */
static int parse_int(const char *str, int32_t *out) {
    if (!str || !*str) return -1;
    bool negative = false;
    const char *p = str;
    if (*p == '-') {
        negative = true;
        p++;
        if (!*p) return -1;  /* Just "-" */
    }
    uint32_t val = 0;
    for (; *p; p++) {
        if (*p < '0' || *p > '9') return -1;
        uint32_t digit = *p - '0';
        if (val > (uint32_t)INT32_MAX / 10) return -1;  /* Overflow */
        val = val * 10 + digit;
    }
    if (negative) {
        if (val > (uint32_t)INT32_MAX + 1) return -1;  /* Overflow */
        *out = -(int32_t)val;
    } else {
        if (val > (uint32_t)INT32_MAX) return -1;  /* Overflow */
        *out = (int32_t)val;
    }
    return 0;
}

/* Parse hex digit */
static int hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Parse hex string to bytes, return byte count or -1 on error */
static int parse_hex(const char *str, uint8_t *out, int max_len) {
    int len = strlen(str);
    if (len % 2 != 0) return -1;  /* Odd number of chars */
    if (len / 2 > max_len) return -1;  /* Too long */

    for (int i = 0; i < len; i += 2) {
        int hi = hex_digit(str[i]);
        int lo = hex_digit(str[i + 1]);
        if (hi < 0 || lo < 0) return -1;
        out[i / 2] = (hi << 4) | lo;
    }
    return len / 2;
}

/* Convert bytes to uppercase hex string */
static void bytes_to_hex(const uint8_t *data, int len, char *out) {
    static const char *hex = "0123456789ABCDEF";
    for (int i = 0; i < len; i++) {
        out[2*i] = hex[(data[i] >> 4) & 0xF];
        out[2*i + 1] = hex[data[i] & 0xF];
    }
    out[2*len] = '\0';
}

/* Parse decimal frequency string (e.g., "863.8") to Hz (863800000) */
static uint32_t parse_freq_mhz(const char *str) {
    uint32_t hz = 0;
    int frac_digits = 0;
    bool in_frac = false;

    for (const char *p = str; *p; p++) {
        if (*p == '.') {
            if (in_frac) return 0;  /* Multiple dots */
            in_frac = true;
            continue;
        }
        if (*p < '0' || *p > '9') return 0;  /* Invalid char */

        if (!in_frac) {
            hz = hz * 10 + (*p - '0');
        } else {
            hz = hz * 10 + (*p - '0');
            frac_digits++;
        }
    }

    /* Convert MHz to Hz: multiply by 1,000,000
       If fractional: multiply by 10^(6-frac_digits)
       If not fractional: multiply by 10^6 */
    if (in_frac) {
        for (int i = frac_digits; i < 6; i++) {
            hz *= 10;
        }
    } else {
        for (int i = 0; i < 6; i++) {
            hz *= 10;
        }
    }

    return hz;
}

/* Integer to string */
static int int_to_str(int val, char *buf, int max_len) {
    if (max_len <= 0) return 0;

    char temp[16];
    int len = 0;
    bool negative = val < 0;

    if (negative) val = -val;

    if (val == 0) {
        buf[0] = '0';
        return 1;
    }

    while (val > 0 && len < 15) {
        temp[len++] = '0' + (val % 10);
        val /= 10;
    }

    int out_len = 0;
    if (negative && out_len < max_len) {
        buf[out_len++] = '-';
    }

    for (int i = len - 1; i >= 0 && out_len < max_len; i--) {
        buf[out_len++] = temp[i];
    }

    return out_len;
}

/* Unsigned integer to string */
static int uint_to_str(uint32_t val, char *buf, int max_len) {
    if (max_len <= 0) return 0;

    char temp[16];
    int len = 0;

    if (val == 0) {
        buf[0] = '0';
        return 1;
    }

    while (val > 0 && len < 15) {
        temp[len++] = '0' + (val % 10);
        val /= 10;
    }

    int out_len = 0;
    for (int i = len - 1; i >= 0 && out_len < max_len; i--) {
        buf[out_len++] = temp[i];
    }

    return out_len;
}

/* Parse argument from "arg1,arg2" string, handling quotes and trimming */
static const char *get_arg(const char *args, int arg_num, char *out, int max_len) {
    int arg_idx = 0;
    int out_idx = 0;
    bool in_quotes = false;

    for (const char *p = args; *p && out_idx < max_len - 1; p++) {
        if (arg_idx == arg_num) {
            if (*p == '"') {
                in_quotes = !in_quotes;
                continue;
            }
            if (*p == ',' && !in_quotes) break;
            if (*p == ' ' && !in_quotes && out_idx == 0) continue;  /* Skip leading space */
            out[out_idx++] = *p;
            continue;
        }

        if (*p == ',') {
            arg_idx++;
            in_quotes = false;
        }
    }

    /* Trim trailing spaces */
    while (out_idx > 0 && out[out_idx - 1] == ' ') {
        out_idx--;
    }

    out[out_idx] = '\0';
    return out_idx > 0 ? out : NULL;
}

/* Parse argument with truncation detection
   Returns: NULL if arg not found, pointer to out otherwise
   Sets truncated=true if arg was longer than max_len-1 */
static const char *get_arg_ex(const char *args, int arg_num, char *out, int max_len, bool *truncated) {
    if (truncated) *truncated = false;
    int arg_idx = 0;
    int out_idx = 0;
    bool in_quotes = false;
    bool saw_end = false;

    for (const char *p = args; *p; p++) {
        if (arg_idx == arg_num) {
            if (*p == '"') {
                in_quotes = !in_quotes;
                continue;
            }
            if (*p == ',' && !in_quotes) {
                saw_end = true;
                break;
            }
            if (*p == ' ' && !in_quotes && out_idx == 0) continue;
            if (out_idx < max_len - 1) {
                out[out_idx++] = *p;
            } else if (truncated) {
                *truncated = true;
            }
            continue;
        }

        if (*p == ',') {
            arg_idx++;
            in_quotes = false;
        }
    }

    if (arg_idx == arg_num && out_idx >= 0) {
        saw_end = true;
    }

    /* Trim trailing spaces */
    while (out_idx > 0 && out[out_idx - 1] == ' ') {
        out_idx--;
    }

    out[out_idx] = '\0';
    return (saw_end && out_idx > 0) ? out : NULL;
}

/* ============= Command Handlers ============= */

static void cmd_at(const at_ops_t *ops, const char *args,
                   void (*write)(const char *, uint16_t)) {
    (void)ops; (void)args;
    write_str(write, "+AT: OK\r\n");
}

static void cmd_log(const at_ops_t *ops, const char *args,
                    void (*write)(const char *, uint16_t)) {
    (void)ops;
    char arg[16];
    if (!get_arg(args, 0, arg, sizeof(arg))) {
        write_str(write, "+AT: ERROR(-2)\r\n");
        return;
    }

    if (strcasecmp_full(arg, "DEBUG") == 0) {
        g_state.log_debug = true;
        write_str(write, "+LOG: DEBUG\r\n");
    } else if (strcasecmp_full(arg, "QUIET") == 0) {
        g_state.log_debug = false;
        write_str(write, "+LOG: QUIET\r\n");
    } else {
        write_str(write, "+AT: ERROR(-2)\r\n");
    }
}

static void cmd_uart(const at_ops_t *ops, const char *args,
                     void (*write)(const char *, uint16_t)) {
    char subkey[16], val_str[16];
    if (!get_arg(args, 0, subkey, sizeof(subkey))) {
        write_str(write, "+AT: ERROR(-2)\r\n");
        return;
    }
    if (!get_arg(args, 1, val_str, sizeof(val_str))) {
        write_str(write, "+AT: ERROR(-2)\r\n");
        return;
    }

    uint32_t val = 0;
    if (parse_uint(val_str, &val) != 0) {
        write_str(write, "+AT: ERROR(-2)\r\n");
        return;
    }

    if (strcasecmp_full(subkey, "TIMEOUT") == 0) {
        /* Inter-character RX timeout, 0..65535 ms; validate before any state change */
        if (val > 65535) {
            write_str(write, "+AT: ERROR(-2)\r\n");
            return;
        }
        at_set_rx_timeout(val);
        if (ops && ops->set_uart_timeout_ms) {
            ops->set_uart_timeout_ms(val);
        }
        g_state.uart_timeout_ms = val;
        char buf[64];
        int len = 0;
        memcpy(buf + len, "+UART: TIMEOUT, ", 16); len += 16;
        len += uint_to_str(val, buf + len, 64 - len);
        buf[len++] = '\r'; buf[len++] = '\n';
        write(buf, len);
    } else if (strcasecmp_full(subkey, "BR") == 0) {
        char buf[64];
        int len = 0;
        memcpy(buf + len, "+UART: BR, ", 11); len += 11;
        len += uint_to_str(val, buf + len, 64 - len);
        buf[len++] = '\r'; buf[len++] = '\n';
        write(buf, len);
        /* Call ops AFTER sending response */
        if (ops && ops->set_baud) {
            ops->set_baud(val);
        }
        g_state.baud = val;
    } else {
        write_str(write, "+AT: ERROR(-2)\r\n");
    }
}

static void cmd_wdt(const at_ops_t *ops, const char *args,
                    void (*write)(const char *, uint16_t)) {
    char arg[16];

    if (!get_arg(args, 0, arg, sizeof(arg))) {
        /* No argument: query */
        const char *status = g_state.wdt ? "+WDT: ON\r\n" : "+WDT: OFF\r\n";
        write_str(write, status);
    } else if (strcasecmp_full(arg, "ON") == 0) {
        if (ops && ops->set_wdt) {
            ops->set_wdt(true);
        }
        g_state.wdt = true;
        write_str(write, "+WDT: ON\r\n");
    } else if (strcasecmp_full(arg, "OFF") == 0) {
        if (ops && ops->set_wdt) {
            ops->set_wdt(false);
        }
        g_state.wdt = false;
        write_str(write, "+WDT: OFF\r\n");
    } else {
        write_str(write, "+AT: ERROR(-2)\r\n");
    }
}

static void cmd_reset(const at_ops_t *ops, const char *args,
                      void (*write)(const char *, uint16_t)) {
    (void)args;
    write_str(write, "+RESET: OK\r\n");
    if (ops && ops->reset) {
        ops->reset();
    }
}

static void cmd_mode(const at_ops_t *ops, const char *args,
                     void (*write)(const char *, uint16_t)) {
    char arg[16];
    if (!get_arg(args, 0, arg, sizeof(arg))) {
        write_str(write, "+AT: ERROR(-2)\r\n");
        return;
    }

    if (strcasecmp_full(arg, "LWABP") == 0) {
        if (ops && ops->set_mode_lwabp) {
            ops->set_mode_lwabp();
        }
        write_str(write, "+MODE: LWABP\r\n");
    } else {
        write_str(write, "+AT: ERROR(-2)\r\n");
    }
}

static void cmd_lowpower(const at_ops_t *ops, const char *args,
                         void (*write)(const char *, uint16_t)) {
    (void)ops;
    char subkey[16];

    if (!get_arg(args, 0, subkey, sizeof(subkey))) {
        /* Query: just return SLEEP */
        write_str(write, "+LOWPOWER: SLEEP\r\n");
        return;
    }

    if (strcasecmp_full(subkey, "AUTOOFF") == 0) {
        write_str(write, "+LOWPOWER: AUTOOFF\r\n");
    } else if (strcasecmp_full(subkey, "AUTOON") == 0) {
        write_str(write, "+LOWPOWER: AUTOON\r\n");
    } else {
        write_str(write, "+AT: ERROR(-2)\r\n");
    }
}

static void cmd_lw(const at_ops_t *ops, const char *args,
                   void (*write)(const char *, uint16_t)) {
    char subkey[16];
    if (!get_arg(args, 0, subkey, sizeof(subkey))) {
        write_str(write, "+AT: ERROR(-2)\r\n");
        return;
    }

    if (strcasecmp_full(subkey, "NET") == 0) {
        char arg2[16];
        if (!get_arg(args, 1, arg2, sizeof(arg2))) {
            /* Query */
            const char *status = g_state.public_network ? "+LW: NET, ON\r\n" : "+LW: NET, OFF\r\n";
            write_str(write, status);
        } else if (strcasecmp_full(arg2, "OFF") == 0) {
            if (ops && ops->set_public_network) {
                ops->set_public_network(false);
            }
            g_state.public_network = false;
            write_str(write, "+LW: NET, OFF\r\n");
        } else if (strcasecmp_full(arg2, "ON") == 0) {
            if (ops && ops->set_public_network) {
                ops->set_public_network(true);
            }
            g_state.public_network = true;
            write_str(write, "+LW: NET, ON\r\n");
        } else {
            write_str(write, "+AT: ERROR(-2)\r\n");
        }
    } else if (strcasecmp_full(subkey, "DC") == 0) {
        char arg2[16];
        if (!get_arg(args, 1, arg2, sizeof(arg2))) {
            write_str(write, "+AT: ERROR(-2)\r\n");
            return;
        }
        if (strcasecmp_full(arg2, "OFF") == 0) {
            if (ops && ops->set_dc) {
                ops->set_dc(false);
            }
            g_state.lw_dc = false;
            write_str(write, "+LW: DC, OFF, 0\r\n");
        } else {
            write_str(write, "+AT: ERROR(-2)\r\n");
        }
    } else if (strcasecmp_full(subkey, "CT") == 0) {
        char arg2[16];
        if (!get_arg(args, 1, arg2, sizeof(arg2))) {
            write_str(write, "+AT: ERROR(-2)\r\n");
            return;
        }
        if (strcasecmp_full(arg2, "OFF") == 0) {
            g_state.lw_ct = false;
            write_str(write, "+LW: CT, OFF\r\n");
        } else {
            write_str(write, "+AT: ERROR(-2)\r\n");
        }
    } else if (strcasecmp_full(subkey, "LDRO") == 0) {
        char arg2[16];
        if (!get_arg(args, 1, arg2, sizeof(arg2))) {
            write_str(write, "+AT: ERROR(-2)\r\n");
            return;
        }
        if (strcasecmp_full(arg2, "OFF") == 0) {
            if (ops && ops->set_ldro) {
                ops->set_ldro(false);
            }
            g_state.lw_ldro = false;
            write_str(write, "+LW: LDRO, OFF\r\n");
        } else {
            write_str(write, "+AT: ERROR(-2)\r\n");
        }
    } else if (strcasecmp_full(subkey, "THLD") == 0) {
        char arg2[16];
        if (!get_arg(args, 1, arg2, sizeof(arg2))) {
            write_str(write, "+AT: ERROR(-2)\r\n");
            return;
        }
        int32_t val = 0;
        if (parse_int(arg2, &val) != 0) {
            write_str(write, "+AT: ERROR(-2)\r\n");
            return;
        }
        if (ops && ops->set_thld) {
            ops->set_thld((int16_t)val);
        }
        g_state.lw_thld = (int16_t)val;
        char buf[64];
        int len = 0;
        memcpy(buf + len, "+LW: THLD, ", 11); len += 11;
        len += int_to_str((int)val, buf + len, 64 - len);
        buf[len++] = '\r'; buf[len++] = '\n';
        write(buf, len);
    } else if (strcasecmp_full(subkey, "DUMRX") == 0) {
        char arg2[16];
        if (!get_arg(args, 1, arg2, sizeof(arg2))) {
            write_str(write, "+AT: ERROR(-2)\r\n");
            return;
        }
        if (strcasecmp_full(arg2, "ON") == 0) {
            if (ops && ops->set_dumrx) {
                ops->set_dumrx(true);
            }
            g_state.lw_dumrx = true;
            write_str(write, "+LW: DUMRX, ON\r\n");
        } else if (strcasecmp_full(arg2, "OFF") == 0) {
            if (ops && ops->set_dumrx) {
                ops->set_dumrx(false);
            }
            g_state.lw_dumrx = false;
            write_str(write, "+LW: DUMRX, OFF\r\n");
        } else {
            write_str(write, "+AT: ERROR(-2)\r\n");
        }
    } else if (strcasecmp_full(subkey, "ULDL") == 0) {
        char arg2[32], arg3[32];
        if (!get_arg(args, 1, arg2, sizeof(arg2))) {
            /* Query: get counters */
            if (ops && ops->get_counters) {
                uint32_t ul = 0, dl = 0;
                ops->get_counters(&ul, &dl);
                char buf[128];
                int len = 0;
                memcpy(buf + len, "+LW: ULDL ", 10); len += 10;
                len += uint_to_str(ul, buf + len, 128 - len);
                buf[len++] = ','; buf[len++] = ' ';
                len += uint_to_str(dl, buf + len, 128 - len);
                buf[len++] = '\r'; buf[len++] = '\n';
                write(buf, len);
            } else {
                write_str(write, "+AT: ERROR(-1)\r\n");
            }
        } else if (get_arg(args, 2, arg3, sizeof(arg3))) {
            /* Set: AT+LW=ULDL,<ul>,<dl> */
            uint32_t ul = 0, dl = 0;
            if (parse_uint(arg2, &ul) != 0 || parse_uint(arg3, &dl) != 0) {
                write_str(write, "+AT: ERROR(-2)\r\n");
                return;
            }
            if (ops && ops->set_counters) {
                ops->set_counters(ul, dl);
            }
            char buf[128];
            int len = 0;
            memcpy(buf + len, "+LW: ULDL, ", 11); len += 11;
            len += uint_to_str(ul, buf + len, 128 - len);
            buf[len++] = ',';
            len += uint_to_str(dl, buf + len, 128 - len);
            buf[len++] = '\r'; buf[len++] = '\n';
            write(buf, len);
        } else {
            write_str(write, "+AT: ERROR(-2)\r\n");
        }
    } else {
        write_str(write, "+AT: ERROR(-2)\r\n");
    }
}

static void cmd_dr(const at_ops_t *ops, const char *args,
                   void (*write)(const char *, uint16_t)) {
    char arg[16];
    if (!get_arg(args, 0, arg, sizeof(arg))) {
        write_str(write, "+AT: ERROR(-2)\r\n");
        return;
    }

    if (strcasecmp_full(arg, "EU868") == 0) {
        write_str(write, "+DR: EU868\r\n");
    } else {
        uint32_t dr_val = 0;
        if (parse_uint(arg, &dr_val) != 0 || dr_val > 5) {
            write_str(write, "+AT: ERROR(-2)\r\n");
            return;
        }
        int dr = (int)dr_val;

        if (ops && ops->set_dr) {
            ops->set_dr((uint8_t)dr);
        }

        /* Format response according to spec spacing rules */
        /* SF = 12 - DR, so: DR0->SF12, DR1->SF11, DR2->SF10, DR3->SF9, DR4->SF8, DR5->SF7 */
        int sf = 12 - dr;

        char buf[64];
        int len = 0;
        memcpy(buf + len, "+DR: EU868 DR", 13); len += 13;
        buf[len++] = '0' + dr;
        buf[len++] = ' '; buf[len++] = ' ';  /* Two spaces after DRn */
        memcpy(buf + len, "SF", 2); len += 2;
        len += uint_to_str((uint32_t)sf, buf + len, 64 - len);

        /* Space rule: for DR > 2 (SF < 10), use two spaces before BW; else one space */
        if (sf < 10) {
            buf[len++] = ' '; buf[len++] = ' ';
        } else {
            buf[len++] = ' ';
        }

        memcpy(buf + len, "BW125K", 6); len += 6;
        buf[len++] = '\r'; buf[len++] = '\n';
        write(buf, len);
    }
}

static void cmd_ch(const at_ops_t *ops, const char *args,
                   void (*write)(const char *, uint16_t)) {
    char idx_str[16], mhz_str[16], dr0_str[16], dr5_str[16];

    if (!get_arg(args, 0, idx_str, sizeof(idx_str)) ||
        !get_arg(args, 1, mhz_str, sizeof(mhz_str)) ||
        !get_arg(args, 2, dr0_str, sizeof(dr0_str)) ||
        !get_arg(args, 3, dr5_str, sizeof(dr5_str))) {
        write_str(write, "+AT: ERROR(-2)\r\n");
        return;
    }

    uint32_t idx_val = 0;
    if (parse_uint(idx_str, &idx_val) != 0 || idx_val > 15) {
        write_str(write, "+AT: ERROR(-2)\r\n");
        return;
    }
    uint8_t idx = (uint8_t)idx_val;

    if (strcasecmp_full(dr0_str, "DR0") != 0 || strcasecmp_full(dr5_str, "DR5") != 0) {
        write_str(write, "+AT: ERROR(-2)\r\n");
        return;
    }

    uint32_t hz = parse_freq_mhz(mhz_str);
    if (hz == 0) {
        write_str(write, "+AT: ERROR(-2)\r\n");
        return;
    }

    if (ops && ops->set_channel) {
        ops->set_channel(idx, hz);
    }

    /* Response: +CH: <idx>,<Hz>,DR0:DR5 */
    char buf[128];
    int len = 0;
    buf[len++] = '+'; buf[len++] = 'C'; buf[len++] = 'H'; buf[len++] = ':';
    buf[len++] = ' ';
    len += uint_to_str((uint32_t)idx, buf + len, 128 - len);
    buf[len++] = ',';
    len += uint_to_str(hz, buf + len, 128 - len);
    buf[len++] = ','; buf[len++] = 'D'; buf[len++] = 'R';
    buf[len++] = '0'; buf[len++] = ':'; buf[len++] = 'D'; buf[len++] = 'R';
    buf[len++] = '5';
    buf[len++] = '\r'; buf[len++] = '\n';
    write(buf, len);
}

static void cmd_class(const at_ops_t *ops, const char *args,
                      void (*write)(const char *, uint16_t)) {
    char arg[16];
    if (!get_arg(args, 0, arg, sizeof(arg))) {
        write_str(write, "+AT: ERROR(-2)\r\n");
        return;
    }

    if (strcasecmp_full(arg, "A") == 0) {
        if (ops && ops->set_class_a) {
            ops->set_class_a();
        }
        write_str(write, "+CLASS: A\r\n");
    } else {
        write_str(write, "+AT: ERROR(-2)\r\n");
    }
}

static void cmd_adr(const at_ops_t *ops, const char *args,
                    void (*write)(const char *, uint16_t)) {
    char arg[16];
    if (!get_arg(args, 0, arg, sizeof(arg))) {
        write_str(write, "+AT: ERROR(-2)\r\n");
        return;
    }

    if (strcasecmp_full(arg, "ON") == 0) {
        if (ops && ops->set_adr) {
            ops->set_adr(true);
        }
        g_state.adr = true;
        write_str(write, "+ADR: ON\r\n");
    } else if (strcasecmp_full(arg, "OFF") == 0) {
        if (ops && ops->set_adr) {
            ops->set_adr(false);
        }
        g_state.adr = false;
        write_str(write, "+ADR: OFF\r\n");
    } else {
        write_str(write, "+AT: ERROR(-2)\r\n");
    }
}

static void cmd_power(const at_ops_t *ops, const char *args,
                      void (*write)(const char *, uint16_t)) {
    char power_str[16], force_str[16];
    if (!get_arg(args, 0, power_str, sizeof(power_str)) ||
        !get_arg(args, 1, force_str, sizeof(force_str))) {
        write_str(write, "+AT: ERROR(-2)\r\n");
        return;
    }

    int32_t power = 0;
    if (parse_int(power_str, &power) != 0 || power < -10 || power > 30) {
        write_str(write, "+AT: ERROR(-2)\r\n");
        return;
    }
    if (strcasecmp_full(force_str, "FORCE") != 0) {
        write_str(write, "+AT: ERROR(-2)\r\n");
        return;
    }

    if (ops && ops->set_tx_power_dbm) {
        ops->set_tx_power_dbm((int8_t)power);
    }
    g_state.tx_power = (uint8_t)power;

    char buf[64];
    int len = 0;
    memcpy(buf + len, "+POWER: ", 8); len += 8;
    len += int_to_str(power, buf + len, 64 - len);
    buf[len++] = ','; buf[len++] = ' ';
    memcpy(buf + len, "FORCE", 5); len += 5;
    buf[len++] = '\r'; buf[len++] = '\n';
    write(buf, len);
}

static void cmd_port(const at_ops_t *ops, const char *args,
                     void (*write)(const char *, uint16_t)) {
    char arg[16];
    if (!get_arg(args, 0, arg, sizeof(arg))) {
        write_str(write, "+AT: ERROR(-2)\r\n");
        return;
    }

    uint32_t port_val = 0;
    if (parse_uint(arg, &port_val) != 0 || port_val < 1 || port_val > 223) {
        write_str(write, "+AT: ERROR(-2)\r\n");
        return;
    }
    int port = (int)port_val;

    if (ops && ops->set_port) {
        ops->set_port((uint8_t)port);
    }
    g_state.port = (uint8_t)port;

    char buf[64];
    int len = 0;
    memcpy(buf + len, "+PORT: ", 7); len += 7;
    len += uint_to_str((uint32_t)port, buf + len, 64 - len);
    buf[len++] = '\r'; buf[len++] = '\n';
    write(buf, len);
}

static void cmd_delay(const at_ops_t *ops, const char *args,
                      void (*write)(const char *, uint16_t)) {
    char type_str[16], delay_str[16];
    if (!get_arg(args, 0, type_str, sizeof(type_str)) ||
        !get_arg(args, 1, delay_str, sizeof(delay_str))) {
        write_str(write, "+AT: ERROR(-2)\r\n");
        return;
    }

    uint8_t rx_type = 0;
    if (strcasecmp_full(type_str, "RX1") == 0) {
        rx_type = 1;
    } else if (strcasecmp_full(type_str, "RX2") == 0) {
        rx_type = 2;
    } else {
        write_str(write, "+AT: ERROR(-2)\r\n");
        return;
    }

    uint32_t delay_ms = 0;
    if (parse_uint(delay_str, &delay_ms) != 0 || delay_ms < 1 || delay_ms > 65535) {
        write_str(write, "+AT: ERROR(-2)\r\n");
        return;
    }
    if (ops && ops->set_rx_delay_ms) {
        ops->set_rx_delay_ms(rx_type, delay_ms);
    }

    char buf[64];
    int len = 0;
    memcpy(buf + len, "+DELAY: ", 8); len += 8;
    len += uint_to_str(delay_ms, buf + len, 64 - len);
    buf[len++] = '\r'; buf[len++] = '\n';
    write(buf, len);
}

static void cmd_rxwin1(const at_ops_t *ops, const char *args,
                       void (*write)(const char *, uint16_t)) {
    (void)ops;
    char a_str[16], b_str[16];
    if (!get_arg(args, 0, a_str, sizeof(a_str)) ||
        !get_arg(args, 1, b_str, sizeof(b_str))) {
        write_str(write, "+AT: ERROR(-2)\r\n");
        return;
    }

    char buf[64];
    int len = 0;
    memcpy(buf + len, "+RXWIN1: ", 9); len += 9;
    memcpy(buf + len, a_str, strlen(a_str)); len += strlen(a_str);
    buf[len++] = ',';
    memcpy(buf + len, b_str, strlen(b_str)); len += strlen(b_str);
    buf[len++] = '\r'; buf[len++] = '\n';
    write(buf, len);
}

static void cmd_retry(const at_ops_t *ops, const char *args,
                      void (*write)(const char *, uint16_t)) {
    char arg[16];
    if (!get_arg(args, 0, arg, sizeof(arg))) {
        write_str(write, "+AT: ERROR(-2)\r\n");
        return;
    }

    uint32_t retry_val = 0;
    if (parse_uint(arg, &retry_val) != 0 || retry_val > 15) {
        write_str(write, "+AT: ERROR(-2)\r\n");
        return;
    }
    uint8_t retry = (uint8_t)retry_val;
    if (ops && ops->set_retry) {
        ops->set_retry(retry);
    }
    g_state.retry = retry;

    char buf[64];
    int len = 0;
    memcpy(buf + len, "+RETRY: ", 8); len += 8;
    len += uint_to_str((uint32_t)retry, buf + len, 64 - len);
    buf[len++] = '\r'; buf[len++] = '\n';
    write(buf, len);
}

static void cmd_rept(const at_ops_t *ops, const char *args,
                     void (*write)(const char *, uint16_t)) {
    char arg[16];
    if (!get_arg(args, 0, arg, sizeof(arg))) {
        write_str(write, "+AT: ERROR(-2)\r\n");
        return;
    }

    uint32_t rept_val = 0;
    if (parse_uint(arg, &rept_val) != 0 || rept_val > 15) {
        write_str(write, "+AT: ERROR(-2)\r\n");
        return;
    }
    uint8_t rept = (uint8_t)rept_val;
    if (ops && ops->set_rept) {
        ops->set_rept(rept);
    }
    g_state.rept = rept;

    char buf[64];
    int len = 0;
    memcpy(buf + len, "+REPT: ", 7); len += 7;
    len += uint_to_str((uint32_t)rept, buf + len, 64 - len);
    buf[len++] = '\r'; buf[len++] = '\n';
    write(buf, len);
}

static void cmd_key(const at_ops_t *ops, const char *args,
                    void (*write)(const char *, uint16_t)) {
    char keytype[16], keyval[128];
    if (!get_arg(args, 0, keytype, sizeof(keytype)) ||
        !get_arg(args, 1, keyval, sizeof(keyval))) {
        write_str(write, "+AT: ERROR(-2)\r\n");
        return;
    }

    uint8_t key_bytes[16];
    int key_len = parse_hex(keyval, key_bytes, 16);
    if (key_len != 16) {
        write_str(write, "+AT: ERROR(-2)\r\n");
        return;
    }

    char hex_str[33];
    bytes_to_hex(key_bytes, 16, hex_str);

    if (strcasecmp_full(keytype, "NWKSKEY") == 0) {
        if (ops && ops->set_nwkskey) {
            ops->set_nwkskey(key_bytes);
        }
        char buf[96];
        int len = 0;
        memcpy(buf + len, "+KEY: NWKSKEY ", 14); len += 14;
        memcpy(buf + len, hex_str, 32); len += 32;
        buf[len++] = '\r'; buf[len++] = '\n';
        write(buf, len);
    } else if (strcasecmp_full(keytype, "APPSKEY") == 0) {
        if (ops && ops->set_appskey) {
            ops->set_appskey(key_bytes);
        }
        char buf[96];
        int len = 0;
        memcpy(buf + len, "+KEY: APPSKEY ", 14); len += 14;
        memcpy(buf + len, hex_str, 32); len += 32;
        buf[len++] = '\r'; buf[len++] = '\n';
        write(buf, len);
    } else {
        write_str(write, "+AT: ERROR(-2)\r\n");
    }
}

static void cmd_id(const at_ops_t *ops, const char *args,
                   void (*write)(const char *, uint16_t)) {
    char id_type[16], id_val[32];
    if (!get_arg(args, 0, id_type, sizeof(id_type)) ||
        !get_arg(args, 1, id_val, sizeof(id_val))) {
        write_str(write, "+AT: ERROR(-2)\r\n");
        return;
    }

    if (strcasecmp_full(id_type, "DevAddr") == 0) {
        uint8_t addr_bytes[4];
        int addr_len = parse_hex(id_val, addr_bytes, 4);
        if (addr_len != 4) {
            write_str(write, "+AT: ERROR(-2)\r\n");
            return;
        }

        uint32_t devaddr = ((uint32_t)addr_bytes[0] << 24) |
                           ((uint32_t)addr_bytes[1] << 16) |
                           ((uint32_t)addr_bytes[2] << 8) |
                           ((uint32_t)addr_bytes[3]);

        if (ops && ops->set_devaddr) {
            ops->set_devaddr(devaddr);
        }

        /* Response format: +ID: DevAddr, XX:XX:XX:XX */
        char buf[64];
        int len = 0;
        memcpy(buf + len, "+ID: DevAddr, ", 14); len += 14;

        static const char *hex = "0123456789ABCDEF";
        for (int i = 0; i < 4; i++) {
            if (i > 0) buf[len++] = ':';
            buf[len++] = hex[(addr_bytes[i] >> 4) & 0xF];
            buf[len++] = hex[addr_bytes[i] & 0xF];
        }

        buf[len++] = '\r'; buf[len++] = '\n';
        write(buf, len);
    } else {
        write_str(write, "+AT: ERROR(-2)\r\n");
    }
}

static void cmd_msghex(const at_ops_t *ops, const char *args,
                       void (*write)(const char *, uint16_t)) {
    if (at_is_msg_sending()) {
        write_str(write, "+AT: ERROR(-3)\r\n");
        return;
    }

    if (!ops || !ops->send_msg) {
        write_str(write, "+AT: ERROR(-1)\r\n");
        return;
    }

    char hexval[486];
    bool truncated = false;
    if (!get_arg_ex(args, 0, hexval, sizeof(hexval), &truncated)) {
        write_str(write, "+AT: ERROR(-2)\r\n");
        return;
    }
    if (truncated) {
        write_str(write, "+AT: ERROR(-2)\r\n");
        return;
    }

    uint8_t payload[242];
    int payload_len = parse_hex(hexval, payload, 242);
    if (payload_len < 0 || payload_len > 242) {
        write_str(write, "+AT: ERROR(-2)\r\n");
        return;
    }

    at_msg_start("+MSGHEX:");

    int result = ops->send_msg(payload, (uint8_t)payload_len, false);
    if (result != 0) {
        at_msg_abort();
        write_str(write, "+MSGHEX: ERROR\r\n");
    }
}

static void cmd_cmsghex(const at_ops_t *ops, const char *args,
                        void (*write)(const char *, uint16_t)) {
    if (at_is_msg_sending()) {
        write_str(write, "+AT: ERROR(-3)\r\n");
        return;
    }

    if (!ops || !ops->send_msg) {
        write_str(write, "+AT: ERROR(-1)\r\n");
        return;
    }

    char hexval[486];
    bool truncated = false;
    if (!get_arg_ex(args, 0, hexval, sizeof(hexval), &truncated)) {
        write_str(write, "+AT: ERROR(-2)\r\n");
        return;
    }
    if (truncated) {
        write_str(write, "+AT: ERROR(-2)\r\n");
        return;
    }

    uint8_t payload[242];
    int payload_len = parse_hex(hexval, payload, 242);
    if (payload_len < 0 || payload_len > 242) {
        write_str(write, "+AT: ERROR(-2)\r\n");
        return;
    }

    at_msg_start("+CMSGHEX:");

    int result = ops->send_msg(payload, (uint8_t)payload_len, true);
    if (result != 0) {
        at_msg_abort();
        write_str(write, "+CMSGHEX: ERROR\r\n");
    }
}

/* ============= Dispatcher ============= */

int at_cmd_dispatch(const at_ops_t *ops, const char *line,
                    void (*write)(const char *, uint16_t)) {
    /* Find command name and arguments */
    if (tolower((unsigned char)line[0]) != 'a' || tolower((unsigned char)line[1]) != 't') {
        write_str(write, "+AT: ERROR(-1)\r\n");
        return -1;
    }

    const char *cmd_start = line + 2;

    /* Check if it's just "AT" */
    if (cmd_start[0] == '\0') {
        cmd_at(ops, "", write);
        return 0;
    }

    if (cmd_start[0] != '+') {
        write_str(write, "+AT: ERROR(-1)\r\n");
        return -1;
    }

    cmd_start++;

    /* Find equals sign or end of command name */
    const char *args_start = cmd_start;
    while (*args_start && *args_start != '=' && *args_start != '\0') {
        args_start++;
    }

    size_t cmd_len = args_start - cmd_start;
    const char *args = (*args_start == '=') ? (args_start + 1) : "";

    /* Dispatch to handler */
    if (cmd_len == 3 && strcasecmp_n(cmd_start, "LOG", 3) == 0) {
        cmd_log(ops, args, write);
    } else if (cmd_len == 4 && strcasecmp_n(cmd_start, "UART", 4) == 0) {
        cmd_uart(ops, args, write);
    } else if (cmd_len == 3 && strcasecmp_n(cmd_start, "WDT", 3) == 0) {
        cmd_wdt(ops, args, write);
    } else if (cmd_len == 5 && strcasecmp_n(cmd_start, "RESET", 5) == 0) {
        cmd_reset(ops, args, write);
    } else if (cmd_len == 4 && strcasecmp_n(cmd_start, "MODE", 4) == 0) {
        cmd_mode(ops, args, write);
    } else if (cmd_len == 8 && strcasecmp_n(cmd_start, "LOWPOWER", 8) == 0) {
        cmd_lowpower(ops, args, write);
    } else if (cmd_len == 2 && strcasecmp_n(cmd_start, "LW", 2) == 0) {
        cmd_lw(ops, args, write);
    } else if (cmd_len == 2 && strcasecmp_n(cmd_start, "DR", 2) == 0) {
        cmd_dr(ops, args, write);
    } else if (cmd_len == 2 && strcasecmp_n(cmd_start, "CH", 2) == 0) {
        cmd_ch(ops, args, write);
    } else if (cmd_len == 5 && strcasecmp_n(cmd_start, "CLASS", 5) == 0) {
        cmd_class(ops, args, write);
    } else if (cmd_len == 3 && strcasecmp_n(cmd_start, "ADR", 3) == 0) {
        cmd_adr(ops, args, write);
    } else if (cmd_len == 5 && strcasecmp_n(cmd_start, "POWER", 5) == 0) {
        cmd_power(ops, args, write);
    } else if (cmd_len == 4 && strcasecmp_n(cmd_start, "PORT", 4) == 0) {
        cmd_port(ops, args, write);
    } else if (cmd_len == 5 && strcasecmp_n(cmd_start, "DELAY", 5) == 0) {
        cmd_delay(ops, args, write);
    } else if (cmd_len == 6 && strcasecmp_n(cmd_start, "RXWIN1", 6) == 0) {
        cmd_rxwin1(ops, args, write);
    } else if (cmd_len == 5 && strcasecmp_n(cmd_start, "RETRY", 5) == 0) {
        cmd_retry(ops, args, write);
    } else if (cmd_len == 4 && strcasecmp_n(cmd_start, "REPT", 4) == 0) {
        cmd_rept(ops, args, write);
    } else if (cmd_len == 3 && strcasecmp_n(cmd_start, "KEY", 3) == 0) {
        cmd_key(ops, args, write);
    } else if (cmd_len == 2 && strcasecmp_n(cmd_start, "ID", 2) == 0) {
        cmd_id(ops, args, write);
    } else if (cmd_len == 6 && strcasecmp_n(cmd_start, "MSGHEX", 6) == 0) {
        cmd_msghex(ops, args, write);
    } else if (cmd_len == 7 && strcasecmp_n(cmd_start, "CMSGHEX", 7) == 0) {
        cmd_cmsghex(ops, args, write);
    } else {
        write_str(write, "+AT: ERROR(-1)\r\n");
    }

    return 0;
}
