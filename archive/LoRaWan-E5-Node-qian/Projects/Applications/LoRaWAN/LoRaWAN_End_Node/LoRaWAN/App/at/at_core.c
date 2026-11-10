/*
 * at_core.c - AT command core processor
 * Stateful line receiver and command dispatcher
 *
 * Buffering design: two buffers and overflow handling
 * - rx_buffer: current line being filled
 * - cmd_buffer: complete pending line (set by at_rx_byte only when !cmd_pending)
 * - overflow_pending: marks a line-too-long error (replaces cmd_buffer content)
 *
 * Race prevention: at_process copies cmd_buffer to work_buffer before clearing
 * cmd_pending flag, preventing at_rx_byte (in interrupt) from overwriting it
 */

#include "at_core.h"
#include "at_cmds.h"
#include <string.h>
#include <ctype.h>

#define MAX_LINE_LEN    640
#define RX_BUF_SIZE     (MAX_LINE_LEN + 1)

/* State */
static const at_ops_t *g_ops;
static void (*g_write)(const char *str, uint16_t len);

/* Buffers: one being filled (rx), one pending (cmd), one for processing (work) */
static char rx_buffer[RX_BUF_SIZE];
static uint16_t rx_pos;
static bool rx_overflow;  /* Overflow flag: drop bytes until next line ending */
static char cmd_buffer[RX_BUF_SIZE];
static bool cmd_pending;
static bool overflow_pending;  /* True when cmd_buffer holds overflow marker */
static char work_buffer[RX_BUF_SIZE];  /* For processing, protected from interrupt */

/* Async message state */
static bool msg_sending;
static char msg_prefix[16];  /* "+MSGHEX: " or "+CMSGHEX: " */

/* Inter-character RX timeout state (spec 5b).
 * timeout_pending is set by at_rx_timeout() (may run in another context) and
 * consumed by at_process(). at_rx_timeout() must not run concurrently with
 * at_rx_byte() on the same buffers; the caller serializes them. */
#define DEFAULT_RX_TIMEOUT_MS  1000u
static uint32_t rx_timeout_ms;
static bool timeout_pending;

/* Forward declarations */
extern int at_cmd_dispatch(const at_ops_t *ops, const char *line,
                           void (*write)(const char *, uint16_t));

static void trim_and_normalize(char *line) {
    /* Trim leading whitespace */
    char *start = line;
    while (*start && (*start == ' ' || *start == '\t')) {
        start++;
    }
    /* Trim trailing whitespace */
    char *end = start + strlen(start) - 1;
    while (end >= start && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) {
        end--;
    }
    *(end + 1) = '\0';

    if (start != line) {
        memmove(line, start, end - start + 2);
    }
}

/* Restart RX timer after an accepted character (no-op if disabled or op missing) */
static void rx_timer_kick(void) {
    if (g_ops && g_ops->rx_timer_restart && rx_timeout_ms > 0) {
        g_ops->rx_timer_restart(rx_timeout_ms);
    }
}

/* Stop RX timer at a line end (no-op if op missing) */
static void rx_timer_halt(void) {
    if (g_ops && g_ops->rx_timer_stop) {
        g_ops->rx_timer_stop();
    }
}

void at_init(const at_ops_t *ops, void (*write)(const char *str, uint16_t len)) {
    g_ops = ops;
    g_write = write;
    rx_pos = 0;
    rx_overflow = false;
    cmd_pending = false;
    overflow_pending = false;
    msg_sending = false;
    rx_timeout_ms = DEFAULT_RX_TIMEOUT_MS;
    timeout_pending = false;
    memset(rx_buffer, 0, sizeof(rx_buffer));
    memset(cmd_buffer, 0, sizeof(cmd_buffer));
    memset(work_buffer, 0, sizeof(work_buffer));
}

