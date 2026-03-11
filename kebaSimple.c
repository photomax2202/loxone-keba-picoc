/*
Input:
T2 - Curr HW
T3 - Curr user
01 - SetEnergy
02 - State
03 - Plug
04 - Enable Sys
05 - X2 phaseSwitch
06 - U1
07 - U2
08 - U3
09 - I1
10 - I2
11 - I3
12 - ChargingAllowed aus WB2
13 - TargetPower aus WB2
Output:
01 - VehicleConnected an WB2
02 - ChargingActive an WB2
03 - 
04 - 
05 - 
06 - 
07 - 
08 - 
09 - 
10 - 
11 - 
12 - 
13 - 

*/

#define ACTIVE					(1)
#define UDP_POLL 				(10)
#define PHASE_SWITCH_COOLDOWN	(300)

// ***************************************************
// System Configuration
// ***************************************************
char *IP_ADDRESS	= "192.168.98.29";
char *PORT       	= "7090";
int SERIALNO		= 20420166;

// GlobalVariables
int gl_nEvents;
int gl_iBreakLoop = 0;
int gl_iInitialLoop = 1;
int gl_iTargetPowerIsChanging;
int gl_iChargingAllowedIsChanging;
int gl_iPhaseSwitchIsChanging;

int gl_iX2PhaseSwitch;

// Timestamps
unsigned int gl_uiReportTime;
unsigned int gl_uiPhaseSwitchTime;

unsigned int getCountdown(unsigned int startTime, unsigned int durationSec) {
    unsigned int now = getcurrenttime();
    unsigned int elapsed;

    // overflow protected: calculate time difference
    if (now >= startTime)
        elapsed = now - startTime;
    else
        elapsed = 0;   // if time was resetted

    // if more time elapsed than duration
    if (elapsed >= durationSec)
        return 0;

    // Restzeit berechnen
    return durationSec - elapsed;
}

void SendCommand(char *ACommand) {
	char LStreamname[100];
	int LWrittenBytes, i;
	
	// Create Stream
	sprintf(LStreamname, "/dev/udp/%s/%s", IP_ADDRESS, PORT);
	STREAM *UDP_STREAM = stream_create(LStreamname, 0, 0);
	if (UDP_STREAM == NULL)
    {
		errorprintf("Creating Stream failed for %s", LStreamname);
        return;
    }

	// Send Command - try 5 times
	for (i = 0; i < 5; i++)
    {
        LWrittenBytes = stream_write(UDP_STREAM, ACommand, strlen(ACommand));
        stream_flush(UDP_STREAM);

        if (LWrittenBytes == strlen(ACommand))
            break;  // Erfolg
    }
	
	if (LWrittenBytes != strlen(ACommand)){
        errorprintf("Send failed after 5 tries");
        stream_close(UDP_STREAM);
        return;
	}
	
	stream_close(UDP_STREAM);
}

// Has ChangedValue

// Get Input Values Function

int getVehicleConnected() {
    int LConnected = 0;
	int iPlug = (int)getinput(2);
    if (iPlug != 0 &&
        iPlug != 1 &&
        iPlug != 3 &&
        iPlug != 5 &&
        iPlug != 7) {

        return 0;
    }
    // Fahrzeug angeschlossen bei Plug-State 5 oder 7
    if (iPlug == 5 || iPlug == 7) {
        LConnected = 1;
    }
    return LConnected;
}

int getChargingActive() {
    int LChargingActive = 0;
	int iState = (int)getinput(1);
    if (iState != 0 &&
        iState != 1 &&
        iState != 2 &&
        iState != 3 &&
        iState != 4 &&
        iState != 5) {
        return 0;
    }
    // Nur State 3 bedeutet: Laden aktiv
    if (iState == 3) {
        LChargingActive = 1;
    }
    return LChargingActive;
}

int getX2PhaseSwitch() {
	return (int)getinput(4);
}

int getEnableSys() {
	return (int)getinput(3);
}

int getVoltagePhaseOne() {
	return (int)getinput(5);
}

int getVoltagePhaseSum() {
	float LVoltage1 = getinput(5);
	float LVoltage2 = getinput(6);
	float LVoltage3 = getinput(7);
	return (int)(LVoltage1 + LVoltage2 + LVoltage3);
}

int getThreePhaseCharging() {
	int LThreePhase = 0;
	float LCurrent1 = getinput(8);
	float LCurrent2 = getinput(9);
	float LCurrent3 = getinput(10);
	int LPhaseSwitch = getX2PhaseSwitch();
	if ((LPhaseSwitch == 1) && (LCurrent1 >= 1 && LCurrent2 >= 1 && LCurrent3 >= 1)) {
		LThreePhase = 1;
	}
	return LThreePhase;
}

int getCurrHw() {
	char LInputBuffer[10];
	LInputBuffer = getinputtext(1);
	int LCurrHw = batoi(LInputBuffer);
	return LCurrHw;
}

int getCurrUser() {
	char LInputBuffer[10];
	LInputBuffer = getinputtext(2);
	int LCurrUser = batoi(LInputBuffer);
	return LCurrUser;
}

// Set Output Values

void setOutputVehicleConnected(int AVehicleConnectes) {
	setoutput(0,AVehicleConnectes);
}

void setOutputChargingActive(int AChargingActive) {
	setoutput(1,AChargingActive);
}

void setX2PhaseSwitch(int APhaseSwitch) {
	gl_iX2PhaseSwitch = APhaseSwitch;
	if (getCountdown(gl_uiPhaseSwitchTime, PHASE_SWITCH_COOLDOWN) > 0)
		return;
	char LOutputBuffer[10];
	sprintf(LOutputBuffer,"X2 %d",gl_iX2PhaseSwitch);
	SendCommand(LOutputBuffer);
}

// Main Application
void MainApplication() {
	if (getCountdown(gl_uiReportTime,UDP_POLL) == 0) {
		SendCommand("report 2");
		SendCommand("report 3");
		gl_uiReportTime = getcurrenttime();
	}
	// Plug or State Changed
	if (gl_nEvents & 0x6) {
		printf("Evaluate States");
		setOutputVehicleConnected(getVehicleConnected());
		setOutputChargingActive(getChargingActive());
	}
	// ChargingAllowed Changed
	if ((gl_nEvents & 0x800) || gl_iChargingAllowedIsChanging) {
		gl_iChargingAllowedIsChanging = 1; 
		gl_iChargingAllowedIsChanging = 0; 
	}
	// TargetPower Changed
	if ((gl_nEvents & 0x1000) || gl_iTargetPowerIsChanging) {
		 gl_iTargetPowerIsChanging = 1;
		 gl_iTargetPowerIsChanging = 0;
	}
	// X2 phase Switch Changed
	if ((gl_nEvents & 0x10) || gl_iPhaseSwitchIsChanging) {
		 gl_iPhaseSwitchIsChanging = 1;
		 gl_iPhaseSwitchIsChanging = 0;
	}
}

while(ACTIVE) {
unsigned int uiProgrammTime = getcurrenttime();
	gl_nEvents = getinputevent();
	if (gl_iInitialLoop) {
		gl_nEvents = 0x1fff;
		gl_iInitialLoop = 0;
	}
	printf("getinputevent: %d", gl_nEvents);
	// MainApplication();
	while ((getCountdown(uiProgrammTime, 1) > 0)) {
		gl_nEvents = getinputevent();
		if (gl_nEvents & 0x1fff) {
			break; 
		}
		sleep(50);
	}
}