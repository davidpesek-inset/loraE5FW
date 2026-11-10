# Seeed-kompatibilní AT vrstva pro LoRa-E5 mini (STM32WLE5JC) – specifikace

Cíl: firmware, který na UART (USART1, 115200 8N1) odpovídá na podmnožinu Seeed LoRa-E5 AT příkazů tak,
aby fungoval stávající ESP kód (třída `IoTLoraDriver`). ESP po každém příkazu čte řádky a porovnává
je s očekávanou odpovědí **bez ohledu na bílé znaky** (předpoklad; přesto emitujte přesně formát níže).

Projekt: `Projects/Applications/LoRaWAN/LoRaWAN_End_Node` (CubeWL 1.1.0, LmHandler, EU868).
Nový kód patří do `LoRaWAN/App/at/` (nové soubory). Existující soubory neupravujte, pokud to zadání výslovně neříká.

## 1. Vstup a výstup

- Vstupní řádek končí `\n` nebo `\r` (ESP posílá `\r\n`). Prázdné řádky ignorujte.
- Ignorujte bajty `0x00` a `0xFF` (budicí bajt).
- Maximální délka řádku 640 znaků. Delší řádek zahoďte do konce řádku a odpovězte chybou `+AT: ERROR(-2)`.
- Před zpracováním ořežte mezery a tabulátory na začátku a na konci.
- Každá odpověď je jeden nebo více řádků zakončených `\r\n`. Žádné echo vstupu.
- Jména příkazů porovnávejte bez rozlišení velikosti písmen. Argumenty `ON/OFF/DEBUG/…` také.

## 2. Syntaxe příkazů

- `AT` – samotný test.
- `AT+<NAME>` – dotaz (bez argumentů).
- `AT+<NAME>=<arg1>,<arg2>,…` – nastavení. Argumenty oddělené čárkou, mezery okolo argumentů se ořežou,
  řetězec v uvozovkách `"…"` se zbaví uvozovek. Příklad: `AT+KEY=NWKSKEY, "2B7E…"`.
- Neznámý příkaz: odpověď `+AT: ERROR(-1)`. Špatný počet nebo hodnota argumentů: `+AT: ERROR(-2)`.
  (Přesné chybové texty Seeed jsou předpoklad.)

## 3. Příkazy v rozsahu (odpovědi přesně takto)

`<n>` = číslo z příkazu, `<HEX>` = hex řetězec velkými písmeny.

