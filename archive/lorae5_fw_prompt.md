# LoRa-E5: Přehled příkazů, jejich funkčnosti a očekávané UART odpovědi
Tento dokument obsahuje seznam AT příkazů. Je strukturovaný ve formátu X.Y, kde X je číslo AT příkazu a Y je způsob použití příkazu popsaný custom datovym typem v .C:
```C
typedef enum {
  AT_OP_RUN,              /* AT+NAME        */
  AT_OP_GET,              /* AT+NAME?       */
  AT_OP_SET,              /* AT+NAME=a,b    */
  AT_OP_TEST              /* AT+NAME=?      */
} at_op_t;
```
Při psaní kódu se inspiruj v již vytvořené části kódu. Bude zachována struktura programu. Pro každý command se vytvoří funkce ve formatu cmd_\<NAME>, kde NAME je název commandu. Kód je vyvíjen pro MCU STM32WLE5JC, konkrétně dev kit lorae5 mini (wio).

## 0. PRAVIDLA A POSTUPY

### 0.1 PRAVIDLA
V této části jsou sepsány v jednotlivých bodech hlavní pravidla, kterých je potřeba se vždy držet.
1. Pokus se zachovat kód napsaný v at.c. Pokud by ale z hlediska standardních programátorských praktik, přehlednosti kódu a jednoduchosti dávalo větší smysl kód upravit, nejdříve se zeptej. Nic neupravuj sám bez svolení.
2. Nic necommituj ani nepushuj na git. S gitem vůbec nepracuj.
3. Kód piš pouze do adresáře C:\Users\pesek\Projekty\stm32_lorae5_fw\lorae5_custom_fw.
4. v jednotlivých cmd_\<NAME> funkcích naimplementuj pouze ty funkcionality RUN, GET, SET a nebo TEST, které jsou definované v příslušné sekci AT commandu níže.
5. Implementuj pouze AT commandy popsané v tomto dokumentu. Jsou to právě ty, které používá hostitelský driver iot_lora_driver.cpp (přehled v lorae5_komunikace.md, sekce 5). Žádné další AT commandy z Seeed specifikace (např. AT+VER, AT+MSG, AT+JOIN, AT+VDD) nepřidávej, ani když je znáš. U každého commandu implementuj jen ty operace (RUN/GET/SET/TEST), které driver používá a které jsou v jeho sekci uvedené. Další varianty (např. dotazy, které driver neposílá) neimplementuj.
6. Ty jsi ORCHESTRATOR. To je tvoje role. Ty nepíšeš kód, ty poveluješ jiné subagenty a provádíš kroky v sekci 0.2.

### 0.2 POSTUP - ORCHESTRATOR
1. Vytvoř log soubor progress.log, do kterého budeš po každém kroku z postupu zadávat krátý 1-2 věty dlouhý popis toho co se povedlo a ne. Také do logu budeš zapisovat důležité průběžné informace o progressu.
2. Vytvoř result.md soubor, kde se budou průběžně dokumentovat napsané a úspěšně otestované AT commandy. Ve formátu tabulky, kde bude zalogován každý test(formát, get/set/..., výsledek, možné nefunkčnosti, na které by se mohlo narazit)
3. Nastuduj již vytvořený kód v souborech main.c, at.c, at.h. Nastuduj si strukturu cmd_\<NAME> funkcí každého již napsaného commandu. Této struktury se drž při psaní vlastních cmd_\<NAME> funkcí - pokud by z hlediska standardních programátorských postupů dávalo smysl tuto strukturu změnit, zeptej se na souhlas.
4. Nastuduj si commandy v následujících sekcích.
5. Vytvoř agenta jménem CODER(Haiku). Tento agent bude psát kód. Budeš mu zadávat postupně zadání podle jednotlivých AT commandů - jeden po druhém. Potom, co napíše kód pro jeden command, kód zkontroluješ a případně necháš CODERA kód opravit. Pro každý command CODER vytvoří jeho funkci a zařadí ji do pole ```cmds``` tak, aby bylo možné ji volat v ```handle``` funkci. Pokud to bude dávat podle standardních programátorských postupů smysl, vytvoří pomocné funkce volané ve funkci commandu.
6. Po vytvoření každého at commandu se zavolá TESTER subagent, který přes UART zkontroluje funkčnost AT commandů - Výsledek testu řekne tobě(ORCHESTRATOROVI), který podle toho poveluje znovu CODERA nebo ne - takto se iteruje.
7. Po úspěšném otestování AT commandu se zaloguje do result.md souboru.


