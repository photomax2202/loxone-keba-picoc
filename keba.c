/*
Integration Keba P30 Wallbox über UDP Kommunikation
Senden der API Output Signale als kombinierter String.

Signal Source > get value from UDP-Report > send to API-Connector
Vc	- report 2 > Plug >= 5
Cp	- report 3 > P
Mr	- report 3 > E total
Cac	- report 2 > Enable sys

Input Value from Wb2 FB I/O Connector
I1	- SetEnergy in kWh
I12	- Enable Charging
I13	- Target Power in kW
Using API-Connector if possible - GETOUTPUT Function not implemented in Loxone API
T1	- Reserved for API-Input
Output Values
Txt1	- Output for Wb2 API-Connector
O1		- SetEnergy Value in kWh
O2		- PhaseSwitch State
O3		- PhaseSwitch Source
O4		- Cooldown Time to next switch
O5		- Cooldown Time Constants
O6      - Sec Timestamp From UDP Report 2
O7      - Sec Timestamp From UDP Report 3
O8      - Timer for PhaseSwitch On Actual
O9      - Timer for PhaseSwitch On Maximum
O10     - Timer for PhaseSwitch Off Actual
O11     - Timer for PhaseSwitch Off Maximum

Function:
	Polling "report 2" & "report 3" every 5s with 1s offset between reports.
	Setup with UDP_POLL

Knowing Issues:
-	Vc Signal is pushed to API-Connector but Wb2 FB does not process correctly
	Walkarround:
	Connect Wb2 Inputs to unused Merker-Objects
*/
// server-address und port bitte anpassen
// define Constants
// #define SERIAL_NO 21464391
// #define STREAM_ADRESS "/dev/udp/192.168.98.28/7090"
#define SERIAL_NO 20420166
#define STREAM_ADRESS "/dev/udp/192.168.98.29/7090"
#define BUFF_SIZE 512
#define POWER_TH_ON 4200 //Power Thereshold Phase switch ON in W after time constant
#define POWER_TH_OFF 4100 //Power Thereshold Phase switch OFF in W after time constant
#define POWER_TH_ON_DIRECT 6000 //Power Thereshold Phase switch ON in W directly
#define POWER_TH_OFF_DIRECT 2000 //Power Thereshold Phase switch OFF in W directly
#define POWER_TH_ON_TIME 30 //Time Constant for Phase switch ON in s
#define POWER_TH_OFF_TIME 30 //Time Constant for Phase switch ON in s
#define COOLDOWN 300 //Cooldown Time after PhaseSwitch in s (Keba Manual)
#define UDP_POLL 10 //Time for polling reports
#define CONV_POWER 1000000 // Constant for Convert Power mW <> kW
#define CONV_ENERGY 10000 // Constant for Convert Energy 0.1 mWh <> kWh
#define CONV_CURRENT 1000 // Constant for Convert Current 1000 mA <> A
#define CONV_PF 10 // Constant for Convert Power Factor 0..1000 <> 0..100%
#define DEBUG_OUTPUT false
// #define MULTI_CONTROL true // Wallbox can be Controlled by second source
#define MULTI_CONTROL false // Wallbox will stop charging if not allowed
//#define COM_ACTIVE false // Programm not active
#define COM_ACTIVE true // Programm active 

// define global variabels
int nEvents;
// API Out Variables
int valueVc;
float valueCp;
float valueMr;
int valueCac;
// API In Variables
int valueCa;
float valueTp;
int valueCaLast;
float valueTpLast;
// Report Variabeles
int iReportId;
float iTimeSecReport2;
float iTimeSecReport3;
int iDeviceSerial;
float valueVoltage1;
float valueVoltage2;
float valueVoltage3;
float valueCurrent1;
float valueCurrent2;
float valueCurrent3;
// Variabeles phaseSwitch
int valuePhaseSwitch;
int valuePhaseSwitchLast;
int valuePhaseSwitchSrc;
int pushPhaseSwitchSrc;
// Variabeles userCurrent
float pushUserCurr;
float pushUserCurrLast;
float valueUserCurr;
float valueUserCurrMax;
// Variabeles setEnergy
float pushSetEnergy;
float pushSetEnergyLast;
float valueSetEnergy;
// Timestamp Variabeles
unsigned int iTimeReport2;
unsigned int iTimeReport3;
unsigned int iTimePhSw;
unsigned int iTimePhSwOn;
unsigned int iTimePhSwOff;

