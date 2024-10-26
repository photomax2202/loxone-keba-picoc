# loxone-keba-picoc
PicoC Script for Communicating with Loxone Wallbox Functionblock via UDP 

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
