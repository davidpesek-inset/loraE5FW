# Port LoRaWAN_AT_Slave (CubeWL 1.4.0, NUCLEO-WL55JC) na LoRa-E5 mini (STM32WLE5JC)

Návod, jak zopakovat úpravy, které převedly příklad `LoRaWAN_AT_Slave` z NUCLEO-WL55JC
(dual-core STM32WL55JC) na Seeed LoRa-E5 mini (single-core STM32WLE5JC).

Cesty níže jsou relativní k `LoRaWAN_AT_Slave/` (příklad v balíku), kořen balíku označuji `<CUBEWL>`
(`STM32Cube_FW_WL_V1.4.0`). Reference pro E5: projekt `LoRaWan-E5-Node-qian` (port `LoRaWAN_End_Node`
na starším CubeWL 1.0/1.1).

Výsledný stav: UART pro AT příkazy = **USART1 PB6/PB7, 115200 8N1**, Stop mód = **Stop1**.

---

## 0. Předpoklady

- Příklad musí ležet na původním místě v balíku:
  `<CUBEWL>\Projects\<BOARD>\Applications\LoRaWAN\LoRaWAN_AT_Slave\`.
  Odkazy v `STM32CubeIDE/.project` jsou `PARENT-6-PROJECT_LOC/...` a počítají s hloubkou 6 úrovní
  od složky `STM32CubeIDE` ke kořeni balíku. Pokud projekt zkopírujete jinam, linkované `.c` soubory
  (Utilities, Drivers, Middlewares) se nenajdou (`No rule to make target 'C:/Utilities/...'`).
- Ve složce `STM32CubeIDE/` nesmí ležet fyzické kopie `Utilities/`, `Drivers/`, `Middlewares/`
  (v originále jsou to jen virtuální linked složky). Jinak CubeIDE kompiluje i `*_if_template.c`
  a vznikají duplicitní symboly.
- Před začátkem zazálohujte celý příklad.

## 1. BSP pro LoRa-E5 mini

1. Zkopírujte `LoRaWan-E5-Node-qian/Drivers/BSP/STM32WLxx_LoRa_E5_mini/` do
   `<CUBEWL>/Drivers/BSP/STM32WLxx_LoRa_E5_mini/`.
2. Doplňte funkci, kterou CubeWL 1.4.0 vyžaduje (qian BSP je ze starší verze a ji nemá):
   - `stm32wlxx_LoRa_E5_mini_radio.h`:
     - enum `BSP_RADIO_RFOMaxPowerConfig_TypeDef { RADIO_RFO_LP_MAXPOWER = 0, RADIO_RFO_HP_MAXPOWER }`
     - `#define RADIO_CONF_RFO_HP_MAX_22_dBm ((int32_t) 22)` a `RADIO_CONF_RFO_LP_MAX_15_dBm ((int32_t) 15)`
     - prototyp `int32_t BSP_RADIO_GetRFOMaxPowerConfig(BSP_RADIO_RFOMaxPowerConfig_TypeDef Config);`
   - `stm32wlxx_LoRa_E5_mini_radio.c`: implementace (vzor je `stm32wlxx_nucleo_radio.c`):
     LP vrací 15 dBm, jinak 22 dBm.
   - Bez toho `radio_board_if.c` (`RBI_GetRFOMaxPowerConfig`) nelinkuje.
3. E5 mini má jen High Power PA: `BSP_RADIO_GetTxConfig()` vrací `RADIO_CONF_RFO_HP`,
   RF switch řídí PA4 (CTRL1) a PA5 (CTRL2), TCXO i DC-DC jsou podporované (už v BSP).

## 2. Projektové soubory CubeIDE (`STM32CubeIDE/`)

### `.cproject` (Debug i Release)
- `STM32WL55JCIx` → `STM32WLE5JCIx`
- `value="NUCLEO-WL55JC1"` (volba Board) → `value="genericBoard"`
- `|| NUCLEO-WL55JC1 ||` (v řetězci Defaults) → `|| STM32WLE5JCIx ||`
- symbol `STM32WL55xx` → `STM32WLE5xx` (`CORE_CM4` a `USE_HAL_DRIVER` zůstávají)
- `STM32WL55JCIX_FLASH.ld` → `STM32WLE5JCIX_FLASH.ld`
- include cesta `Drivers/BSP/STM32WLxx_Nucleo` → `Drivers/BSP/STM32WLxx_LoRa_E5_mini`

