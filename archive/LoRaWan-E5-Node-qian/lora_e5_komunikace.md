# LoRa-E5: přehled komunikace a použitých AT příkazů

Repozitáře (`C:\Users\pesek\Repozitare`):
- **IOTCore** – společná logika, hlavní soubor `IOTCore/src/iot_lora_driver.cpp` (+ `include/iot_lora_driver.h`)
- **IoTIV** – konkrétní zařízení, tenká vrstva `IoTIV/src/iotiv_lora_driver.cpp`

Čísla řádků jsou orientační (stav v době analýzy).

## 0. Shrnutí režimů

| Co | Hodnota |
|---|---|
| Provozní LoRa mód | **LoRaWAN ABP** (`AT+MODE=LWABP`), privátní síť (`AT+LW=NET,OFF`), EU868, třída A, ADR off |
| Pomocný mód | **TEST** (raw LoRa P2P, `AT+MODE=TEST`) – jen pro příjem časového majáku (860 MHz, BW 500 kHz, SF7) |
| AT mód | Klasické textové AT příkazy přes UART (Serial1, 8N1, 115200 baud v IoTIV), synchronně příkaz → očekaná odpověď |
| Odesílání | Ve výchozím stavu nepotvrzené (`AT+MSGHEX`), potvrzené (`AT+CMSGHEX`) jen při `LORA_SEND_CONFIRMED` |
| Auto low-power | Ve výchozí konfiguraci vypnuté (`_lora_low_power_auto_mode=false`) |
| Stavy driveru | `LoraModuleModeE`: NO_MODE_YET, NORMAL, TEST, WAKEUP (`iot_lora_driver.h:19`) |

## 1. Inicializace

| Krok | Metoda | Kde |
|---|---|---|
| Vytvoření driveru (IoTIV) | `IoTIVLoraDriver` ctor | `IoTIV/src/iotiv_core.cpp:45` |
| Propojení závislostí (config, constants, esp driver…) | `core_entity_core_setup()` | `iot_lora_driver.cpp:22`, IoTIV `iotiv_lora_driver.cpp:15` |
| Přeskočení LoRa, když je měření vypnuté | `disable_setup()` | `iot_lora_driver.cpp:643`, volá `IOTCore/src/iot_driver.cpp:91` |
| Otevření UARTu | `hw_setup()` – `Serial1.begin(baud, 8N1, RX, TX)`, timeout `LORA_SERIAL_TIMEOUT_MS` (500 ms) | `iotiv_lora_driver.cpp:23` |
| Napájení + uvolnění resetu přes multiplexer | `power_up_component()` | `iotiv_lora_driver.cpp:31`, volá `core_component.cpp:43` |
| Hlavní setup | `setup_entity()` | `iot_lora_driver.cpp:45` |
| 1) Probuzení / sjednání baudrate | `wake_up_from_low_power_mode()` | `iot_lora_driver.cpp:492` |
| 2) Vypnutí auto low-power | `disable_low_power_auto_mode()` | `iot_lora_driver.cpp:566` |
| 3) Základní parametry (log, UART timeout, watchdog) | `configure_basic_params()` | `iot_lora_driver.cpp:182` |
| 4) Zapnutí LoRaWAN módu | `turn_on_normal_mode()` | `iot_lora_driver.cpp:166` |
| 5) Případné zapnutí auto low-power | `enable_low_power_auto_mode()` | `iot_lora_driver.cpp:552` |
| LoRaWAN parametry (odloženo do prvního odeslání) | `configure_lorawan_params()` | `iot_lora_driver.cpp:199` |
| HW reset modulu (RST pin přes multiplexer) | `hw_reset()` → `reset_lora()` | IoTIV `iotiv_lora_driver.cpp:36`, `iotiv_multiplexer_driver.cpp:110` |

**Průběh `wake_up_from_low_power_mode()`:** `AT` → čeká `+AT: OK`. Když ne: zkouší baudrate 9600…230400, na každém pošle `AT+UART=BR,115200` + `AT+RESET`, přepne ESP na 115200 a znovu `AT`. Při neúspěchu `hw_reset()`; celkem až 5 kol.

**`configure_lorawan_params()`** se spouští při prvním `send_data()`; parametry se do modulu zapisují jen když `AT+LW=NET` neodpoví `OFF` nebo když `boot_count % 10 == 1`. `AT+LW=DUMRX,ON` se posílá pokaždé.

