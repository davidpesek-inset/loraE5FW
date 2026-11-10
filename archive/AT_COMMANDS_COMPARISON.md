# Porovnání AT příkazů: lorae5_fw_prompt.md vs. ST AT_Slave

Zdroje:
- **Prompt:** `C:\Users\pesek\Projekty\stm32_lorae5_fw\lorae5_fw_prompt.md` (příkazy a operace, které používá hostitelský driver).
- **AT_Slave:** `LoRaWAN/App/lora_command.c` (tabulka příkazů), `lora_at.c` (handlery a výpisy), `lora_at.h` (názvy příkazů).

## Legenda

- **RUN** = `AT+X`, **GET** = `AT+X?`, **SET** = `AT+X=<params>` (dle `at_op_t` v promptu).
- **Implementováno v AT_Slave** = ANO, pokud AT_Slave obsahuje příkaz **se stejným názvem a stejnou operací**. Pokud AT_Slave dělá podobnou věc pod jiným názvem, je to NE a ekvivalent je uvedený v poznámce.
- Operace `TEST` (`AT+X=?`) prompt nepožaduje u žádného příkazu, proto není v tabulkách.

---

## Tabulka 1: Implementace příkazů z promptu v AT_Slave

| Č. | Příkaz | Operace (dle promptu) | Implementováno v AT_Slave | Poznámka / nejbližší ekvivalent v AT_Slave |
| :-: | :----- | :-------------------- | :-----------------------: | :------------------------------------------ |
| 1 | AT+WDT | SET | NE | — |
| 1 | AT+WDT | GET (i bez `?`) | NE | — |
| 2 | AT+LOG | SET | NE | `AT+VL=<0..3>` (verbose level) |
| 3 | AT | RUN | ANO | `AT` → `+AT:OK` |
| 4 | AT+UART | SET | NE | — |
| 5 | AT+RESET | RUN | NE | `ATZ` (reset MCU) |
| 6 | AT+MODE | SET | NE | — |
| 7 | AT+DR | SET | ANO (částečně) | `AT+DR=<0..15>` ano, ale hodnotu `EU868` (region) nepřijme; region se nastavuje přes `AT+BAND=<id>` |
| 8 | AT+CH | SET | NE | — |
| 9 | AT+CLASS | SET | ANO | `AT+CLASS=A\|B\|C` (prompt vyžaduje jen `A`) |
| 10 | AT+ADR | SET | ANO (částečně) | `AT+ADR=0\|1` místo `ON`/`OFF` |
| 11 | AT+POWER | SET | NE | `AT+TXP=<idx>` (index výkonu dle regionu, ne dBm, bez `FORCE`) |
| 12 | AT+LW | SET | NE | — (`DC` odpovídá `AT+DCS`, jinak žádný ekvivalent) |
| 12 | AT+LW | GET (`AT+LW=<par>`) | NE | — |
| 13 | AT+PORT | SET | NE | port je součástí `AT+SEND=<port>:...` |
| 14 | AT+DELAY | SET | NE | `AT+RX1DL=<ms>`, `AT+RX2DL=<ms>` (dva samostatné příkazy) |
| 15 | AT+RXWIN1 | SET | NE | `AT+RX2FQ=<Hz>`, `AT+RX2DR=<DR>` (jen RX2) |
| 16 | AT+RETRY | SET | NE | — |
| 17 | AT+REPT | SET | NE | — |
| 18 | AT+KEY | SET | NE | `AT+NWKSKEY=...`, `AT+APPSKEY=...` (dva samostatné příkazy) |
| 19 | AT+ID | SET | NE | `AT+DADDR=XX:XX:XX:XX` |
| 20 | AT+MSGHEX | SET | NE | `AT+SEND=<port>:0:<hex>` |
| 21 | AT+CMSGHEX | SET | NE | `AT+SEND=<port>:1:<hex>` |
| 22 | AT+TEMP | RUN | NE | — (`AT+BAT?` vrací napětí baterie, ne teplotu) |
| 23 | AT+RTC | SET | NE | — |
| 23 | AT+RTC | GET (`AT+RTC=UTCMS`) | NE | `AT+LTIME?` (lokální čas bez ms) |
| 24 | AT+EEPROM | GET (`AT+EEPROM=<addr>`) | NE | — |
| 24 | AT+EEPROM | SET | NE | — |
| 25 | AT+TEST | SET | NE | `AT+TCONF=...`, `AT+TTX=<n>`, `AT+TRX=<n>` (jiné příkazy, jiná syntaxe) |
| 26 | AT+LOWPOWER | RUN | NE | — |
| 26 | AT+LOWPOWER | SET | NE | — |

**Souhrn:** AT_Slave přímo pokrývá jen `AT` (RUN), `AT+CLASS` (SET) a částečně `AT+DR` a `AT+ADR` (SET). Zbylých 22 příkazů z promptu v AT_Slave pod stejným názvem neexistuje.

---

## Tabulka 2: Porovnání formátů (prompt vs. AT_Slave)

Pokud AT_Slave příkaz pod stejným názvem nemá, je v pravém sloupci formát nejbližšího ekvivalentu (označen `ekv.`) nebo `—`.

