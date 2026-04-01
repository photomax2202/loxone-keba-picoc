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
int gl_iPollStates = 0;
int gl_iChargingAllowedIsChanging;
int gl_iTargetPowerIsChanging;

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

void SendCommand(char *ACommand, int APollStates) {
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
	gl_iPollStates = APollStates;
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
	if ((LPhaseSwitch == 1) && (LCurrent1 > 0 && LCurrent2 > 0 && LCurrent3 > 0)) {
		LThreePhase = 1;
	}
	return LThreePhase;
}

int getCurrHw() {
	char *LInputBuffer = getinputtext(1);
	int LCurrHw = batoi(LInputBuffer);
	return LCurrHw;
}

int getCurrUser() {
	char *LInputBuffer = getinputtext(2);
	int LCurrUser = batoi(LInputBuffer);
	return LCurrUser;
}

int getChargingAllowed() {
	return (int)getinput(11);
}

float getTargetPower() {
	return getinput(12)*1000; // Return TargetPower in W
}

// Set Output Values

void setOutputVehicleConnected(int AVehicleConnected) {
	int LVehicleConnected = 1;
	if (AVehicleConnected == 0) {
		LVehicleConnected = 0;
	} 
	setoutput(0,LVehicleConnected);
}

void setOutputChargingActive(int AChargingActive) {
	int LChargingActive = 1;
	if (AChargingActive == 0) {
		LChargingActive = 0;
	} 
	setoutput(1,LChargingActive);
}

void setEnableSys(int AEnableSys) {
	char LOutputBuffer[10];
	int LEnableSys = 1;
	if (AEnableSys == 0) {
		LEnableSys = 0;
	}
	sprintf(LOutputBuffer,"ena %d",LEnableSys);
	SendCommand(LOutputBuffer,1);
}

void setCurrUser(int ACurrUser) {
	char LOutputBuffer[10];
	sprintf(LOutputBuffer,"curr %d",ACurrUser);
	SendCommand(LOutputBuffer,1);
}

void setX2PhaseSwitch(int APhaseSwitch) {
	gl_iX2PhaseSwitch = APhaseSwitch;
	if (getCountdown(gl_uiPhaseSwitchTime, PHASE_SWITCH_COOLDOWN) > 0)
		return;
	char LOutputBuffer[10];
	sprintf(LOutputBuffer,"X2 %d",gl_iX2PhaseSwitch);
	SendCommand(LOutputBuffer,1);
}

void storePhaseSwitchTime() {
	gl_uiPhaseSwitchTime = getcurrenttime();
}

int calculateCurrUser() {
	float LCurrUser;
	if (getThreePhaseCharging() == 1) {
		LCurrUser = getTargetPower()/(getVoltagePhaseSum()*sqrt(3));
	} else {
		LCurrUser = getTargetPower()/getVoltagePhaseOne();
	}
	LCurrUser = LCurrUser * 1000; // converting A in mA
	// test min and max value
	if (LCurrUser < 6000)
        LCurrUser = 6000;
    else if (LCurrUser > getCurrHw())
        LCurrUser = getCurrHw();
	return (int) LCurrUser;
}

// Main Application
void MainApplication() {
	if ((getCountdown(gl_uiReportTime,UDP_POLL) == 0) || gl_iPollStates) {
		SendCommand("report 2",0);
		SendCommand("report 3",0);
		gl_uiReportTime = getcurrenttime();
	}
	// Plug or State Changed
	if (gl_nEvents & 0x30) {
		setOutputVehicleConnected(getVehicleConnected());
		setOutputChargingActive(getChargingActive());
	}
	// ChargingAllowed Changed
	if ((gl_nEvents & 0x4000) || gl_iChargingAllowedIsChanging) {
		gl_iChargingAllowedIsChanging = 1;
		if (getChargingAllowed() == getEnableSys()) {
			gl_iChargingAllowedIsChanging = 0; 
		} else {
			setEnableSys(getChargingAllowed());
		}
	}
	// TargetPower Changed
	if ((gl_nEvents & 0x8000) || gl_iTargetPowerIsChanging) {
		 gl_iTargetPowerIsChanging = 1;
		 int iNewCurrUser = calculateCurrUser();
		 if (iNewCurrUser == getCurrUser()) {
			gl_iTargetPowerIsChanging = 0; 
		 } else {
			setCurrUser(iNewCurrUser); 
		 } 
	}
	// X2 phase Switch Changed
	if (gl_nEvents & 0x80) {
		gl_uiPhaseSwitchTime = getcurrenttime(); 
	}
}

while(ACTIVE) {
unsigned int uiProgrammTime = getcurrenttime();
	if (gl_iBreakLoop == 0) {
		gl_nEvents = getinputevent();
		gl_iBreakLoop = 0;
	}
	if (gl_iInitialLoop) {
		gl_nEvents = 0x1fff;
		gl_iInitialLoop = 0;
	}
	MainApplication();
	while ((getCountdown(uiProgrammTime, 1) > 0)) {
		gl_nEvents = getinputevent();
		if (gl_nEvents & 0x1fff) {
			gl_iBreakLoop = 1;
			break; 
		}
		sleep(50);
	}
}