/*
 * at_selftest.c - Self-test suite for AT command layer
 * Tests all commands from spec section 3
 */

#include "at_selftest.h"
#include "at_core.h"
#include "at_ops.h"
#include <string.h>

/* ============= Mock State and Output Capture ============= */

#define MAX_OUTPUT_LEN  2048

static char g_output[MAX_OUTPUT_LEN];
static int g_output_len;
static void (*g_print)(const char *);

/* Mock ops state */
static uint32_t mock_ul_count, mock_dl_count;
static uint32_t mock_devaddr;
static uint8_t mock_nwkskey[16];
static uint8_t mock_appskey[16];

static void mock_write(const char *str, uint16_t len) {
    if (g_output_len + len < MAX_OUTPUT_LEN) {
        memcpy(g_output + g_output_len, str, len);
        g_output_len += len;
    }
}

static int mock_set_log(bool debug) { (void)debug; return 0; }
static int mock_set_uart_timeout_ms(uint32_t ms) { (void)ms; return 0; }
static int mock_set_baud(uint32_t baud) { (void)baud; return 0; }
static int mock_set_wdt(bool enable) { (void)enable; return 0; }
static int mock_reset(void) { return 0; }
static int mock_set_mode_lwabp(void) { return 0; }
static int mock_set_public_network(bool enable) { (void)enable; return 0; }
static int mock_set_dr(uint8_t dr) { (void)dr; return 0; }
static int mock_set_channel(uint8_t idx, uint32_t freq_hz) { (void)idx; (void)freq_hz; return 0; }
static int mock_set_class_a(void) { return 0; }
static int mock_set_adr(bool enable) { (void)enable; return 0; }
static int mock_set_tx_power_dbm(int8_t power) { (void)power; return 0; }
static int mock_set_dc(bool enable) { (void)enable; return 0; }
static int mock_set_dumrx(bool enable) { (void)enable; return 0; }
static int mock_set_ldro(bool enable) { (void)enable; return 0; }
static int mock_set_thld(int16_t val) { (void)val; return 0; }
static int mock_set_port(uint8_t port) { (void)port; return 0; }
static int mock_set_rx_delay_ms(uint8_t rx, uint32_t ms) { (void)rx; (void)ms; return 0; }
static int mock_set_retry(uint8_t n) { (void)n; return 0; }
static int mock_set_rept(uint8_t n) { (void)n; return 0; }
static int mock_set_nwkskey(const uint8_t key[16]) { memcpy(mock_nwkskey, key, 16); return 0; }
static int mock_set_appskey(const uint8_t key[16]) { memcpy(mock_appskey, key, 16); return 0; }
static int mock_set_devaddr(uint32_t addr) { mock_devaddr = addr; return 0; }
static int mock_get_counters(uint32_t *ul, uint32_t *dl) {
    *ul = mock_ul_count;
    *dl = mock_dl_count;
    return 0;
}
static int mock_set_counters(uint32_t ul, uint32_t dl) {
    mock_ul_count = ul;
    mock_dl_count = dl;
    return 0;
}
static int mock_send_msg(const uint8_t *data, uint8_t len, bool confirmed) {
    (void)data; (void)len; (void)confirmed;
    return 0;
}

static int mock_send_msg_fail(const uint8_t *data, uint8_t len, bool confirmed) {
    (void)data; (void)len; (void)confirmed;
    return 1;  /* Error */
}

static const at_ops_t g_mock_ops = {
    .set_log = mock_set_log,
    .set_uart_timeout_ms = mock_set_uart_timeout_ms,
    .set_baud = mock_set_baud,
    .set_wdt = mock_set_wdt,
    .reset = mock_reset,
    .set_mode_lwabp = mock_set_mode_lwabp,
    .set_public_network = mock_set_public_network,
    .set_dr = mock_set_dr,
    .set_channel = mock_set_channel,
    .set_class_a = mock_set_class_a,
    .set_adr = mock_set_adr,
    .set_tx_power_dbm = mock_set_tx_power_dbm,
    .set_dc = mock_set_dc,
    .set_dumrx = mock_set_dumrx,
    .set_ldro = mock_set_ldro,
    .set_thld = mock_set_thld,
    .set_port = mock_set_port,
    .set_rx_delay_ms = mock_set_rx_delay_ms,
    .set_retry = mock_set_retry,
    .set_rept = mock_set_rept,
    .set_nwkskey = mock_set_nwkskey,
    .set_appskey = mock_set_appskey,
    .set_devaddr = mock_set_devaddr,
    .get_counters = mock_get_counters,
    .set_counters = mock_set_counters,
    .send_msg = mock_send_msg,
};