Výchozí hodnoty: `IOTCore/src/iot_config.cpp:44-62`, `iot_constants.cpp:18-62`, IoTIV `iotiv_constants.cpp:82`, `IoTIV/nvs/usr_config.csv:51-56` (`lora_data_rate=3`, `lora_tx_power=14`, 8 frekvencí 863.8–865.2 MHz) a `sys_config.csv:7-8` (klíče).

## 2. Odesílání dat (uplink)

| Krok | Metoda | Kde |
|---|---|---|
| Sestavení protobuf a hex řetězce | `IoTDriver::send_pb_output()` (`to_lora_string`) | `IOTCore/src/iot_driver.cpp` ~r. 700–770 |
| Náhodná prodleva (light sleep) před odesláním | `LORA_MAX_RANDOM_WAIT_BEFORE_SEND_MS` | `iot_driver.cpp:747` |
| Odeslání | `IoTLoraDriver::send_data()` | `iot_lora_driver.cpp:364`, volání `iot_driver.cpp:753` |
| Opětovné odeslání neodeslaných záznamů z NVS (max. 10) | `lora_driver->send_data(hex)` | `IOTCore/src/iot_nvs_manager.cpp:279-311` |

**Průběh `send_data()`:**
1. Kontrola `is_setup_done()`.
2. Pokud modul není v NORMAL, `turn_on_normal_mode()`.
3. Pokud nejsou nastaveny LoRaWAN parametry, `configure_lorawan_params()`.
4. Až 3× `AT+MSGHEX="<hex>"` (nebo `AT+CMSGHEX`), čekání na `+MSGHEX: Done` / `+CMSGHEX: Done` do `MS_FOR_LORA_SEND_CONFIRMATION` (5 s).
5. `reset_frame_counters()` – `AT+LW=ULDL`, při přetečení 16 bitů vynulování.

**Základ komunikace s modulem:**
- `at_command(cmd, expected, timeout)` – `iot_lora_driver.cpp:284`: vyprázdní RX, (při auto low-power pošle 0xFF + 5 ms), pošle příkaz, čte řádky `readStringUntil('\n')`, porovnává s očekávanou odpovědí bez mezer; timeout 1000 ms (`LORA_MODULE_COMMAND_TIMEOUT_MS`).
- `get_modem_answer(cmd, resp, timeout)` – `iot_lora_driver.cpp:339`: pošle příkaz a vrátí první neprázdný řádek bez porovnání; timeout 500 ms.

## 3. Příjem dat (downlink)

- **LoRaWAN downlink se nezpracovává.** Používá se `AT+LW=DUMRX,ON`, RX1/RX2 delay 1 ms / 10 ms. Příjem uživatelských dat tedy neexistuje.
- **Jediný příjem = časový maják v TEST módu:** `get_external_time()` – `iot_lora_driver.cpp:749`
  1. `turn_on_test_mode()` (`:148`) → `AT+MODE=TEST`, `AT+TEST=RFCFG,…`
  2. Volitelně `send_test_packet("T")` (`:124`), jen při `BEACON_REQUEST_ENABLED`
  3. `start_listening_for_test_data()` (`:134`) → `AT+TEST=RXLRPKT`
  4. Smyčka `get_modem_answer("")` + `HelperFunctions::format_beacon_date()` do `MS_BEACON_TIME_TIMEOUT`
  5. Návrat `turn_on_normal_mode()`
- Zaregistrováno jako externí zdroj času: `IoTIV/src/iotiv_driver.cpp:491`, použito v `iot_driver.cpp:660-700` (`sync_timestamp_from_external_source`).

## 4. Ostatní funkce

| Funkce | Metoda | Kde |
|---|---|---|
| Teplota modulu | `get_temperature()` (`AT+TEMP`) | `iot_lora_driver.cpp:449`, volá `iot_driver.cpp:790` |
| Nastavení / čtení RTC modulu | `set_time()`, `get_time()` | `:672`, `:694` |
| EEPROM modulu (čas poslední synchronizace, adresa 0x54) | `write_eeprom()`, `read_eeprom()`, `set/get_last_external_sync_timestamp()` | `:648`, `:657`, `:719`, `:733` |
| Sleep modulu | `put_to_low_power_mode()` | `:475` (v `iotiv_driver.cpp:166` zakomentováno) |
| Wakeup režim / maják | `set_for_device_wakeup()`, `send_wakeup_beacon()` | `:580`, `:595` – **nikdo je nevolá** |

## 5. Používané AT příkazy

