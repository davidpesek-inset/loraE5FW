function cmd_write {
    param (
        [string]$command
    )
    $port.WriteLine($command)
    $ret = $port.ReadLine()
    Write-Output "$command -> $ret"
    return  
}


$port = New-Object System.IO.Ports.SerialPort("COM5", 9600, "None", 8, "One");
$port.Open();
cmd_write -command "AT"
cmd_write -command "AT+LOWPOWER=AUTOOFF"
cmd_write -command "AT+UART=TIMEOUT,1000"
cmd_write -command "AT+WDT"
cmd_write -command "AT+MODE=LWABP"
cmd_write -command "AT+DR=EU868"
cmd_write -command "AT+DR=2"
cmd_write -command "AT+CH=0,863.8,DR0,DR5"
cmd_write -command "AT+CH=1,864,DR0,DR5"
cmd_write -command "AT+CH=2,864.2,DR0,DR5"
cmd_write -command "AT+CH=3,864.4,DR0,DR5"
cmd_write -command "AT+CH=4,864.6,DR0,DR5"
cmd_write -command "AT+CH=5,864.8,DR0,DR5"
cmd_write -command "AT+CH=6,865,DR0,DR5"
cmd_write -command "AT+CH=7,865.2,DR0,DR5"
cmd_write -command "AT+CLASS=A"
cmd_write -command "AT+ADR=OFF"
cmd_write -command "AT+POWER=18,FORCE"
cmd_write -command "AT+LW=DC,OFF"
cmd_write -command "AT+LW=CT,OFF"
cmd_write -command "AT+LW=LDRO,OFF"
cmd_write -command "AT+LW=THLD,-65"
cmd_write -command "AT+PORT=8"
cmd_write -command "AT+DELAY=RX1,1000"
cmd_write -command "AT+DELAY=RX2,10000"
cmd_write -command "AT+RXWIN1=0,0"
cmd_write -command "AT+RETRY=1"
cmd_write -command "AT+REPT=1"
cmd_write -command 'AT+KEY=NWKSKEY, "2B7E151628AED2A6ABF7158809CF4F3C"'
cmd_write -command 'AT+KEY=APPSKEY, "2B7E151628AED2A6ABF7158809CF4F3C"'
cmd_write -command 'AT+ID=DevAddr, "00181FF1"'
cmd_write -command "AT+LW=DUMRX,ON"
cmd_write -command "AT"

$port.Close();