| Příkaz | Odpověď |
|---|---|
| `AT` | `+AT: OK` |
| `AT+LOG=DEBUG` / `AT+LOG=QUIET` | `+LOG: DEBUG` / `+LOG: QUIET` |
| `AT+UART=TIMEOUT,<n>` | `+UART: TIMEOUT, <n>` |
| `AT+UART=BR,<n>` | `+UART: BR, <n>`. Odpověď se odešle na STÁVAJÍCÍ rychlosti a teprve potom se zavolá `ops->set_baud(n)`. Význam `set_baud`: uložit rychlost pro příští start, **neměnit UART okamžitě** (ESP po tomto příkazu posílá `AT+RESET` ještě na staré rychlosti a přepne se až po resetu) |
| `AT+WDT` | `+WDT: ON` nebo `+WDT: OFF` (podle stavu, výchozí OFF) |
| `AT+WDT=ON` | `+WDT: ON` |
| `AT+RESET` | `+RESET: OK` (poté ops->reset()) |
| `AT+MODE=LWABP` | `+MODE: LWABP` |
| `AT+LOWPOWER=AUTOOFF` / `AUTOON` | `+LOWPOWER: AUTOOFF` / `+LOWPOWER: AUTOON` (jen se uloží stav) |
| `AT+LOWPOWER` | `+LOWPOWER: SLEEP` (zatím jen odpověď, bez spánku) |
| `AT+LW=NET` | `+LW: NET, ON` nebo `+LW: NET, OFF` (výchozí stav po startu je **ON**) |
| `AT+LW=NET,OFF` / `AT+LW=NET,ON` | `+LW: NET, OFF` / `+LW: NET, ON` |
| `AT+DR=EU868` | `+DR: EU868` |
| `AT+DR=<n>` (n = 0..5) | `+DR: EU868 DR<n>  SF<12-n> BW125K` – mezery: pro n>2 dvě mezery před `BW125K` (`SF9  BW125K`), pro n<=2 jedna (`SF10 BW125K`); za `DR<n>` vždy dvě mezery. Příklad: `+DR: EU868 DR3  SF9  BW125K`, `+DR: EU868 DR2  SF10 BW125K`. Pro n mimo 0..5 `+AT: ERROR(-2)` |
| `AT+CH=<idx>,<MHz>,DR0,DR5` | `+CH: <idx>,<Hz>,DR0:DR5`, kde `<Hz>` = `1000*(uint32)(1000*MHz)` (MHz jako desetinné číslo, např. `863.8` -> `863800000`, `864` -> `864000000`) |
| `AT+CLASS=A` | `+CLASS: A` (jiná třída: `+AT: ERROR(-2)`) |
| `AT+ADR=OFF` / `ON` | `+ADR: OFF` / `+ADR: ON` |
| `AT+POWER=<n>,FORCE` | `+POWER: <n>, FORCE` |
| `AT+LW=DC,OFF` | `+LW: DC, OFF, 0` |
| `AT+LW=CT,OFF` | `+LW: CT, OFF` |
| `AT+LW=LDRO,OFF` | `+LW: LDRO, OFF` |
| `AT+LW=THLD,<n>` | `+LW: THLD, <n>` (n může být záporné) |
| `AT+LW=DUMRX,ON` | `+LW: DUMRX, ON` (a `OFF` analogicky) |
| `AT+PORT=<n>` | `+PORT: <n>` (1..223) |
| `AT+DELAY=RX1,<n>` / `AT+DELAY=RX2,<n>` | `+DELAY: <n>` (n v ms) |
| `AT+RXWIN1=<a>,<b>` | `+RXWIN1: <a>,<b>` |
| `AT+RETRY=<n>` | `+RETRY: <n>` |
| `AT+REPT=<n>` | `+REPT: <n>` |
| `AT+KEY=NWKSKEY,"<32 hex>"` | `+KEY: NWKSKEY <HEX>` (velká písmena, bez uvozovek) |
| `AT+KEY=APPSKEY,"<32 hex>"` | `+KEY: APPSKEY <HEX>` |
| `AT+ID=DevAddr,"<8 hex>"` | `+ID: DevAddr, XX:XX:XX:XX` (po dvojicích, oddělené dvojtečkou, velká písmena) |
| `AT+LW=ULDL` | `+LW: ULDL <ul>, <dl>` (čítače uplink a downlink, desítkově) |
| `AT+LW=ULDL,<ul>,<dl>` | `+LW: ULDL, <ul>,<dl>` (nastaví čítače) |
| `AT+MSGHEX="<hex>"` | volá `ops->send_msg(payload, len, confirmed=0)`; odpověď řeší asynchronně `at_msg_done()` viz níže |
| `AT+CMSGHEX="<hex>"` | totéž s `confirmed=1` |

`AT+MSGHEX` sekvence odpovědí: ihned `+MSGHEX: Start`, po dokončení odeslání (callback z vyšší vrstvy)
`+MSGHEX: Done`. Pro `CMSGHEX` stejně s prefixem `+CMSGHEX:`. Při chybě hex řetězce (liché
množství znaků, neplatné znaky, délka > 242 bajtů): `+AT: ERROR(-2)`. Při odmítnutí odeslání stackem
(`ops->send_msg` vrátí nenulu): `+MSGHEX: ERROR` (resp. `+CMSGHEX: ERROR`).
Dokud odesílání běží, další `MSGHEX/CMSGHEX` odmítněte odpovědí `+AT: ERROR(-3)`.

## 4. Architektura (soubory v `LoRaWAN/App/at/`)

1. `at_ops.h` – struktura `at_ops_t` s ukazateli na funkce, které implementuje vyšší vrstva (v etapě 3
   LmHandler, v testech atrapa). Vše vrací `int` (0 = OK):
   `set_log(bool debug)`, `set_uart_timeout_ms(uint32_t)`, `set_baud(uint32_t)`, `set_wdt(bool)`,
   `reset(void)`, `set_mode_lwabp(void)`, `set_public_network(bool)`, `set_dr(uint8_t)`,
   `set_channel(uint8_t idx, uint32_t freq_hz)`, `set_class_a(void)`, `set_adr(bool)`,
   `set_tx_power_dbm(int8_t)`, `set_dc(bool)`, `set_dumrx(bool)`, `set_port(uint8_t)`,
   `set_rx_delay_ms(uint8_t rx /*1|2*/, uint32_t ms)`, `set_retry(uint8_t)`, `set_rept(uint8_t)`,
   `set_nwkskey(const uint8_t key[16])`, `set_appskey(const uint8_t key[16])`, `set_devaddr(uint32_t)`,
   `get_counters(uint32_t *ul, uint32_t *dl)`, `set_counters(uint32_t ul, uint32_t dl)`,
   `send_msg(const uint8_t *data, uint8_t len, bool confirmed)`.
   Neimplementované (NULL) ukazatele = příkaz vrátí `+AT: ERROR(-1)`.