### 0.3 SUBAGENTI
1. CODER(Haiku): píše kód, je povelován ORCHESTRATOREM
2. TESTER(Haiku): testuje přes UART AT_commandy, je povelován ORCHESTRATOREM, ORCHESTRATOROVI předává výsledky testů

### 0.4 OBECNÁ PRAVIDLA UART KOMUNIKACE
Platí pro všechny commandy níže, pokud není v sekci commandu uvedeno jinak.
- Každý příkaz i každá odpověď končí "\r\n". V odpovědích níže je "\r\n" vynecháno.
- Hostitel (driver) při kontrole odpovědi ignoruje bílé znaky (mezery), ale ne interpunkci (čárky, dvojtečky). Dodrž proto přesně uvedené čárky a dvojtečky.
- Parametry typu "\<hex>" a klíče se v odpovědi vrací velkými písmeny (A-F).
- Chybová odpověď: "+\<NAME>: ERROR(\<code>)", např. "+TEST: ERROR(-12)". Kódy:
    - -1: neplatný parametr
    - -10: neznámý command
    - -11: command ve špatném formátu
    - -12: command není dostupný v aktuálním módu (např. AT+TEST=... mimo TEST mód)
    - -20: příliš mnoho parametrů (max. 15)
    - -21: příkaz je příliš dlouhý (více než 528 B)
- Operace GET je obecně ve tvaru "AT+NAME?". Výjimkou jsou dotazy s podpříkazem, které driver posílá jako "AT+NAME=\<par>" (AT+LW=NET, AT+LW=ULDL, AT+RTC=UTCMS, AT+EEPROM=\<addr>). Na úrovni parseru se tváří jako SET s jedním parametrem, rozliš je podle počtu parametrů. Totéž platí pro AT+WDT (viz poznámka v sekci 1.2).

### 0.5 TESTOVÁNÍ
Testování provádí TESTER subagent. Tento subagent pro každý at command a jeho typy(SET, GET, ...) provede jednoduchý test, kde bude testovat očekávanou odpověď. Případně, pokud je to v kontextu konkrétního SET at commandu možné, otestuje jeho funkčnost zavoláním GET commandu, který vyčte, zdali byla správně SET commandem nastavena hodnota.

### 0.6 SKRIPTY
Pro buildnutí a nahrání FW na MCU bude TESTER používat skripty níže, jiné skripty se používat NEBUDOU.

*Build FW*:
```bash
$cubeIDE = "C:\ST\STM32CubeIDE_2.2.0\STM32CubeIDE\stm32cubeidec.exe"
$workspace = "C:\Users\pesek\STM32CubeIDE\workspace_2.2.0"
$project = "lorae5_custom_fw"
$config = "Debug"

& $cubeIDE --launcher.suppressErrors -nosplash `
  -application org.eclipse.cdt.managedbuilder.core.headlessbuild `
  -data $workspace `
  -build "$project/$config"