### `.project`
- linky na BSP: `STM32WLxx_Nucleo/stm32wlxx_nucleo.c` a `..._nucleo_radio.c` přepsat na
  `STM32WLxx_LoRa_E5_mini/stm32wlxx_LoRa_E5_mini.c` a `..._LoRa_E5_mini_radio.c`
  (jméno i `locationURI`).

Příklad sed příkazů (spusťte v `STM32CubeIDE/`):

```bash
sed -i -e 's/value="NUCLEO-WL55JC1"/value="genericBoard"/g' \
       -e 's/|| NUCLEO-WL55JC1 ||/|| STM32WLE5JCIx ||/g' \
       -e 's/STM32WL55JCIx/STM32WLE5JCIx/g' \
       -e 's/STM32WL55JCIX_FLASH\.ld/STM32WLE5JCIX_FLASH.ld/g' \
       -e 's/STM32WL55xx/STM32WLE5xx/g' \
       -e 's#Drivers/BSP/STM32WLxx_Nucleo#Drivers/BSP/STM32WLxx_LoRa_E5_mini#g' .cproject
sed -i -e 's#STM32WLxx_Nucleo/stm32wlxx_nucleo_radio\.c#STM32WLxx_LoRa_E5_mini/stm32wlxx_LoRa_E5_mini_radio.c#g' \
       -e 's#STM32WLxx_Nucleo/stm32wlxx_nucleo\.c#STM32WLxx_LoRa_E5_mini/stm32wlxx_LoRa_E5_mini.c#g' \
       -e 's#STM32WLxx_Nucleo#STM32WLxx_LoRa_E5_mini#g' .project
```

### Startup a linker script
- `Application/User/Startup/startup_stm32wl55jcix.s` smazat a nahradit souborem
  `<CUBEWL>/Drivers/CMSIS/Device/ST/STM32WLxx/Source/Templates/gcc/startup_stm32wle5xx.s`
  uloženým jako `Application/User/Startup/startup_stm32wle5jcix.s`
  (WLE5 nemá IPCC ani C2SEV vektory).
- `STM32WL55JCIX_FLASH.ld` přejmenovat na `STM32WLE5JCIX_FLASH.ld` (v komentáři `STM32WL55xC` → `STM32WLE5xC`).
  Paměťové rozložení nechte z originálu (RAM1 32K, NVM_RAM 4K, RAM2 28K, FLASH 248K, USER_Key region),
  odpovídá WLE5JC (256 KB flash, 64 KB RAM) a je v něm rezervované místo pro LoRaWAN NVM.

## 3. Úpravy zdrojového kódu (`Core/`, `LoRaWAN/`)

### Piny (`Core/Inc/main.h`, `Core/Src/gpio.c`)
Nahradit blok `Private defines` (zachovat `RTC_*`):

```c
#define USART_BAUDRATE 115200
#define LED1_Pin GPIO_PIN_5
#define LED1_GPIO_Port GPIOB
/* LoRa-E5 mini has a single user LED: TX/RX/join indications share LED1 */
#define LED2_Pin LED1_Pin
#define LED2_GPIO_Port LED1_GPIO_Port
#define LED3_Pin LED1_Pin
#define LED3_GPIO_Port LED1_GPIO_Port
#define BUT1_Pin GPIO_PIN_13
#define BUT1_GPIO_Port GPIOB
#define RF_CTRL1_Pin GPIO_PIN_4
#define RF_CTRL1_GPIO_Port GPIOA
#define RF_CTRL2_Pin GPIO_PIN_5
#define RF_CTRL2_GPIO_Port GPIOA
#define USARTx_RX_Pin GPIO_PIN_7
#define USARTx_RX_GPIO_Port GPIOB
#define USARTx_TX_Pin GPIO_PIN_6
#define USARTx_TX_GPIO_Port GPIOB
```

`MX_GPIO_Init()`: zapnout hodiny GPIOA a GPIOB, nakonfigurovat jen LED1 (výstup) a BUT1 (vstup s pull-up,
bez EXTI, aplikace tlačítka nepoužívá). `lora_app.c` pak beze změny používá LED1/2/3 (všechny = PB5).

### Platform a BSP conf
- `Core/Inc/platform.h`: includy `stm32wlxx_nucleo_radio.h` / `stm32wlxx_nucleo.h` →
  `stm32wlxx_LoRa_E5_mini_radio.h` / `stm32wlxx_LoRa_E5_mini.h`.