| Příkaz | Popis | Kde |
|---|---|---|
| `AT` | Test spojení, odpověď `+AT: OK` | `wake_up_from_low_power_mode` |
| `AT+UART=BR,<baud>` | Nastaví baudrate modulu | `wake_up_from_low_power_mode` |
| `AT+UART=TIMEOUT,<ms>` | Timeout UARTu modulu | `configure_basic_params` |
| `AT+RESET` | Softwarový reset modulu | `wake_up_from_low_power_mode` |
| `AT+WDT`, `AT+WDT=ON` | Dotaz / zapnutí watchdogu | `configure_basic_params` |
| `AT+LOG=DEBUG\|QUIET` | Úroveň logování modulu | `configure_basic_params` |
| `AT+MODE=LWABP` | LoRaWAN ABP mód | `turn_on_normal_mode` |
| `AT+MODE=TEST` | Raw LoRa TEST mód | `turn_on_test_mode` |
| `AT+DR=EU868` | Region/plán kanálů | `configure_lorawan_params` |
| `AT+DR=<n>` | Data rate (SF = 12 − n) | `configure_lorawan_params` |
| `AT+CH=<n>,<MHz>,DR0,DR5` | Frekvence kanálu n | `configure_lorawan_params` |
| `AT+CLASS=A` | LoRaWAN třída A | `configure_lorawan_params` |
| `AT+ADR=OFF` | Vypnutí adaptivního data rate | `configure_lorawan_params` |
| `AT+POWER=<dBm>,FORCE` | Vysílací výkon | `configure_lorawan_params` |
| `AT+LW=DC,OFF` | Vypnutí duty-cycle limitu | `configure_lorawan_params` |
| `AT+LW=CT,OFF` | Vypnutí LoRaWAN test módu | `configure_lorawan_params` |
| `AT+LW=LDRO,OFF` | Low data rate optimization | `configure_lorawan_params` |
| `AT+LW=THLD,-65` | Práh listen-before-talk | `configure_lorawan_params` |
| `AT+LW=NET,OFF` | Privátní síť (dotaz = test, zda je konfigurace v modulu) | `configure_lorawan_params` |
| `AT+LW=DUMRX,ON` | Vypne RX okno u nepotvrzených zpráv (rychlejší uplink) | `configure_lorawan_params` |
| `AT+LW=ULDL` / `AT+LW=ULDL,<ul>,<dl>` | Čtení / reset čítačů rámců | `reset_frame_counters` |
| `AT+PORT=8` | LoRaWAN port | `configure_lorawan_params` |
| `AT+DELAY=RX1,<ms>` / `RX2,<ms>` | Zpoždění RX oken | `configure_lorawan_params` |
| `AT+RXWIN1=0,0` | RX1 okno, výchozí frekvence | `configure_lorawan_params` |
| `AT+RETRY=<n>` | Počet opakování u potvrzených zpráv | `configure_lorawan_params` |
| `AT+REPT=<n>` | Počet opakování u nepotvrzených zpráv | `configure_lorawan_params` |
| `AT+KEY=NWKSKEY,"…"` / `APPSKEY,"…"` | Klíče relace | `configure_lorawan_params` |
| `AT+ID=DevAddr,"…"` | Adresa zařízení | `configure_lorawan_params` |
| `AT+MSGHEX="<hex>"` | Odeslání nepotvrzené zprávy (hex), odpověď `+MSGHEX: Done` | `send_data` |
| `AT+CMSGHEX="<hex>"` | Odeslání potvrzené zprávy, `+CMSGHEX: Done` | `send_data` |
| `AT+TEMP` | Teplota modulu | `get_temperature` |
| `AT+RTC="YYYY-MM-DD HH:MM:SS"` | Nastavení RTC modulu | `set_time` |
| `AT+RTC=UTCMS` | Čtení času z RTC modulu | `get_time` |
| `AT+EEPROM=<addr>` / `AT+EEPROM=<addr>,<data>` | Čtení / zápis EEPROM modulu | `read_eeprom`, `write_eeprom` |
| `AT+TEST=RFCFG,<MHz>,SF<n>,<BW>,12,15,22,ON,OFF,OFF` | Radio konfigurace v TEST módu | `turn_on_test_mode` |
| `AT+TEST=RXLRPKT` | Naslouchání raw LoRa paketům | `start_listening_for_test_data` |
| `AT+TEST=TXLRPKT,<hex>` | Vyslání raw LoRa paketu | `send_test_packet` |
| `AT+LOWPOWER` | Uspání modulu | `put_to_low_power_mode`, `set_for_device_wakeup` (nevolané) |
| `AT+LOWPOWER=AUTOON` / `AUTOOFF` | Zapnutí / vypnutí automatického low-power | `setup_entity` |