// init global objects
STREAM* udpStream = stream_create(STREAM_ADRESS,0,0); // create udp stream
valueCa = 0;
valueCac = 0;
pushPhaseSwitchSrc = 0;
pushSetEnergy = 99;	// Initial Value for pushing new value
iTimeReport2 = getcurrenttime();
iTimeReport3 = getcurrenttime() + 5;
iTimePhSw = getcurrenttime() - 270;
iTimePhSwOn =  getcurrenttime();
iTimePhSwOff =  getcurrenttime();

void sendBuffer(char *strAOutputBuffer) {
	stream_write(udpStream,strAOutputBuffer,strlen(strAOutputBuffer)); // write to output buffer
	stream_flush(udpStream); // flush output buffer
	if(DEBUG_OUTPUT)
	{
		printf("Flush UDP-Stream: %s",strAOutputBuffer);
	}
}

unsigned int calcTimer(unsigned int ATimeStamp,unsigned int ATimeBase){
	unsigned int LTimer;
	unsigned int LCurrentTime;
	unsigned int LTimeDifference;
	LCurrentTime = getcurrenttime();
	LTimeDifference = LCurrentTime-ATimeStamp;
	if (LTimeDifference > ATimeBase) {
		LTimeDifference = ATimeBase;
	}
	LTimer = ATimeBase - LTimeDifference;
	if(DEBUG_OUTPUT)
	{
		printf("Timer: %d with Base: %d",LTimer,ATimeBase);
	}
	return LTimer;
}

float f_extractValueFromReport(char *str,char *strfind)
{
	char strBufferOne[BUFF_SIZE];
	char strBufferTwo[BUFF_SIZE];
	char strBufferOut[BUFF_SIZE];
	char* p;
	
	strBufferOne = strstrskip(str,strfind);
	strBufferTwo = strstr(strBufferOne,"\n");
	strncpy(strBufferOut,strBufferOne,strlen(strBufferOne)-strlen(strBufferTwo));
	p = strstr(strBufferOut,",");
	if (p != NULL)	{
		strBufferOne[0] = 0;
		strBufferOne = strBufferOut;
		strBufferTwo[0] = 0;
		strBufferOut[0] = 0;
		strBufferTwo = strstr(strBufferOne,",");
		strncpy(strBufferOut,strBufferOne,strlen(strBufferOne)-strlen(strBufferTwo));
		p = strstr(strBufferOut,"\"");
		if (p != NULL) {
			strBufferOne[0] = 0;
			strBufferOne = strBufferOut;
			strBufferTwo[0] = 0;
			strBufferOut[0] = 0;
			strBufferTwo = strstr(strBufferOne,"\"");
			strncpy(strBufferOut,strBufferOne,strlen(strBufferOne)-strlen(strBufferTwo));
		}
	}
	if(DEBUG_OUTPUT)
	{
		printf("Index: %s Value: %s",strfind,strBufferOut);
	}
	return batof(strBufferOut);
}

int i_extractValueFromReport(char *str,char *strfind)
{
	return (int)f_extractValueFromReport(str,strfind);
}

void flushApiOutput(int AValueCac, int AValueVc, float AValueCp, float AValueMr)
{
	char strOutputText[BUFF_SIZE];
	
	sprintf(strOutputText,"SET(Wb2;Cac;%d)&SET(Wb2;Vc;%d)&SET(Wb2;Cp;%f)&SET(Wb2;Mr;%f)",AValueCac,AValueVc,AValueCp,AValueMr);
	if(DEBUG_OUTPUT)
	{
		printf("API-Output Command: %s",strOutputText);
	}
	setoutputtext(0,strOutputText);
}