void at_rx_byte(uint8_t b) {
    /* Skip null bytes and wake bytes */
    if (b == 0x00 || b == 0xFF) {
        return;
    }

    /* Check for line ending */
    if (b == '\n' || b == '\r') {
        if (rx_overflow) {
            /* End of overflow line, set overflow_pending only if no cmd pending */
            rx_timer_halt();
            if (!cmd_pending) {
                overflow_pending = true;
                cmd_pending = true;
            }
            rx_overflow = false;
            rx_pos = 0;
        } else if (rx_pos > 0) {
            /* Complete line: only queue if no command is pending */
            rx_timer_halt();
            rx_buffer[rx_pos] = '\0';
            trim_and_normalize(rx_buffer);

            if (strlen(rx_buffer) == 0) {
                rx_pos = 0;
                return;
            }

            if (!cmd_pending) {
                strcpy(cmd_buffer, rx_buffer);
                overflow_pending = false;
                cmd_pending = true;
            }
            rx_pos = 0;
        }
        /* Terminator with no partial line (e.g. the \r of \r\n) does not end a line */
        return;
    }

    /* Accepted non-terminator byte: restart inter-character timer
     * (also while skipping overflow bytes, so a stalled long line times out) */
    rx_timer_kick();

    /* In overflow mode, just skip bytes until line ending */
    if (rx_overflow) {
        return;
    }

    /* Check line length */
    if (rx_pos >= MAX_LINE_LEN) {
        /* Line too long: enter overflow mode */
        rx_overflow = true;
        rx_pos = 0;
        return;
    }

    /* Add byte to buffer */
    rx_buffer[rx_pos++] = (char)b;
}

void at_process(void) {
    /* RX timeout is reported before any pending command line */
    if (timeout_pending) {
        timeout_pending = false;
        const char *err = "+AT: ERROR(-4)\r\n";
        g_write(err, strlen(err));
    }

    if (!cmd_pending) {
        return;
    }

    /* Copy to work buffer while cmd_pending is still true (atomic from at_rx_byte perspective) */
    strcpy(work_buffer, cmd_buffer);
    bool is_overflow = overflow_pending;

    /* Now clear cmd_pending so at_rx_byte can queue another command */
    cmd_pending = false;

    /* Handle overflow error */
    if (is_overflow) {
        const char *err = "+AT: ERROR(-2)\r\n";
        g_write(err, strlen(err));
        return;
    }

    /* Dispatch to command handler */
    at_cmd_dispatch(g_ops, work_buffer, g_write);
}

void at_msg_done(bool success) {
    if (!msg_sending) {
        return;
    }

    msg_sending = false;

    const char *status = success ? "Done" : "ERROR";
    char response[32];
    int len = 0;

    /* Build response: "+MSGHEX: Done\r\n" or "+CMSGHEX: ERROR\r\n" */
    while (msg_prefix[len] && len < 15) {
        response[len] = msg_prefix[len];
        len++;
    }
    response[len++] = ' ';

    int i = 0;
    while (status[i] && len < 30) {
        response[len++] = status[i++];
    }
    response[len++] = '\r';
    response[len++] = '\n';

    g_write(response, len);
}

/* Called by at_cmds to signal MSGHEX/CMSGHEX start */
void at_msg_start(const char *prefix) {
    msg_sending = true;
    strncpy(msg_prefix, prefix, sizeof(msg_prefix) - 1);
    msg_prefix[sizeof(msg_prefix) - 1] = '\0';

    char response[32];
    int len = 0;
    while (msg_prefix[len] && len < 15) {
        response[len] = msg_prefix[len];
        len++;
    }
    response[len++] = ' ';
    response[len++] = 'S';
    response[len++] = 't';
    response[len++] = 'a';
    response[len++] = 'r';
    response[len++] = 't';
    response[len++] = '\r';
    response[len++] = '\n';

    g_write(response, len);
}

bool at_is_msg_sending(void) {
    return msg_sending;
}

/* Abort message send without output (called on send_msg error) */
void at_msg_abort(void) {
    msg_sending = false;
}

/* Set inter-character RX timeout; 0 disables the timer */
void at_set_rx_timeout(uint32_t ms) {
    rx_timeout_ms = ms;
}

/* RX timer expired: drop a partial line and flag the error.
 * Only sets state; the ERROR(-4) text is written by at_process(). */
void at_rx_timeout(void) {
    if (rx_pos > 0 || rx_overflow) {
        rx_pos = 0;
        rx_overflow = false;
        timeout_pending = true;
    }
}
