# loxone-keba-picoc
PicoC Script for Communicating with Loxone Wallbox Functionblock via UDP 

## References
Keba Download Section
https://www.keba.com/de/emobility/service-support/downloads/downloads
- UDP Programmers guide

Loxone Documentation
https://www.loxone.com/dede/kb/api-ports-domains/
- API Connector

## Knowing Issues
- Keba phase Switch needs 5 minutes cooldown time
- Loxone API Commands with type Number or Boolenan will not accepted by Function Block connector. Walkarround: Connect unused "Merker" with type of Input Connector.
- Loxone API Command can not get Values from Function Block. They need a direct Connection to Program Block input.

## Example Screenshot
<img src="Screenshot_Loxone Config.jpg" width="800"/>
External Status Block is for Logging Commands by Tracker object.

## Compiler Options/Macros
See Compiler Option Switches for your own Configuration.

## Output
- Txt1 sends API Commands to Wb1 FunctionBlock.
- O1 Value of SetEnergy in kWh
- O2 PhaseSwitch State
- O3 PhaseSwitch Source
- O4 Cooldown Time to next possible switch
- O5 Coolout Time constant maximum

## Input
Receiving Data directly from API-Connecter actually not working.
- T1 Text input reserved for API-Connector
- I1 SetEnergy limits the charging for kWh 
- I12 receives "ChargingAllowed" Signal from Wb1 FunctionBlock.
- I13 receives "TargetChargingPower" Signal from Wb1 FunctionBlock.
  Change connection to API Connector if Possible

## Features
- Read Values from Keba P30 over UDP (Meter Value, Current Charging Power, Vehicle Connected [calculated by logic], Charging active [calculated by logic])
- Enable charging by input 
- Phaseswitch triggered by target power value
- SetEnergy-Controlling by input

## Future Features (maybe)
- Display text line value on Wallbox
- Read RFID-Tagy from Wallbox