void getApiOutput(char *strOutput)
{
	// Get Value from FunctionBlock Output from API-Connector
	// https://www.loxforum.com/forum/german/software-konfiguration-programm-und-visualisierung/393263-api-connector-auslesen
	char o_apiBuffer[BUFF_SIZE];
	//GETOUTPUT(FunctionBlock;Output;[Value1:Text1];[Value2:Text2];...)
	sprintf(o_apiBuffer,"GETOUTPUT(Wb2;%s)",strOutput);
	if(DEBUG_OUTPUT)
	{
		printf("API-Input Command: %s",o_apiBuffer);
	}
	setoutputtext(0,o_apiBuffer);
}

void getInputValues() {
	// Temporary usage until API-Connector can be integrated for getting Wb2 FB Output Signals
	valueCaLast = valueCa;
	valueTpLast = valueTp;	
	pushSetEnergyLast = pushSetEnergy;
	valueCa = (int)getinput(11);
	valueTp = getinput(12);
	valueTp = valueTp*1000; // kW >> W
	// Read Input Values from ProgrammBlock
	pushSetEnergy = getinput(0);
	pushSetEnergy = pushSetEnergy*CONV_ENERGY; // kWh >> 0.1 mWh
	if(DEBUG_OUTPUT)
	{
		printf("Read Input 1: %f mWh",pushSetEnergy);
		printf("Read Input 12: %d",valueCa);
		printf("Read Input 13: %f W",valueTp);
	}
}

void setEnableCharging(int AChargingAllowed) {
	// Set Enable Signal 1/0
	char enaBuffer[BUFF_SIZE];
	sprintf(enaBuffer,"ena %d",AChargingAllowed);
	if((AChargingAllowed == 0) && (valuePhaseSwitch == 1) && (calcTimer(iTimePhSw,COOLDOWN) == 0)) {
		printf("Reset phaseSwitch to LOW");
		sendBuffer("x2 0");
		iTimePhSw = getcurrenttime();
		sleep(1000);
		sendBuffer(enaBuffer);
	}
	if((valueCaLast != valueCa) || (valueCa != valueCac))
	{
		sendBuffer(enaBuffer);
		valueCac = valueCa;
	}
}

void setUserCurrent(float ATargetPower) {
/*
Calculating unser Current
*/
	char currUserBuffer[BUFF_SIZE];
	if (valueCa == 1) {
		pushUserCurrLast = pushUserCurr;
		
		// store timestamps for delayed phase switch
		if (ATargetPower < POWER_TH_ON) {
			iTimePhSwOn = getcurrenttime(); //store timestamp while "ATargetPower" is below POWER_TH_ON
		}
		
		if (ATargetPower > POWER_TH_OFF) {
			iTimePhSwOff = getcurrenttime(); //store timestamp while "ATargetPower" is above POWER_TH_OFF
		}

		// switch to three phases if i is over POWER_TH_ON and Cooldown Time in 0
		if ((ATargetPower >= POWER_TH_ON_DIRECT) || ((ATargetPower >= POWER_TH_ON) && (calcTimer(iTimePhSwOn,POWER_TH_ON_TIME) == 0))) {
			if ((calcTimer(iTimePhSw,COOLDOWN) == 0) && (valuePhaseSwitch == 0)) {
				sendBuffer("x2 1");
				iTimePhSw = getcurrenttime();
			}
		} else if ((ATargetPower <= POWER_TH_OFF_DIRECT) || ((ATargetPower <= POWER_TH_OFF) && (calcTimer(iTimePhSwOff,POWER_TH_OFF_TIME) == 0))) {
			if ((calcTimer(iTimePhSw,COOLDOWN) == 0) && (valuePhaseSwitch == 1)) {
				sendBuffer("x2 0");
				iTimePhSw = getcurrenttime();
			}
		}

		// choose user current calculation - 3 phase if switched on and voltage and current is valid
		if ((valuePhaseSwitch == 1) && ((valueVoltage1+valueVoltage2+valueVoltage3) > 600) && (valueCurrent1 > 1) && (valueCurrent2 > 1) && (valueCurrent3 > 1)) {
			pushUserCurr = ATargetPower/(valueVoltage1*sqrt(3))/sqrt(3);
		} else {
			pushUserCurr = ATargetPower/valueVoltage1;
		}
		// convert user current
		pushUserCurr = pushUserCurr*CONV_CURRENT; // A >> mA
		
		// check minimum and maximum user current
		if (pushUserCurr >= valueUserCurrMax) {
			pushUserCurr = valueUserCurrMax;
		} else if (pushUserCurr <= 6000) {
			pushUserCurr = 6000;
		}
		
		if((pushUserCurrLast != pushUserCurr) || (valueUserCurr != pushUserCurr)) {
			sprintf(currUserBuffer,"curr %d",(int)pushUserCurr);
			sendBuffer(currUserBuffer);
		}

		if(DEBUG_OUTPUT)
		{
			printf("push user current: %d",(int)pushUserCurr);
		}
	} else {
		iTimePhSwOn = getcurrenttime(); //store timestamp while not charging
		iTimePhSwOff = getcurrenttime(); //store timestamp while not charging
	}
}