### Obecné

| Vlastnost | Formát dle promptu | Formát v AT_Slave | Shodné |
| :-------- | :----------------- | :---------------- | :----: |
| Ukončení příkazu | `\r\n` | `\r` nebo `\n` | ČÁSTEČNĚ |
| Ukončení odpovědi | `\r\n` na konci řádku | řádky `\r\n`, po každém příkazu navíc `\r\nOK\r\n` nebo chybové slovo | NE |
| Úspěch bez dat | nic navíc (jen řádek `+X: ...`) | `\r\nOK\r\n` | NE |
| Chyba | `+<NAME>: ERROR(<code>)` (`-1`, `-10`, `-11`, `-12`, `-20`, `-21`) | `\r\nAT_ERROR\r\n`, `AT_PARAM_ERROR`, `AT_BUSY_ERROR`, `AT_TEST_PARAM_OVERFLOW`, `AT_NO_NETWORK_JOINED`, `AT_RX_ERROR`, `AT_NO_CLASS_B_ENABLE`, `AT_DUTYCYCLE_RESTRICTED`, `AT_CRYPTO_ERROR` | NE |
| Oddělovač parametrů | čárka `,` (max 15), uvozovky chrání čárky | jeden řetězec za `=`; oddělovač podle příkazu (`:` nebo `,`), bez společného parseru parametrů | NE |
| Velikost písmen u příkazů | nerozlišuje | rozlišuje (`strncmp`) | NE |
| Maximální délka příkazu | 528 B | 539 znaků (`CMD_SIZE` 540) | NE |
| Dotaz (GET) | `AT+X?`, výjimky `AT+X=<par>` (LW, RTC, EEPROM) a `AT+WDT` bez `?` | `AT+X?` | ČÁSTEČNĚ |
| Hex hodnoty | velkými písmeny, klíče bez dvojteček | velkými písmeny, **s dvojtečkami** (`XX:XX:...`) | NE |

### Příkazy