2. `at_core.h/.c` – API:
   - `void at_init(const at_ops_t *ops, void (*write)(const char *str, uint16_t len))`
   - `void at_rx_byte(uint8_t b)` – volá se z UART přerušení nebo úlohy; skládá řádek do bufferu,
     po konci řádku jej zařadí ke zpracování.
   - `void at_process(void)` – zpracuje případný hotový řádek (volá se ze sequenceru/hlavní smyčky;
     nikdy ne v přerušení).
   - `void at_msg_done(bool success)` – vyšší vrstva ohlásí dokončení odeslání; vytiskne `Done` nebo `ERROR`.
3. `at_cmds.c` (+ `.h`) – tabulka příkazů `{ "LOG", handler }` a handlery. Stav (`at_state_t`):
   public_network (výchozí true), log_debug, wdt, baud (115200), uart_timeout, ULDL se bere z ops.
4. `at_selftest.c` (+ `.h`) – `int at_selftest(void (*print)(const char*))`: atrapa ops, krmí `at_core`
   řádky z tabulky v sekci 3 a porovnává výstup. Vrací počet chyb a vytiskne `PASS`/`FAIL <příkaz> got <…>`.
   Žádné závislosti na HAL. Musí jít přeložit i hostitelským překladačem (čisté C99).

## 5. Pravidla kódu

- C99, žádné `malloc`, žádné dynamické struktury. Statické buffery. Bez `printf` z newlib
  (ten nepoužívejte; vlastní převody čísel na text nebo `snprintf` jen na cílové platformě – preferujte vlastní).
- Žádné závislosti na HAL/LmHandler v `at_core`, `at_cmds` a `at_selftest`; vše přes `at_ops_t`.
- Kód musí projít `arm-none-eabi-gcc -std=gnu11 -Wall -Wextra -Werror -mcpu=cortex-m4 -mthumb -fsyntax-only`.
- Komentáře krátké, anglicky. Žádné emoji.

## 5b. UART receive timeout (mezi-znakový)

Časovač začíná běžet po přijetí prvního znaku řádku a **při každém dalším přijatém znaku se restartuje**.
Pokud vyprší dřív, než dorazí konec řádku (`\n`/`\r`), nedokončený řádek se zahodí a odešle se
`+AT: ERROR(-4)\r\n`.

- Hodnota (ms) se nastavuje příkazem `AT+UART=TIMEOUT,<n>` (už existuje; musí navíc předat hodnotu jádru
  přes `at_set_rx_timeout(uint32_t ms)`). Výchozí 1000 ms. `0` = časovač vypnutý. Povolený rozsah 0..65535,
  jinak `+AT: ERROR(-2)`.
- Znaky 0x00 a 0xFF se ignorují a časovač nespouštějí.
- Jádro samo čas neměří. Přes `at_ops_t` dostane dvě funkce: `rx_timer_restart(uint32_t ms)` (spustit/restartovat
  jednorázový časovač) a `rx_timer_stop(void)`. Jsou-li NULL, funkce časovače je neaktivní.
- Při každém přijatém platném znaku (ne konec řádku): `rx_timer_restart(timeout)` (jen pokud timeout > 0).
  Při konci řádku (dokončený, prázdný nebo přetečený): `rx_timer_stop()`.
- Nové API: `void at_rx_timeout(void)` – volá ho vyšší vrstva, když časovač vyprší. Pokud je rozepsaný
  řádek (`rx_pos > 0` nebo režim přetečení), vymaže ho a nastaví příznak `timeout_pending`; `at_process()` pak
  vypíše `+AT: ERROR(-4)\r\n`. Pokud není nic rozepsané, nedělá nic. Smí se volat z jiného kontextu než
  `at_rx_byte`, proto jen nastavuje příznaky (žádné volání handlerů, žádný výpis).
- Chyba `+AT: ERROR(-4)` = timeout příjmu.

## 6. Mimo rozsah této etapy

TEST mód, RTC, EEPROM, `AT+TEMP`, skutečné spojení s LmHandler, perzistence do flash, low-power.