void setSetEnergy(float ASetEnergy) {
	// Set setEnergy Command
	char LSerEnergyBuffer[BUFF_SIZE];
	if ((pushSetEnergyLast != ASetEnergy) || (valueSetEnergy != ASetEnergy)) {
		sprintf(LSerEnergyBuffer,"setenergy %d",(int)ASetEnergy);
		sendBuffer(LSerEnergyBuffer);
	}
}

void getReport2 () {
	int nCnt;
	char udpBuffer[BUFF_SIZE];
	sendBuffer("report 2");	
	nCnt = stream_read(udpStream,udpBuffer,BUFF_SIZE,100); // read stream to buffer
	iTimeReport2 = getcurrenttime();
	if(nCnt > 0) { //check received bytes 
		if(DEBUG_OUTPUT) {
			printf("UDP Buffer received from stream %s: \n%s",STREAM_ADRESS,udpBuffer);	
		}
		iReportId = i_extractValueFromReport(udpBuffer,"\"ID\": \"");
		iTimeSecReport2 = i_extractValueFromReport(udpBuffer,"\"Sec\": ");
		if(iReportId == 2) //read report 2
		{
			iDeviceSerial = i_extractValueFromReport(udpBuffer,"\"Serial\": \"");
			if (iDeviceSerial == SERIAL_NO)
			{
				//calculating Charging active signal
				valueCac = i_extractValueFromReport(udpBuffer,"\"Enable sys\": ");
				if((valueCac != valueCa) && (!MULTI_CONTROL)) {
					valueCa = valueCac; // Stop Charging if valueCa != valueCac
				}
				//calculating Vehicle connected signal
				valueVc = i_extractValueFromReport(udpBuffer,"\"Plug\": ");
				if (valueVc > 4){ // if Plug State > 4 Vehicle is connected
					valueVc = 1;
				} else {
					valueVc = 0;
				}
				valueUserCurr  = f_extractValueFromReport(udpBuffer,"\"Curr user\": ");
				valueUserCurrMax  = f_extractValueFromReport(udpBuffer,"\"Curr HW\": ");
				valuePhaseSwitchLast = valuePhaseSwitch;
				valuePhaseSwitch  = i_extractValueFromReport(udpBuffer,"\"X2 phaseSwitch\": ");
				valuePhaseSwitchSrc  = i_extractValueFromReport(udpBuffer,"\"X2 phaseSwitch source\": ");
				valueSetEnergy  = f_extractValueFromReport(udpBuffer,"\"Setenergy\": ");
				valueSetEnergy = valueSetEnergy / CONV_ENERGY; // kWh << 0.1 Wh

				if(valuePhaseSwitchLast != valuePhaseSwitch) {
					iTimePhSw = getcurrenttime();
				}
				if(valuePhaseSwitchSrc != 4) { // Switch Source if it is not UDP
					pushPhaseSwitchSrc = 1;
				} else {
					pushPhaseSwitchSrc = 0;
				}
			}
		}
	}
}