| Příkaz | Operace | Formát dle promptu (příkaz → odpověď) | Formát v AT_Slave (příkaz → odpověď) | Shodné | DONE | Ukládat do eeprom? | Ukládáno přes NVM |
| :----- | :------ | :-------------------------------------- | :------------------------------------ | :----: | :---: | :---: | :---: |
| AT+WDT | SET | `AT+WDT=ON\|OFF` → `+WDT: ON` / `+WDT: OFF` | — | — | DONE | ANO | NE |
| AT+WDT | GET | `AT+WDT?` (i `AT+WDT`) → `+WDT: ON\|OFF` | — | — | DONE | - | - |
| AT+LOG | SET | `AT+LOG=QUIET\|DEBUG` → `+LOG: QUIET` / `+LOG: DEBUG` | ekv.: `AT+VL=<0..3>` → `OK` | NE | DONE | NE | NE |
| AT | RUN | `AT` → `+AT: OK` | `AT` → `+AT:OK` a poté `OK` | ČÁSTEČNĚ (chybí mezera, navíc `OK`) | DONE | - | - |
| AT+UART | SET | `AT+UART=BR,<baud>` → `+UART: BR, <baud>`; `AT+UART=TIMEOUT,<ms>` → `+UART: TIMEOUT, <ms>` | — | — | DONE | ANO - BR, TIMEOUT | NE |
| AT+RESET | RUN | `AT+RESET` → `+RESET: OK`, pak reset | ekv.: `ATZ` → bez odpovědi, okamžitý reset | NE | DONE | - | - |
| AT+MODE | SET | `AT+MODE=LWABP\|TEST` → `+MODE: LWABP` / `+MODE: TEST` | — | — | DONE | ANO | NE |
| AT+DR | SET | `AT+DR=EU868` → `+DR: EU868`; `AT+DR=<n>` → `+DR: EU868 DR<n> SF<sf> BW125K` | `AT+DR=<0..15>` → `OK` (bez výpisu hodnoty); `EU868` nepodporuje | ČÁSTEČNĚ (jiná odpověď) | DONE | ANO (DR(int) je nutno přepsat ručně) | ANO |
| AT+CH | SET | `AT+CH=<n>,<MHz>,DR<min>,DR<max>` → `+CH: <n>,<Hz>,DR<min>:DR<max>` | — | — | DONE | NE | ANO |
| AT+CLASS | SET | `AT+CLASS=A` → `+CLASS: A` | `AT+CLASS=A\|B\|C` → `OK` | ČÁSTEČNĚ (jiná odpověď) | DONE | NE | ANO |
| AT+ADR | SET | `AT+ADR=ON\|OFF` → `+ADR: ON` / `+ADR: OFF` | `AT+ADR=0\|1` → `OK` | NE | DONE | NE | ANO |
| AT+POWER | SET | `AT+POWER=<dBm>,FORCE` → `+POWER: <dBm>, FORCE` | ekv.: `AT+TXP=<idx 0..15>` → `OK` | NE | DONE | ANO | ANO |
| AT+LW | SET | `AT+LW=<par>,<val>` (`DC`, `CT`, `LDRO`, `THLD`, `NET`, `DUMRX`, `ULDL,<ul>,<dl>`) → `+LW: <par>, <val>[, ...]` | — | — | DONE | ANO - DC, NET, LDRO, THLD, DUMRX | DC, CT, NET, ULDL - ANO / LDRO, THLD, DUMRX - NE |
| AT+LW | GET | `AT+LW=NET` → `+LW: NET, <ON\|OFF>`; `AT+LW=ULDL` → `+LW: ULDL <ul>, <dl>` | — | — | DONE | - | - |
| AT+PORT | SET | `AT+PORT=<1..255>` → `+PORT: <port>` | — (port je součástí `AT+SEND`) | — | DONE | ANO | NE |
| AT+DELAY | SET | `AT+DELAY=RX1\|RX2,<ms>` → `+DELAY: <ms>` | ekv.: `AT+RX1DL=<ms>`, `AT+RX2DL=<ms>` → `OK` | NE | DONE | NE | ANO |
| AT+RXWIN1 | SET | `AT+RXWIN1=<ch>,<freq>` → `+RXWIN1: <ch>,<freq>` | — (jen `AT+RX2FQ=<Hz>`, `AT+RX2DR=<DR>`) | — | DONE(echo) | NE | ANO |
| AT+RETRY | SET | `AT+RETRY=<n>` → `+RETRY: <n>` | — | — | DONE | ANO | NE |
| AT+REPT | SET | `AT+REPT=<1..15>` → `+REPT: <n>` | — | — | DONE | NE | ANO |
| AT+KEY | SET | `AT+KEY=NWKSKEY\|APPSKEY, "<32 hex>"` → `+KEY: <name> <32 hex>` | ekv.: `AT+NWKSKEY=XX:..:XX`, `AT+APPSKEY=XX:..:XX` (16 B s dvojtečkami) → `OK` | NE | DONE | NE | ANO |
| AT+ID | SET | `AT+ID=DevAddr, "<8 hex>"` → `+ID: DevAddr, xx:xx:xx:xx` | ekv.: `AT+DADDR=XX:XX:XX:XX` → `OK` | NE | DONE | NE | ANO |
| AT+MSGHEX | SET | `AT+MSGHEX="<hex>"` → `+MSGHEX: Start`, `+MSGHEX: Done` | ekv.: `AT+SEND=<port>:0:<hex>` → `OK`; události `+EVT:...` | NE | DONE | - | - |
| AT+CMSGHEX | SET | `AT+CMSGHEX="<hex>"` → `+CMSGHEX: Start`, `+CMSGHEX: Wait ACK`, `+CMSGHEX: Done` | ekv.: `AT+SEND=<port>:1:<hex>` → `OK`; události `+EVT:SEND_CONFIRMED` | NE | DONE | - | - |
| AT+TEMP | RUN | `AT+TEMP` → `+TEMP: <°C s 1 desetinným místem>` | — | — | DONE | - | - |
| AT+RTC | SET | `AT+RTC="YYYY-MM-DD HH:MM:SS"` → `+RTC: YYYY-MM-DD HH:MM:SS` | — | — | DONE | NE | NE |
| AT+RTC | GET | `AT+RTC=UTCMS` → `+RTC: YYYY-MM-DD HH:MM:SS.mmm` | ekv.: `AT+LTIME?` → `LTIME:HHhMMmSSs on DD/MM/YYYY` | NE | DONE | - | - |
| AT+EEPROM | GET | `AT+EEPROM=<addr>` → `+EEPROM: <addr>, <val>` (vždy 15 znaků) | — | — | DONE | - | - |
| AT+EEPROM | SET | `AT+EEPROM=<addr>,<val>` → `+EEPROM: <addr>, <val>` | — | — | DONE | - | - |
| AT+TEST | SET | `AT+TEST=RFCFG,...` / `RXLRPKT` / `TXLRPKT,<hex>` → `+TEST: ...` | ekv.: `AT+TCONF=<Freq>:<Pwr>:<BW>:<SF>:<CR>:...` → `OK`; `AT+TTX=<n>`, `AT+TRX=<n>` | NE | DONE | - | - |
| AT+LOWPOWER | RUN | `AT+LOWPOWER` → `+LOWPOWER: SLEEP` | — | — | DONE | - | - |
| AT+LOWPOWER | SET | `AT+LOWPOWER=AUTOON\|AUTOOFF` → `+LOWPOWER: AUTOON` / `AUTOOFF` | — | — | DONE | ANO | NE |

## Poznámky

- AT_Slave mění stav a po SET vrací pouze `OK`, hodnotu nevrací. Prompt vždy očekává echo `+X: <hodnota>`. Pro použití AT_Slave s hostitelským driverem by bylo nutné doplnit výpisy.
- AT_Slave neobsahuje parser parametrů oddělených čárkami; každý handler si parsuje řetězec sám (`tiny_sscanf`).
- U `AT+DADDR` AT_Slave help píše `XXXXXXXX`, ale parser (`sscanf_uint32_as_hhx`) pravděpodobně očekává dvojtečky; neověřeno.
- U `AT+DR` AT_Slave povoluje 0..15 (help string píše 0..7).