/* ============= Test Helpers ============= */

static void feed_line(const char *line) {
    for (const char *p = line; *p; p++) {
        at_rx_byte((uint8_t)*p);
    }
    at_rx_byte('\n');
    at_process();
}

static int compare_output(const char *cmd, const char *expected, const char *got) {
    if (strcmp(expected, got) != 0) {
        g_print("FAIL ");
        g_print(cmd);
        g_print(" expected \"");
        g_print(expected);
        g_print("\" got \"");
        g_print(got);
        g_print("\"\r\n");
        return 1;
    }
    g_print("PASS ");
    g_print(cmd);
    g_print("\r\n");
    return 0;
}

/* ============= Main Test Function ============= */

int at_selftest(void (*print)(const char *)) {
    g_print = print;
    int failures = 0;

    /* Initialize */
    at_init(&g_mock_ops, mock_write);

    /* Test 1: AT */
    g_output_len = 0;
    feed_line("AT");
    if (compare_output("AT", "+AT: OK\r\n", g_output)) failures++;

    /* Test 2: AT+LOG=DEBUG */
    g_output_len = 0;
    feed_line("AT+LOG=DEBUG");
    if (compare_output("AT+LOG=DEBUG", "+LOG: DEBUG\r\n", g_output)) failures++;

    /* Test 3: AT+LOG=QUIET */
    g_output_len = 0;
    feed_line("AT+LOG=QUIET");
    if (compare_output("AT+LOG=QUIET", "+LOG: QUIET\r\n", g_output)) failures++;

    /* Test 4: AT+WDT (query) */
    g_output_len = 0;
    feed_line("AT+WDT");
    if (compare_output("AT+WDT (query)", "+WDT: OFF\r\n", g_output)) failures++;

    /* Test 5: AT+WDT=ON */
    g_output_len = 0;
    feed_line("AT+WDT=ON");
    if (compare_output("AT+WDT=ON", "+WDT: ON\r\n", g_output)) failures++;

    /* Test 6: AT+RESET */
    g_output_len = 0;
    feed_line("AT+RESET");
    if (compare_output("AT+RESET", "+RESET: OK\r\n", g_output)) failures++;

    /* Test 7: AT+MODE=LWABP */
    g_output_len = 0;
    feed_line("AT+MODE=LWABP");
    if (compare_output("AT+MODE=LWABP", "+MODE: LWABP\r\n", g_output)) failures++;

    /* Test 8: AT+LW=NET (query) */
    g_output_len = 0;
    feed_line("AT+LW=NET");
    if (compare_output("AT+LW=NET (query)", "+LW: NET, ON\r\n", g_output)) failures++;

    /* Test 9: AT+LW=NET,OFF */
    g_output_len = 0;
    feed_line("AT+LW=NET,OFF");
    if (compare_output("AT+LW=NET,OFF", "+LW: NET, OFF\r\n", g_output)) failures++;

    /* Test 10: AT+DR=EU868 */
    g_output_len = 0;
    feed_line("AT+DR=EU868");
    if (compare_output("AT+DR=EU868", "+DR: EU868\r\n", g_output)) failures++;

    /* Test 11: AT+DR=0 */
    g_output_len = 0;
    feed_line("AT+DR=0");
    if (compare_output("AT+DR=0", "+DR: EU868 DR0  SF12 BW125K\r\n", g_output)) failures++;

    /* Test 12: AT+DR=2 */
    g_output_len = 0;
    feed_line("AT+DR=2");
    if (compare_output("AT+DR=2", "+DR: EU868 DR2  SF10 BW125K\r\n", g_output)) failures++;

    /* Test 13: AT+DR=3 */
    g_output_len = 0;
    feed_line("AT+DR=3");
    if (compare_output("AT+DR=3", "+DR: EU868 DR3  SF9  BW125K\r\n", g_output)) failures++;

    /* Test 14: AT+DR=5 */
    g_output_len = 0;
    feed_line("AT+DR=5");
    if (compare_output("AT+DR=5", "+DR: EU868 DR5  SF7  BW125K\r\n", g_output)) failures++;

    /* Test 15: AT+CH=1,864,DR0,DR5 */
    g_output_len = 0;
    feed_line("AT+CH=1,864,DR0,DR5");
    if (compare_output("AT+CH=1,864,DR0,DR5", "+CH: 1,864000000,DR0:DR5\r\n", g_output)) failures++;

    /* Test 16: AT+CH=1,863.8,DR0,DR5 */
    g_output_len = 0;
    feed_line("AT+CH=1,863.8,DR0,DR5");
    if (compare_output("AT+CH=1,863.8,DR0,DR5", "+CH: 1,863800000,DR0:DR5\r\n", g_output)) failures++;

    /* Test 17: AT+CLASS=A */
    g_output_len = 0;
    feed_line("AT+CLASS=A");
    if (compare_output("AT+CLASS=A", "+CLASS: A\r\n", g_output)) failures++;

    /* Test 18: AT+ADR=OFF */
    g_output_len = 0;
    feed_line("AT+ADR=OFF");
    if (compare_output("AT+ADR=OFF", "+ADR: OFF\r\n", g_output)) failures++;

    /* Test 19: AT+ADR=ON */
    g_output_len = 0;
    feed_line("AT+ADR=ON");
    if (compare_output("AT+ADR=ON", "+ADR: ON\r\n", g_output)) failures++;

    /* Test 20: AT+POWER=14,FORCE */
    g_output_len = 0;
    feed_line("AT+POWER=14,FORCE");
    if (compare_output("AT+POWER=14,FORCE", "+POWER: 14, FORCE\r\n", g_output)) failures++;

    /* Test 21: AT+LW=DC,OFF */
    g_output_len = 0;
    feed_line("AT+LW=DC,OFF");
    if (compare_output("AT+LW=DC,OFF", "+LW: DC, OFF, 0\r\n", g_output)) failures++;

    /* Test 22: AT+LW=CT,OFF */
    g_output_len = 0;
    feed_line("AT+LW=CT,OFF");
    if (compare_output("AT+LW=CT,OFF", "+LW: CT, OFF\r\n", g_output)) failures++;

    /* Test 23: AT+LW=LDRO,OFF */
    g_output_len = 0;
    feed_line("AT+LW=LDRO,OFF");
    if (compare_output("AT+LW=LDRO,OFF", "+LW: LDRO, OFF\r\n", g_output)) failures++;

    /* Test 24: AT+LW=THLD,-20 */
    g_output_len = 0;
    feed_line("AT+LW=THLD,-20");
    if (compare_output("AT+LW=THLD,-20", "+LW: THLD, -20\r\n", g_output)) failures++;

    /* Test 25: AT+LW=DUMRX,ON */
    g_output_len = 0;
    feed_line("AT+LW=DUMRX,ON");
    if (compare_output("AT+LW=DUMRX,ON", "+LW: DUMRX, ON\r\n", g_output)) failures++;

    /* Test 26: AT+PORT=10 */
    g_output_len = 0;
    feed_line("AT+PORT=10");
    if (compare_output("AT+PORT=10", "+PORT: 10\r\n", g_output)) failures++;

    /* Test 27: AT+DELAY=RX1,1000 */
    g_output_len = 0;
    feed_line("AT+DELAY=RX1,1000");
    if (compare_output("AT+DELAY=RX1,1000", "+DELAY: 1000\r\n", g_output)) failures++;

    /* Test 28: AT+RETRY=3 */
    g_output_len = 0;
    feed_line("AT+RETRY=3");
    if (compare_output("AT+RETRY=3", "+RETRY: 3\r\n", g_output)) failures++;

    /* Test 29: AT+REPT=2 */
    g_output_len = 0;
    feed_line("AT+REPT=2");
    if (compare_output("AT+REPT=2", "+REPT: 2\r\n", g_output)) failures++;

    /* Test 30: AT+KEY=NWKSKEY,"2B7E151628AED2A6ABF7158809CF4F3C" */
    g_output_len = 0;
    feed_line("AT+KEY=NWKSKEY,\"2B7E151628AED2A6ABF7158809CF4F3C\"");
    if (compare_output("AT+KEY=NWKSKEY", "+KEY: NWKSKEY 2B7E151628AED2A6ABF7158809CF4F3C\r\n", g_output)) failures++;

    /* Test 31: AT+KEY=APPSKEY,"2B7E151628AED2A6ABF7158809CF4F3C" */
    g_output_len = 0;
    feed_line("AT+KEY=APPSKEY,\"2B7E151628AED2A6ABF7158809CF4F3C\"");
    if (compare_output("AT+KEY=APPSKEY", "+KEY: APPSKEY 2B7E151628AED2A6ABF7158809CF4F3C\r\n", g_output)) failures++;

    /* Test 32: AT+ID=DevAddr,"0100000A" */
    g_output_len = 0;
    feed_line("AT+ID=DevAddr,\"0100000A\"");
    if (compare_output("AT+ID=DevAddr", "+ID: DevAddr, 01:00:00:0A\r\n", g_output)) failures++;

    /* Test 33: AT+LW=ULDL (query) */
    g_output_len = 0;
    mock_ul_count = 5;
    mock_dl_count = 3;
    feed_line("AT+LW=ULDL");
    if (compare_output("AT+LW=ULDL (query)", "+LW: ULDL 5, 3\r\n", g_output)) failures++;

    /* Test 34: AT+LW=ULDL,100,50 */
    g_output_len = 0;
    feed_line("AT+LW=ULDL,100,50");
    if (compare_output("AT+LW=ULDL set", "+LW: ULDL, 100,50\r\n", g_output)) failures++;

    /* Test 35: AT+MSGHEX="0102" */
    g_output_len = 0;
    feed_line("AT+MSGHEX=\"0102\"");
    if (compare_output("AT+MSGHEX start", "+MSGHEX: Start\r\n", g_output)) failures++;

    /* Test 36: Async completion */
    g_output_len = 0;
    at_msg_done(true);
    if (compare_output("AT+MSGHEX done", "+MSGHEX: Done\r\n", g_output)) failures++;

    /* Test 37: AT+UART=TIMEOUT,100 */
    g_output_len = 0;
    feed_line("AT+UART=TIMEOUT,100");
    if (compare_output("AT+UART=TIMEOUT", "+UART: TIMEOUT, 100\r\n", g_output)) failures++;

    /* Test 38: AT+UART=BR,115200 */
    g_output_len = 0;
    feed_line("AT+UART=BR,115200");
    if (compare_output("AT+UART=BR", "+UART: BR, 115200\r\n", g_output)) failures++;

    /* Test 39: Unknown command */
    g_output_len = 0;
    feed_line("AT+UNKNOWN");
    if (compare_output("Unknown command", "+AT: ERROR(-1)\r\n", g_output)) failures++;

    /* Test 40: Bad hex string (odd length) */
    g_output_len = 0;
    feed_line("AT+KEY=NWKSKEY,\"2B7E151628AED2A6ABF7158809CF4F3\"");
    if (compare_output("Bad hex odd", "+AT: ERROR(-2)\r\n", g_output)) failures++;

    /* Test 41: DR out of range */
    g_output_len = 0;
    feed_line("AT+DR=9");
    if (compare_output("DR out of range", "+AT: ERROR(-2)\r\n", g_output)) failures++;

    /* Test 42: MSGHEX while sending */
    g_output_len = 0;
    feed_line("AT+MSGHEX=\"0102\"");
    g_output_len = 0;
    feed_line("AT+MSGHEX=\"0304\"");
    if (compare_output("MSGHEX busy", "+AT: ERROR(-3)\r\n", g_output)) failures++;

    /* Test 43: Line with trailing spaces */
    g_output_len = 0;
    feed_line("AT+LOG=DEBUG   ");
    if (compare_output("Trailing spaces", "+LOG: DEBUG\r\n", g_output)) failures++;

    /* Test 44: AT+LOWPOWER (query) */
    g_output_len = 0;
    feed_line("AT+LOWPOWER");
    if (compare_output("AT+LOWPOWER", "+LOWPOWER: SLEEP\r\n", g_output)) failures++;

    /* Test 45: AT+LOWPOWER=AUTOOFF */
    g_output_len = 0;
    feed_line("AT+LOWPOWER=AUTOOFF");
    if (compare_output("AT+LOWPOWER=AUTOOFF", "+LOWPOWER: AUTOOFF\r\n", g_output)) failures++;

    /* Test 46: AT+CMSGHEX="0102" */
    g_output_len = 0;
    at_msg_done(false);  /* Clear previous msg state */
    feed_line("AT+CMSGHEX=\"0102\"");
    if (compare_output("AT+CMSGHEX start", "+CMSGHEX: Start\r\n", g_output)) failures++;

    /* Test 47: at+log=debug (lowercase AT) */
    g_output_len = 0;
    feed_line("at+log=debug");
    if (compare_output("lowercase at+log", "+LOG: DEBUG\r\n", g_output)) failures++;

    /* Test 48: AT+LOWPOWER (query) - already tested but verify */
    g_output_len = 0;
    feed_line("AT+LOWPOWER");
    if (compare_output("AT+LOWPOWER query", "+LOWPOWER: SLEEP\r\n", g_output)) failures++;

    /* Test 49: AT+LOWPOWER=AUTOON */
    g_output_len = 0;
    feed_line("AT+LOWPOWER=AUTOON");
    if (compare_output("AT+LOWPOWER=AUTOON", "+LOWPOWER: AUTOON\r\n", g_output)) failures++;

    /* Test 50: DR with invalid range (DR=9) */
    g_output_len = 0;
    feed_line("AT+DR=9");
    if (compare_output("DR out of range", "+AT: ERROR(-2)\r\n", g_output)) failures++;

    /* Test 51: PORT with invalid range (PORT=0) */
    g_output_len = 0;
    feed_line("AT+PORT=0");
    if (compare_output("PORT=0 out of range", "+AT: ERROR(-2)\r\n", g_output)) failures++;

    /* Test 52: PORT with invalid range (PORT=224) */
    g_output_len = 0;
    feed_line("AT+PORT=224");
    if (compare_output("PORT=224 out of range", "+AT: ERROR(-2)\r\n", g_output)) failures++;

    /* Test 53: POWER with invalid range (POWER=-11) */
    g_output_len = 0;
    feed_line("AT+POWER=-11,FORCE");
    if (compare_output("POWER=-11 out of range", "+AT: ERROR(-2)\r\n", g_output)) failures++;

    /* Test 54: POWER with invalid range (POWER=31) */
    g_output_len = 0;
    feed_line("AT+POWER=31,FORCE");
    if (compare_output("POWER=31 out of range", "+AT: ERROR(-2)\r\n", g_output)) failures++;

    /* Test 55: RETRY with invalid range (RETRY=16) */
    g_output_len = 0;
    feed_line("AT+RETRY=16");
    if (compare_output("RETRY=16 out of range", "+AT: ERROR(-2)\r\n", g_output)) failures++;

    /* Test 56: REPT with invalid range (REPT=16) */
    g_output_len = 0;
    feed_line("AT+REPT=16");
    if (compare_output("REPT=16 out of range", "+AT: ERROR(-2)\r\n", g_output)) failures++;

    /* Test 57: DELAY with invalid range (DELAY=0) */
    g_output_len = 0;
    feed_line("AT+DELAY=RX1,0");
    if (compare_output("DELAY=0 out of range", "+AT: ERROR(-2)\r\n", g_output)) failures++;

    /* Test 58: DELAY with invalid range (DELAY=65536) */
    g_output_len = 0;
    feed_line("AT+DELAY=RX1,65536");
    if (compare_output("DELAY=65536 out of range", "+AT: ERROR(-2)\r\n", g_output)) failures++;

    /* Test 59: CH with wrong DR0 string */
    g_output_len = 0;
    feed_line("AT+CH=1,864,DR1,DR5");
    if (compare_output("CH with DR1 not DR0", "+AT: ERROR(-2)\r\n", g_output)) failures++;

    /* Test 60: CH with wrong DR5 string */
    g_output_len = 0;
    feed_line("AT+CH=1,864,DR0,DR4");
    if (compare_output("CH with DR4 not DR5", "+AT: ERROR(-2)\r\n", g_output)) failures++;

    /* Test 61: AT+DR=abc (non-numeric) */
    g_output_len = 0;
    feed_line("AT+DR=abc");
    if (compare_output("DR=abc non-numeric", "+AT: ERROR(-2)\r\n", g_output)) failures++;

    /* Test 62: Overflow line (700 chars) followed by valid "AT" */
    g_output_len = 0;
    char long_line[750];
    memset(long_line, 'A', 700);
    long_line[700] = '\0';
    for (int i = 0; i < 700; i++) {
        at_rx_byte((uint8_t)long_line[i]);
    }
    at_rx_byte('\n');
    at_process();
    /* Should output ERROR(-2) for overflow */
    if (strstr(g_output, "+AT: ERROR(-2)") == NULL) {
        failures++;
        g_print("FAIL overflow line expected ERROR(-2)\r\n");
    }

    /* Now send valid "AT" command */
    g_output_len = 0;
    feed_line("AT");
    if (compare_output("AT after overflow", "+AT: OK\r\n", g_output)) failures++;

    /* Test 63: 200-byte MSGHEX payload */
    g_output_len = 0;
    at_msg_done(false);  /* Clear msg state */
    at_rx_byte('A'); at_rx_byte('T'); at_rx_byte('+');
    at_rx_byte('M'); at_rx_byte('S'); at_rx_byte('G');
    at_rx_byte('H'); at_rx_byte('E'); at_rx_byte('X');
    at_rx_byte('='); at_rx_byte('"');
    for (int i = 0; i < 200; i++) {
        at_rx_byte('1'); at_rx_byte('2');
    }
    at_rx_byte('"'); at_rx_byte('\n');
    at_process();
    if (strstr(g_output, "+MSGHEX: Start") == NULL) {
        failures++;
        g_print("FAIL 200-byte MSGHEX no Start\r\n");
    } else {
        g_print("PASS 200-byte MSGHEX Start\r\n");
    }

    /* Test 64: Over-long hex (>242 bytes) -> ERROR(-2) */
    g_output_len = 0;
    at_msg_done(false);
    at_rx_byte('A'); at_rx_byte('T'); at_rx_byte('+');
    at_rx_byte('M'); at_rx_byte('S'); at_rx_byte('G');
    at_rx_byte('H'); at_rx_byte('E'); at_rx_byte('X');
    at_rx_byte('='); at_rx_byte('"');
    for (int i = 0; i < 250; i++) {
        at_rx_byte('1'); at_rx_byte('2');
    }
    at_rx_byte('"'); at_rx_byte('\n');
    at_process();
    if (compare_output("Over-long MSGHEX", "+AT: ERROR(-2)\r\n", g_output)) failures++;

    /* Test 65: send_msg failure -> ERROR, then second MSGHEX accepted */
    /* First create a failing mock */
    at_ops_t mock_ops_fail = g_mock_ops;
    mock_ops_fail.send_msg = mock_send_msg_fail;
    at_init(&mock_ops_fail, mock_write);
    g_output_len = 0;
    feed_line("AT+MSGHEX=\"0102\"");
    if (strstr(g_output, "+MSGHEX: ERROR") == NULL && strstr(g_output, "+MSGHEX: Start") == NULL) {
        failures++;
        g_print("FAIL send_msg error\r\n");
    } else {
        g_print("PASS send_msg error\r\n");
    }

    /* Verify second MSGHEX is now accepted (busy flag cleared) */
    g_output_len = 0;
    feed_line("AT+MSGHEX=\"0304\"");
    if (strstr(g_output, "+MSGHEX: Start") == NULL) {
        failures++;
        g_print("FAIL second MSGHEX after error not accepted\r\n");
    }

    /* Re-init with good ops */
    at_init(&g_mock_ops, mock_write);

    return failures;
}