void getReport3 () {
	int nCnt;
	char udpBuffer[BUFF_SIZE];
	sendBuffer("report 3");
	nCnt = stream_read(udpStream,udpBuffer,BUFF_SIZE,100); // read stream to buffer
	iTimeReport3 = getcurrenttime();
	if(nCnt > 0) { //check received bytes
		if(DEBUG_OUTPUT) {
			printf("UDP Buffer received from stream %s: \n%s",STREAM_ADRESS,udpBuffer);	
		}
		iReportId = i_extractValueFromReport(udpBuffer,"\"ID\": \"");
		iTimeSecReport3 = i_extractValueFromReport(udpBuffer,"\"Sec\": ");
		if(iReportId == 3) //read report 3
		{
			iDeviceSerial = i_extractValueFromReport(udpBuffer,"\"Serial\": \"");
			if (iDeviceSerial == SERIAL_NO)
			{
				valueCp = f_extractValueFromReport(udpBuffer,"\"P\": ");
				valueCp = valueCp / CONV_POWER; // mW >> kW

				valueMr = f_extractValueFromReport(udpBuffer,"\"E total\": ");
				valueMr = valueMr / CONV_ENERGY; // 0.1 Wh >> kWh 

				valueVoltage1 = f_extractValueFromReport(udpBuffer,"\"U1\": ");
				valueVoltage2 = f_extractValueFromReport(udpBuffer,"\"U2\": ");
				valueVoltage3 = f_extractValueFromReport(udpBuffer,"\"U3\": ");
				valueCurrent1 = f_extractValueFromReport(udpBuffer,"\"I1\": ");
				valueCurrent2 = f_extractValueFromReport(udpBuffer,"\"I2\": ");
				valueCurrent3 = f_extractValueFromReport(udpBuffer,"\"I3\": ");
				valueCurrent1 = valueCurrent1/CONV_CURRENT; //mA to A
				valueCurrent2 = valueCurrent2/CONV_CURRENT; //mA to A
				valueCurrent3 = valueCurrent3/CONV_CURRENT; //mA to A
			}
		}
	}
}

void MainLoop() {
	nEvents = getinputevent();
	nEvents = (nEvents & 0xe);
	// Set PhaseSwitch-Source to UDP if is not present
	if(pushPhaseSwitchSrc == 1) {
		sendBuffer("x2src 4"); 
	}
	// getApiOutput("Mr");
	// if ((nEvents) || ((valueCa != valueCac) && (!MULTI_CONTROL))) {
		getInputValues(); //Read input values to global variabels
		setEnableCharging(valueCa); //set ena0 Signal, reset phaseSwitch signal
		setUserCurrent(valueTp); //set userCurrent with phaseSwitch signal on thereshold level - - push value if changed
		setSetEnergy(pushSetEnergy); //control setEnergy value - push value if changed
	// }
	//polling report messages
	if (calcTimer(iTimeReport2,UDP_POLL) == 0) {
		getReport2();
	} else if (calcTimer(iTimeReport3,UDP_POLL) == 0) {
		getReport3();
	}
	// Write API-Outputs
	flushApiOutput(valueCac,valueVc,valueCp,valueMr);
	// Write OutputValues	
	setoutput(0,valueSetEnergy); 							// O1	- SetEnergy Value in kWh
	setoutput(1,valuePhaseSwitch); 							// O2	- PhaseSwitch State
	setoutput(2,valuePhaseSwitchSrc); 						// O3	- PhaseSwitch Source
	setoutput(3,calcTimer(iTimePhSw,COOLDOWN)); 			// O4	- Cooldown Time to next switch
	setoutput(4,COOLDOWN); 									// O5	- Cooldown Time Constant
	setoutput(5,iTimeSecReport2); 							// O6	- Sec Timestamp From UDP Report 2
	setoutput(6,iTimeSecReport3); 							// O7	- Sec Timestamp From UDP Report 3
	setoutput(7,calcTimer(iTimePhSwOn,POWER_TH_ON_TIME));	// O8	- Timer for PhaseSwitch On Actual
	setoutput(8,POWER_TH_ON_TIME); 							// O9	- Timer for PhaseSwitch On Maximum
	setoutput(9,calcTimer(iTimePhSwOff,POWER_TH_OFF_TIME));	//O10	- Timer for PhaseSwitch Off Actual
	setoutput(10,POWER_TH_OFF_TIME); 						//O11	- Timer for PhaseSwitch Off Maximum
}

while(COM_ACTIVE)
{
	MainLoop();	
	sleep(100);
}