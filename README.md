# loxone-keba-picoc
PicoC Script for Communicating with Loxone Wallbox Functionblock via UDP 

## Compiler Macro
- DEBUG_OUTPUT controls debug output to def.log
- STREAM_ADRESS Streamadress with local IP
- SERIAL_NO Sn-No for comparing serial from id Report
- POWER_TH 4200 Power Thereshold Phase switch in W
- COOLDOWN 300 s Cooldown Time after PhaseSwitch in s (Keba Manual)
- MULTI_CONTROL enable/disable Charging if ChargingAllowed is not charging active

## Output
- Txt1 sends API Commands to Wb1 FunctionBlock.
- O1 Value of SetEnergy in kWh
- O2 PhaseSwitch State
- O3 PhaseSwitch Source
- O4 Cooldown Time to next possible switch
## Input
- Receiving Data directly from API-Connecter actually not working.
- T1 Text input reserved for API-Connector
- I12 receives "ChargingAllowed" Signal from Wb1 FunctionBlock.
- I13 receives "TargetChargingPower" Signal from Wb1 FunctionBlock.
  Change connection to API Connector if Possible
- I1 SetEnergy limits the charging for kWh 