```

*Nahrání(flash) FW na MCU*:
```bash
$cubeProg = "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
$fw="C:\Users\pesek\Projekty\stm32_lorae5_fw\lorae5_custom_fw\Debug\lorae5_custom_fw.hex"
&$cubeProg -c port=SWD mode=UR reset=HWrst freq=1000 -w $fw -v -g
```

### 0.7 WORKFLOW PROGRAMOVÁNÍ
Obecná workflow při psaní kódu a testování AT commandů je následující:
1. ORCHESTRATOR zaúkoluje CODERA pro napsání kodu AT commandu.
2. CODER napíše kód a ORCHESTRATOR ho zkontroluje.
    - Pokud není podle ORCHESTRATORA kod ok(není funkční), tak poveluje CODERA k opravě
    - Pokud je OK, jde se dál
3. ORCHESTRATOR poveluje TESTERA k otestování nového AT commandu
4. TESTER buildne FW a nahraje ho na MCU pomocí skriptů 
4. TESTER reportuje ORCHESTRATOROVI výsledek testů
    - pokud není test OK, tak ORCHESTRATOR poveluje CODERA k opravě kódu
    - pokud je OK, tak se jde dál
5. ORCHESTRATOR poveluje TESTERA ke zdokumentování testů podle formátu popsaného v sekci 0.2 bod 2. - předá mu navíc info o možných nefunkčnostech.

## 1. AT+WDT
*Watchdog funkce*
### 1.1 AT_OP_SET
- Funkcionalita: Zapne nebo vypne watchdog STM32 MCU
- Formát: "AT+WDT=\<par>\r\n"
- parametr par může nabývat hodnot:
    - "ON": zapne se watchdog, UART odpověď ve formátu: "+WDT: ON"
    - "OFF": vypne se watchdog, UART odpověď ve formátu: "+WDT: OFF"

### 1.2 AT_OP_GET
- Zjistí, zdali je watchdog zapnutý nebo vypnutý
- Formát: "AT+WDT?\r\n"
- Poznámka: Hostitelský driver posílá dotaz bez otazníku ("AT+WDT"). Tvar bez otazníku proto musí cmd_WDT zpracovat stejně jako GET.
- Odpověď ve formátu: "+WDT: \<out>\r\n", kde \<out> je buď "ON" nebo "OFF" podle toho, zdali watchdog běží nebo ne.

## 2. AT+LOG
*Nastaví logging na úroveň DEBUG nebo ho vypne*
### 2.1 AT_OP_SET
- Funkcionalita: Nastaví UART logging STM32 na úroveň DEBUG nebo ho vypne.
- Formát: "AT+LOG=\<par>\r\n"
- parametr může nabývat hodnot:
    - "QUIET": vypne se, UART odpověď ve formátu: "+LOG: QUIET"
    - "DEBUG": logging se zapne a nastaví na úroveň debug, UART odpověď ve formátu: "+LOG: DEBUG"
- Poznámka: Použij mimo jiné funkci ```UTIL_ADV_TRACE_SetVerboseLevel()```

## 3. AT
*Test spojení s modulem*
### 3.1 AT_OP_RUN
- Funkcionalita: Pouze ověří, že modul komunikuje.
- Formát: "AT\r\n"
- UART odpověď: "+AT: OK"

## 4. AT+UART
*Nastavení parametrů UARTu*
### 4.1 AT_OP_SET
- Formát: "AT+UART=\<par>,\<val>\r\n"
- parametr par může nabývat hodnot:
    - "BR": nastaví baudrate UARTu. val je jedna z hodnot 9600, 14400, 19200, 38400, 57600, 76800, 115200, 230400. Nový baudrate se uplatní až po AT+RESET (nebo po restartu), do té doby komunikace běží na původním. UART odpověď: "+UART: BR, \<val>", např. "+UART: BR, 115200"
    - "TIMEOUT": nastaví timeout příjmu příkazu v ms (čas od prvního přijatého znaku "AT" do příchodu "\n"). val = 0 timeout vypne. Po vypršení se vypíše "+INFO: Input timeout". UART odpověď: "+UART: TIMEOUT, \<val>", např. "+UART: TIMEOUT, 500"

## 5. AT+RESET
*Softwarový reset modulu*
### 5.1 AT_OP_RUN
- Funkcionalita: Provede softwarový reset MCU.
- Formát: "AT+RESET\r\n"
- UART odpověď: "+RESET: OK". Odpověď se odešle před samotným resetem (po dokončení UART přenosu), poté se MCU resetuje.

## 6. AT+MODE
*Přepnutí pracovního módu*
### 6.1 AT_OP_SET
- Funkcionalita: Přepne mezi LoRaWAN ABP módem a raw LoRa TEST módem. Mód LWABP je výchozí. Příkazy AT+TEST=... jsou dostupné jen v TEST módu (jinak chyba -12).
- Formát: "AT+MODE=\<par>\r\n"
- parametr par může nabývat hodnot:
    - "LWABP": LoRaWAN ABP mód, UART odpověď: "+MODE: LWABP"
    - "TEST": raw LoRa TEST mód, UART odpověď: "+MODE: TEST"

## 7. AT+DR
*Region a data rate*
### 7.1 AT_OP_SET
- Formát: "AT+DR=\<par>\r\n"
- parametr par může nabývat hodnot:
    - "EU868": nastaví region/kanálový plán EU868, UART odpověď: "+DR: EU868"
    - číslo n (0-5 pro EU868, 0-15 syntakticky): nastaví data rate DRn, UART odpověď: "+DR: EU868 DR\<n> SF\<sf> BW125K", kde sf = 12 - n pro n = 0-5. Např. n=3: "+DR: EU868 DR3 SF9 BW125K". Pro n=6: "+DR: EU868 DR6 SF7 BW250K".

## 8. AT+CH
*Nastavení frekvence kanálu*
### 8.1 AT_OP_SET
- Funkcionalita: Nastaví frekvenci a rozsah data rate kanálu n.
- Formát: "AT+CH=\<n>,\<MHz>,DR\<min>,DR\<max>\r\n", např. "AT+CH=0,863.8,DR0,DR5\r\n". Frekvence je v MHz (může obsahovat desetinnou část), n je číslo kanálu (0-95).
- UART odpověď ve formátu: "+CH: \<n>,\<Hz>,DR\<min>:DR\<max>", kde Hz je frekvence v Hz (např. "+CH: 0,863800000,DR0:DR5").

## 9. AT+CLASS
*LoRaWAN třída zařízení*
### 9.1 AT_OP_SET
- Formát: "AT+CLASS=A\r\n"
- Podporovaná je pouze třída "A", UART odpověď: "+CLASS: A"

## 10. AT+ADR
*Adaptive data rate*
### 10.1 AT_OP_SET
- Formát: "AT+ADR=\<par>\r\n"
- parametr par může nabývat hodnot:
    - "ON": ADR zapnuto, UART odpověď: "+ADR: ON"
    - "OFF": ADR vypnuto, UART odpověď: "+ADR: OFF"

## 11. AT+POWER
*Vysílací výkon*
### 11.1 AT_OP_SET
- Funkcionalita: Nastaví pevný vysílací výkon bez ohledu na tabulku výkonů a LinkADRReq.
- Formát: "AT+POWER=\<dBm>,FORCE\r\n"
- UART odpověď ve formátu: "+POWER: \<dBm>, FORCE", např. "+POWER: 14, FORCE"

## 12. AT+LW
*Skupina LoRaWAN řídicích příkazů, první parametr určuje podpříkaz*
### 12.1 AT_OP_SET
- Formát: "AT+LW=\<par>,\<val>\r\n"
- parametr par může nabývat hodnot:
    - "DC": omezení duty cycle (val = "ON" nebo "OFF"). UART odpověď: "+LW: DC, \<val>, 0" (poslední číslo je MaxDCycle), např. "+LW: DC, OFF, 0"
    - "CT": strict LoRaWAN compliance test mód (val = "ON" nebo "OFF"). UART odpověď: "+LW: CT, \<val>"
    - "LDRO": low data rate optimization (val = "ON", "OFF" nebo "AUTO"). UART odpověď: "+LW: LDRO, \<val>"
    - "THLD": práh listen-before-talk v dBm (val od -1 do -140). UART odpověď: "+LW: THLD, \<val>", např. "+LW: THLD, -65"
    - "NET": veřejná/privátní síť (val = "ON" veřejná, "OFF" privátní). UART odpověď: "+LW: NET, \<val>"
    - "DUMRX": vypnutí RX okna po nepotvrzených zprávách (val = "ON" okno vypnuto, "OFF" okno aktivní). UART odpověď: "+LW: DUMRX, \<val>"
    - "ULDL": nastaví čítače rámců, formát "AT+LW=ULDL,\<ul>,\<dl>\r\n". UART odpověď: "+LW: ULDL \<ul>, \<dl>", např. "+LW: ULDL 5, 10"

### 12.2 AT_OP_GET
- Formát: "AT+LW=\<par>\r\n"
- parametr par může nabývat hodnot:
    - "NET": UART odpověď: "+LW: NET, \<out>", kde out je "ON" nebo "OFF"
    - "ULDL": vrátí čítače uplink a downlink rámců. UART odpověď: "+LW: ULDL \<ul>, \<dl>", např. "+LW: ULDL 1, 0"

## 13. AT+PORT
*LoRaWAN port používaný pro odesílání zpráv*
### 13.1 AT_OP_SET
- Formát: "AT+PORT=\<port>\r\n", port je 1-255.
- UART odpověď: "+PORT: \<port>", např. "+PORT: 8"

## 14. AT+DELAY
*Zpoždění RX oken*
### 14.1 AT_OP_SET
- Formát: "AT+DELAY=\<par>,\<ms>\r\n"
- parametr par může nabývat hodnot:
    - "RX1": zpoždění prvního RX okna v ms
    - "RX2": zpoždění druhého RX okna v ms
- UART odpověď: "+DELAY: \<ms>", např. "+DELAY: 1000" (stejný formát pro RX1 i RX2, bez názvu okna - tak ho očekává hostitelský driver)

## 15. AT+RXWIN1
*Frekvence prvního RX okna*
### 15.1 AT_OP_SET
- Formát: "AT+RXWIN1=\<ch>,\<freq>\r\n". freq = 0 znamená použití výchozí frekvence RX1 daného kanálu, nenulová hodnota (MHz) výchozí frekvenci přepíše.
- UART odpověď: "+RXWIN1: \<ch>,\<freq>", např. "+RXWIN1: 0,0"

## 16. AT+RETRY
*Počet pokusů u potvrzených zpráv (CMSGHEX)*
### 16.1 AT_OP_SET
- Formát: "AT+RETRY=\<n>\r\n". Pro n < 2 se odešle jen jedna zpráva, jinak se při chybějícím ACK opakuje n-1 krát s náhodnou prodlevou 3-10 s.
- UART odpověď: "+RETRY: \<n>"

## 17. AT+REPT
*Počet opakování nepotvrzených zpráv (MSGHEX)*
### 17.1 AT_OP_SET
- Formát: "AT+REPT=\<n>\r\n", n je 1-15.
- UART odpověď: "+REPT: \<n>"

## 18. AT+KEY
*Klíče relace (ABP). Klíče nelze číst zpět.*
### 18.1 AT_OP_SET
- Formát: "AT+KEY=\<par>, \"\<key>\"\r\n", key je 16 B jako 32 hex znaků (mezery mezi bajty jsou povolené a ignorují se).
- parametr par může nabývat hodnot:
    - "NWKSKEY": network session key, UART odpověď: "+KEY: NWKSKEY \<key>"
    - "APPSKEY": application session key, UART odpověď: "+KEY: APPSKEY \<key>"
- key se v odpovědi vrací bez mezer, velkými písmeny, např. "+KEY: NWKSKEY 2B7E151628AED2A6ABF7158809CF4F3C"

## 19. AT+ID
*Identifikátory zařízení*
### 19.1 AT_OP_SET
- Formát: "AT+ID=DevAddr, \"\<addr>\"\r\n", addr je 4 B jako 8 hex znaků (big endian).
- UART odpověď: "+ID: DevAddr, xx:xx:xx:xx", např. pro "01234567" je odpověď "+ID: DevAddr, 01:23:45:67"

## 20. AT+MSGHEX
*Odeslání nepotvrzené zprávy*
### 20.1 AT_OP_SET
- Funkcionalita: Odešle LoRaWAN uplink (nepotvrzený) na portu nastaveném AT+PORT, počet opakování určuje AT+REPT.
- Formát: "AT+MSGHEX=\"\<hex>\"\r\n", hex je payload jako hex řetězec (mezery povolené).
- UART odpověď: nejdříve "+MSGHEX: Start", po dokončení vysílání "+MSGHEX: Done". Hostitel čeká na řádek "+MSGHEX: Done" (timeout několik sekund).

## 21. AT+CMSGHEX
*Odeslání potvrzené zprávy*
### 21.1 AT_OP_SET
- Funkcionalita: Odešle LoRaWAN uplink vyžadující potvrzení (ACK) na portu nastaveném AT+PORT, počet pokusů určuje AT+RETRY.
- Formát: "AT+CMSGHEX=\"\<hex>\"\r\n"
- UART odpověď: "+CMSGHEX: Start", "+CMSGHEX: Wait ACK", po dokončení "+CMSGHEX: Done". Hostitel čeká na řádek "+CMSGHEX: Done".

## 22. AT+TEMP
*Teplota modulu*
### 22.1 AT_OP_RUN
- Funkcionalita: Změří teplotu čipem a vrátí ji ve °C.
- Formát: "AT+TEMP\r\n"
- UART odpověď: "+TEMP: \<out>", kde out je číslo s jedním desetinným místem, např. "+TEMP: 20.5". Hostitel parsuje číslo od 7. znaku řádku.

## 23. AT+RTC
*Real-time clock modulu. Po resetu začíná na 2000-01-01 00:00:00.*
### 23.1 AT_OP_SET
- Funkcionalita: Nastaví čas RTC.
- Formát: "AT+RTC=\"YYYY-MM-DD HH:MM:SS\"\r\n". Formát je striktní, rok musí začínat 20xx.
- UART odpověď: "+RTC: YYYY-MM-DD HH:MM:SS", např. "+RTC: 2016-06-14 18:16:11"

### 23.2 AT_OP_GET
- Funkcionalita: Vrátí aktuální čas RTC v UTC včetně milisekund.
- Formát: "AT+RTC=UTCMS\r\n"
- UART odpověď: "+RTC: YYYY-MM-DD HH:MM:SS.mmm", např. "+RTC: 2016-06-14 18:16:11.576". Hostitel čte řetězec od 7. znaku řádku.

## 24. AT+EEPROM
*256 B uživatelských dat. Adresa a hodnota jsou hex, rozsah 00-FF.*
### 24.1 AT_OP_GET
- Formát: "AT+EEPROM=\<addr>\r\n", addr jako 2 hex znaky.
- UART odpověď: "+EEPROM: \<addr>, \<val>", oboje 2 hex znaky velkými písmeny, např. "+EEPROM: 54, AB" (odpověď má vždy délku 15 znaků, hostitel na tom závisí).

### 24.2 AT_OP_SET
- Formát: "AT+EEPROM=\<addr>,\<val>\r\n", addr a val jako 2 hex znaky. Hodnota musí přežít reset.
- UART odpověď: "+EEPROM: \<addr>, \<val>", např. "+EEPROM: 54, AB"

## 25. AT+TEST
*Raw LoRa příkazy. Dostupné jen po AT+MODE=TEST, jinak chyba "+TEST: ERROR(-12)".*
### 25.1 AT_OP_SET
- Formát: "AT+TEST=\<par>[,...]\r\n"
- parametr par může nabývat hodnot:
    - "RFCFG": nastaví RF konfiguraci. Formát: "AT+TEST=RFCFG,\<MHz>,SF\<n>,\<BW kHz>,\<TXPR>,\<RXPR>,\<dBm>,\<CRC>,\<IQ>,\<NET>", např. "AT+TEST=RFCFG,860.00,SF7,500,12,15,22,ON,OFF,OFF" (BW je 125, 250 nebo 500; CRC, IQ, NET jsou ON/OFF). UART odpověď: "+TEST: RFCFG F:\<Hz>, SF\<n>, BW\<kHz>K, TXPR:\<TXPR>, RXPR:\<RXPR>, POW:\<dBm>dBm, CRC:\<CRC>, IQ:\<IQ>, NET:\<NET>", např. "+TEST: RFCFG F:860000000, SF7, BW500K, TXPR:12, RXPR:15, POW:22dBm, CRC:ON, IQ:OFF, NET:OFF"
    - "RXLRPKT": přepne radio do nepřetržitého příjmu. Okamžitá UART odpověď: "+TEST: RXLRPKT". Při každém přijatém paketu se pak asynchronně vypíší dva řádky: "+TEST: LEN:\<len>, RSSI:\<rssi>, SNR:\<snr>" a "+TEST: RX \"\<hex>\"" (hex payload paketu).
    - "TXLRPKT": vyšle jeden raw LoRa paket. Formát: "AT+TEST=TXLRPKT,\<hex>\r\n". UART odpověď: "+TEST: TXLRPKT \"\<hex>\"", po dokončení vysílání "+TEST: TX DONE". Hostitel čeká na řádek "+TEST: TX DONE".

## 26. AT+LOWPOWER
*Spánkový režim*
### 26.1 AT_OP_RUN
- Funkcionalita: Uspí modul. Probudí ho jakýkoli znak přijatý na UART RX (hostitel pak čeká alespoň 5 ms před dalším příkazem).
- Formát: "AT+LOWPOWER\r\n"
- UART odpověď: "+LOWPOWER: SLEEP"

### 26.2 AT_OP_SET
- Formát: "AT+LOWPOWER=\<par>\r\n"
- parametr par může nabývat hodnot:
    - "AUTOON": zapne automatický low-power (modul spí, když je nečinný), UART odpověď: "+LOWPOWER: AUTOON". V tomto režimu hostitel před každým příkazem posílá probouzecí znak 0xFF a modul před každou odpověď také přidává 0xFF; parser s tím musí počítat.
    - "AUTOOFF": vypne automatický low-power, UART odpověď: "+LOWPOWER: AUTOOFF"