- Smazat `Core/Inc/stm32wlxx_nucleo_conf.h`, přidat `Core/Inc/stm32wlxx_LoRa_E5_mini_conf.h`
  (kopie z `LoRaWan-E5-Node-qian/.../Core/Inc/`).

### Probe piny (`Core/Inc/sys_debug.h`, `Core/Src/sys_debug.c`)
PB12/PB13 jsou na NUCLEO debug probe, na E5 mini je PB13 tlačítko.
- `sys_debug.h` před `#if !defined (DISABLE_PROBE_GPIO)` přidat
  `#ifndef DISABLE_PROBE_GPIO` / `#define DISABLE_PROBE_GPIO` / `#endif`.
- `sys_debug.c` (`DBG_Init`): blok s `PROBE_LINE1/2_CLK_ENABLE`, `HAL_GPIO_Init` a resetem pinů obalit
  `#if !defined (DISABLE_PROBE_GPIO)` ... `#endif`.

### UART: LPUART1 → USART1
Nutné jen pokud AT příkazy chodí do USART1 (PB6/PB7), jak to dělá E5 mini s ESP/USB mostem.
Soubory `Core/Src/usart.c`, `Core/Inc/usart.h`, `Core/Src/usart_if.c`, `Core/Src/stm32wlxx_it.c`, `Core/Inc/stm32wlxx_it.h`:

| Původní | Nové |
|---|---|
| `hlpuart1`, `hdma_lpuart1_tx` | `huart1`, `hdma_usart1_tx` |
| `MX_LPUART1_UART_Init` | `MX_USART1_UART_Init` |
| `LPUART1`, `LPUART1_IRQn`, `LPUART1_IRQHandler`, `__HAL_RCC_LPUART1_*` | `USART1`, `USART1_IRQn`, `USART1_IRQHandler`, `__HAL_RCC_USART1_*` |
| `RCC_PERIPHCLK_LPUART1`, `Lpuart1ClockSelection = RCC_LPUART1CLKSOURCE_LSE` | `RCC_PERIPHCLK_USART1`, `Usart1ClockSelection = RCC_USART1CLKSOURCE_HSI` |
| `GPIO_AF8_LPUART1` | `GPIO_AF7_USART1` |
| `DMA_REQUEST_LPUART1_TX` | `DMA_REQUEST_USART1_TX` |
| GPIOA (PA2/PA3) | GPIOB (PB6/PB7): `__HAL_RCC_GPIOB_CLK_ENABLE`, `HAL_GPIO_Init(GPIOB,...)`, `HAL_GPIO_DeInit(GPIOB,...)` |
| `BaudRate = 9600` | `BaudRate = USART_BAUDRATE` |
| `LL_EXTI_LINE_28` (wakeup LPUART1) | `LL_EXTI_LINE_26` (wakeup USART1) |

Dále:
- V `usart.c` smazat volání `HAL_DMA_ConfigChannelAttributes(..., DMA_CHANNEL_NPRIV)`: WLE5 HAL ho nemá
  (build jinak padá na implicit declaration a `DMA_CHANNEL_NPRIV` undeclared).
- `vcom_ReceiveInit()` ponechat (wakeup na start bit, `UART_IT_WUF`, `HAL_UARTEx_EnableStopMode`).

### Hodiny a nízká spotřeba
- `Core/Src/main.c`, `SystemClock_Config()`: do `RCC_OscInitStruct.OscillatorType` přidat `RCC_OSCILLATORTYPE_HSI`
  a nastavit `HSIState = RCC_HSI_ON`, `HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT`
  (HSI16 je kernel clock USART1).
- `Core/Src/stm32_lpm_if.c`:
  - `PWR_EnterStopMode`: `HAL_PWREx_EnterSTOP2Mode` → `HAL_PWREx_EnterSTOP1Mode` (USART1 neprobouzí ze Stop2).
  - `PWR_ExitStopMode`: před `vcom_Resume()` přidat
    `__HAL_RCC_HSI_ENABLE(); while (__HAL_RCC_GET_FLAG(RCC_FLAG_HSIRDY) == RESET) {}`
    (HW vypíná HSI při vstupu do Stopu).

### Radio (`LoRaWAN/Target/`)
Beze změny: `radio_board_if.c` volá BSP (`BSP_RADIO_GetTxConfig`, `IsTCXO`, `IsDCDC`, `GetRFOMaxPowerConfig`).
TCXO napětí (`TCXO_CTRL_1_7V`) je shodné s NUCLEO.

## 4. Build

Z CubeIDE: Project → Clean + Build. Z příkazové řádky:

