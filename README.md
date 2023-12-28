# loxone-keba-picoc
PicoC Script for Communicating with Loxone Wallbox Functionblock via UDP 

## Compiler Macro
- DEBUG_OUTPUT controls debug output to def.log
- STREAM_ADRESS Streamadress with local IP
- SERIAL_NO Sn-No for comparing serial from id Report

## Output
- Txt1 sends API Commands to Wb1 FunctionBlock.
## Input
- Receiving Data directly from API-Connecter actually not working.
- I1 receives "ChargingAllowed" Signal from Wb1 FunctionBlock.
- I2 receives "TargetChargingPower" Signal from Wb1 FunctionBlock.