```powershell
$cubeIDE = "C:\ST\STM32CubeIDE_2.2.0\STM32CubeIDE\stm32cubeidec.exe"
$workspace = "C:\Users\pesek\STM32CubeIDE\workspace_2.2.0"
& $cubeIDE --launcher.suppressErrors -nosplash `
  -application org.eclipse.cdt.managedbuilder.core.headlessbuild `
  -data $workspace -build "LoRaWAN_AT_Slave/Debug"
```

Očekávaný výsledek: 0 chyb, `text` ≈ 93 kB, `bss` ≈ 15 kB. Varování "LOAD segment with RWX permissions"
od linkeru je neškodné. Po změně `.cproject`/`.project` smažte `Debug/` a nechte makefiley přegenerovat.

---

## 5. Co může v budoucnu zlobit

1. **Stop1 místo Stop2.** USART1 neumí probouzet ze Stop2 (jen LPUART1), proto je použit Stop1 s vyšší
   spotřebou ve spánku o jednotky µA. Toto je znalost z RM0461 a nebylo ověřeno měřením; zkontrolujte
   v manuálu a změřte. Pokud záleží na spotřebě, vraťte LPUART1 (PA2/PA3, LSE, Stop2), pokud je na desce vyvedený.
2. **První znak po probuzení.** Při 115200 Bd je start bit ~9 µs, MCU se ze Stopu rozbíhá. První bajt může
   dorazit poškozený. Posílejte budicí bajt (`0x00`/`0xFF`) před příkazem nebo snižte baudrate.
3. **HSI16 trvale zapnutý v Run módu** zvyšuje spotřebu. Při HSI jako kernel clocku se po každém Stopu
   musí ručně znovu zapnout (viz `PWR_ExitStopMode`); pokud někdo tento kód odstraní, UART po prvním spánku přestane přijímat.
4. **Piny UARTu (PB6/PB7)** potvrzuje tabulka v README projektu LoRaWan-E5-Node-qian (E5 mini/Dev Board:
   USART1 PB6/PB7, LED PB5, BUT1 PB13, RF_CTRL1/2 PA4/PA5, DBG1 PA0, DBG2 PB10, DBG3 PB3, DBG4 PB4).
   BSP (`COM1`) ale dál popisuje LPUART1 PA2/PA3 (nepoužito, `USE_BSP_COM_FEATURE 0`).
   Probe piny jsou vypnuté; chcete-li je zapnout, přemapujte `PROBE_LINE1/2` na PA0 a PB10 (ne PB12/PB13).
5. **Sdílená LED.** `LED2` a `LED3` jsou aliasy LED1, takže TX/RX/join blikají na jedné LED a mohou se překrývat.
6. **Výkon 22 dBm.** Hodnota je převzatá z NUCLEO, ne z E5 specifikace. Reálný výkon omezuje matching obvod,
   a pro EU868 platí limit 14 dBm ERP, který musí řešit LoRaWAN region/ADR, ne BSP.
7. **Linkované soubory a hloubka složky.** Přesun projektu mimo balík (nebo jiná hloubka) rozbije
   `PARENT-6-PROJECT_LOC`. Při reimportu ověřte, že ve `STM32CubeIDE/` nevznikly fyzické složky
   `Utilities`, `Drivers`, `Middlewares`, `Application/...` navíc (kompilují se pak `*_template.c`
   a dochází k duplicitám).
8. **Jiná verze CubeWL.** Qian BSP je ze starší verze a po přechodu na jiné CubeWL může chybět další funkce
   (jako `BSP_RADIO_GetRFOMaxPowerConfig`). Chyba se projeví na linkeru, ne při kompilaci.
9. **Regenerace z CubeMX / `.ioc`.** `LoRaWAN_AT_Slave.ioc` je pořád pro WL55 a NUCLEO. Regenerace kódu
   by přepsala většinu těchto úprav (piny, USART1, GPIO). Úpravy proto drží ručně, `.ioc` nepoužívejte
   bez předchozí zálohy.
10. **EWARM a MDK-ARM projekty** nebyly upraveny a jsou dál pro WL55/NUCLEO. Slouží jen CubeIDE.
11. **Dual-core zbytky.** Linker script má sekce `RAM1_region`/`RAM2_region` a `NVM_RAM` z WL55; WLE5
    je jednojádrové a funguje s nimi, ale pokud změníte velikost flash/NVM, upravte `FLASH` a `USER_Key_region_ROM` zároveň.
