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
// #define SERIAL_NO 20420166
// #define STREAM_ADRESS "/dev/udp/192.168.98.29/7090"
#define SERIAL_NO 22618526
#define STREAM_ADRESS "/dev/udp/192.168.178.39/7090"
#define BUFF_SIZE 512
#define UDP_PAUSE 100 //Sleep time between cycles
#define POWER_TH_ON 4200 //Power Thereshold Phase switch ON in W
#define POWER_TH_OFF 4100 //Power Thereshold Phase switch OFF in W
#define COOLDOWN 300 //Cooldown Time after PhaseSwitch in s (Keba Manual)
#define UDP_POLL 5 //Time for polling reports
#define CONV_POWER 1000000 // Constant for Convert Power mW <> kW
#define CONV_ENERGY 10000 // Constant for Convert Energy 0.1 mWh <> kWh
#define CONV_CURRENT 1000 // Constant for Convert Current 1000 mA <> A
#define CONV_PF 10 // Constant for Convert Power Factor 0..1000 <> 0..100%
#define DEBUG_OUTPUT false
// #define MULTI_CONTROL true // Wallbox can be Controlled by second source
#define MULTI_CONTROL false // Wallbox will stop charging if not allowed
// #define COM_ACTIVE false // Programm not active
#define COM_ACTIVE true // Programm active 

// define global variabels
int nCnt;
int cCnt;
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
float reportId;
int deviceSerial;
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
// Variabeles setEnergy
float pushSetEnergy;
float pushSetEnergyLast;
float valueSetEnergy;
// Timestamp Variabeles
unsigned int iTimeReport2;
unsigned int iTimeReport3;
unsigned int iTimePhaseSwitch;

// init global objects
STREAM* udpStream = stream_create(STREAM_ADRESS,0,0); // create udp stream
valueCa = 0;
valueCac = 0;
pushPhaseSwitchSrc = 0;
pushSetEnergy = 99;	// Initial Value for pushing new value
iTimeReport2 = getcurrenttime();
iTimeReport3 = getcurrenttime() + 1;
iTimePhaseSwitch = getcurrenttime() - 250;
cCnt = 0;
void sendBuffer(char *str) {
	stream_write(udpStream,str,strlen(str)); // write to output buffer
	stream_flush(udpStream); // flush output buffer
	if(DEBUG_OUTPUT)
	{
		printf("Flush UDP-Stream: %s",str);
	}
}

unsigned int calcCooldownTime(unsigned int timeStamp){
	unsigned int cooldownTime;
	unsigned int currentTime;
	unsigned int differentTime;
	currentTime = getcurrenttime();
	differentTime = currentTime-timeStamp;
	if (differentTime > COOLDOWN) {
		differentTime = COOLDOWN;
	}
	cooldownTime = COOLDOWN - differentTime;
	if(DEBUG_OUTPUT)
	{
		printf("Cooldown Time: %d",cooldownTime);
	}
	return cooldownTime;
}

float f_extractValueFromReport(char *str,char *strfind)
{
	char strBufferOne[BUFF_SIZE];
	char strBufferTwo[BUFF_SIZE];
	char strBufferOut[BUFF_SIZE];
	char* p;
	
	strBufferOne = strstrskip(str,strfind);
	strBufferTwo = strstr(strBufferOne,",");
	strncpy(strBufferOut,strBufferOne,strlen(strBufferOne)-strlen(strBufferTwo));
	p = strstr(strBufferOut,"\"");
	if(p!=NULL)
	{
		strBufferTwo = "";
		strBufferOut = "";
		strBufferTwo = strstr(strBufferOne,"\"");
		strncpy(strBufferOut,strBufferOne,strlen(strBufferOne)-strlen(strBufferTwo));
	}
	if(DEBUG_OUTPUT)
	{
	printf("Index: %s Value: %s",strfind,strBufferOut);
	}
	return batof(strBufferOut);
	free(strBufferOne);
	free(strBufferTwo);
	free(strBufferOut);
	free(p);
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
	// every 10th cycle is empty
	if (cCnt < 1) {
		setoutputtext(0,"");
	} else {
	setoutputtext(0,strOutputText);
	}
	if (cCnt > 10) {
		cCnt = 0;
	}
	cCnt = cCnt + 1;
	free(strOutputText);
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
	free(o_apiBuffer);
}

void getInputValues(int i) {
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
		printf("Read Input 1: %f",pushSetEnergy);
		printf("Read Input 12: %d",valueCa);
		printf("Read Input 13: %f",valueTp);
	}
}

void setEnableCharging(int i) {
	// Set Enable Signal 1/0
	char enaBuffer[BUFF_SIZE];
	sprintf(enaBuffer,"ena %d",i);
	if((i == 0) && (valuePhaseSwitch == 1) && (calcCooldownTime(iTimePhaseSwitch) == 0)) {
		printf("Reset phaseSwitch to LOW");
		sendBuffer("x2 0");
		iTimePhaseSwitch = getcurrenttime();
		sleep(1000);
		sendBuffer(enaBuffer);
	}
	if(valueCaLast != valueCa)
	{
		sendBuffer(enaBuffer);
		valueCac = valueCa;
	}
	free(enaBuffer);
}

void setUserCurrent(float i) {
/*
Calculating unser Current
*/
	char currUserBuffer[BUFF_SIZE];
	if (valueCa == 1) {
		pushUserCurrLast = pushUserCurr;
		// switch to three phases if i is over POWER_TH_ON and Cooldown Time in 0
		if (i <= POWER_TH_ON) {
			if (calcCooldownTime(iTimePhaseSwitch) == 0) {
				sendBuffer("x2 1");
				iTimePhaseSwitch = getcurrenttime();
			}
		} else if (i >= POWER_TH_OFF) {
			if (calcCooldownTime(iTimePhaseSwitch) == 0) {
				sendBuffer("x2 0");
				iTimePhaseSwitch = getcurrenttime();
			}
		}
		// choose user current calculation
		if (valuePhaseSwitch == 1 && (valueVoltage1+valueVoltage2+valueVoltage3) > 600) {
			pushUserCurr = i/(valueVoltage1*sqrt(3))/sqrt(3);
		} else {
			pushUserCurr = i/valueVoltage1;
		}
		pushUserCurr = pushUserCurr*CONV_CURRENT; // A >> mA
		if(pushUserCurrLast != pushUserCurr) {
			sprintf(currUserBuffer,"curr %d",(int)pushUserCurr);
			sendBuffer(currUserBuffer);
		}
		// printf("push user current: %d",(int)pushUserCurr);
	}
	free(currUserBuffer);
}

void setSetEnergy(float i) {
	// Set setEnergy Command
	char energyBuffer[BUFF_SIZE];
	if(pushSetEnergyLast != pushSetEnergy)
	{
		sprintf(energyBuffer,"setenergy %d",(int)i);
		sendBuffer(energyBuffer);
	}
	free(energyBuffer);
}

void RemarkForMainLoop() {
}

while(COM_ACTIVE)
{
	// Set PhaseSwitch-Source to UDP if is not present
	if(pushPhaseSwitchSrc == 1) {
		sendBuffer("x2src 4"); 
	}
	// getApiOutput("Mr");
	getInputValues(1); //Read input values to global variabels
	setEnableCharging(valueCa); //set ena0 Signal, reset phaseSwitch signal
	setUserCurrent(valueTp); //set userCurrent with phaseSwitch signal on thereshold level - - push value if changed
	setSetEnergy(pushSetEnergy); //control setEnergy value - push value if changed
	//polling report messages
	char szBuffer[BUFF_SIZE];
	if ((getcurrenttime()- iTimeReport2) > UDP_POLL)
	{
		iTimeReport2 = getcurrenttime();
		sendBuffer("report 2");
	} 
	if ((getcurrenttime()- iTimeReport3) > UDP_POLL) {
		iTimeReport3 = getcurrenttime();
		sendBuffer("report 3");
	}
	//flush buffer memory
	szBuffer = "";
	//read udp stream content
	nCnt = stream_read(udpStream,szBuffer,BUFF_SIZE,UDP_PAUSE); // read stream to buffer
	sleep(UDP_PAUSE); // sleep time 
	if(nCnt > 0) //check received bytes
	{
		if(DEBUG_OUTPUT)
		{
			printf("Buffer received: \n%s",szBuffer);	
		}
		reportId = i_extractValueFromReport(szBuffer,"\"ID\": \"");
		if(reportId == 2) //read report 2
		{
			deviceSerial = i_extractValueFromReport(szBuffer,"\"Serial\": \"");
			if (deviceSerial == SERIAL_NO)
			{
				//calculating Charging active signal
				valueCac = i_extractValueFromReport(szBuffer,"\"Enable sys\": ");
				if((valueCac != valueCa) && (!MULTI_CONTROL)) {
					valueCa = valueCac; // Stop Charging if valueCa != valueCac
				}
				//calculating Vehicle connected signal
				valueVc = i_extractValueFromReport(szBuffer,"\"Plug\": ");
				if (valueVc > 4){ // if Plug State > 4 Vehicle is connected
					valueVc = 1;
				} else {
					valueVc = 0;
				}
				valueUserCurr  = f_extractValueFromReport(szBuffer,"\"Curr user\": ");
				valuePhaseSwitchLast = valuePhaseSwitch;
				valuePhaseSwitch  = i_extractValueFromReport(szBuffer,"\"X2 phaseSwitch\": ");
				valuePhaseSwitchSrc  = i_extractValueFromReport(szBuffer,"\"X2 phaseSwitch source\": ");
				valueSetEnergy  = f_extractValueFromReport(szBuffer,"\"Setenergy\": ");
				valueSetEnergy = valueSetEnergy / CONV_ENERGY; // kWh << 0.1 Wh

				if(valuePhaseSwitchLast != valuePhaseSwitch) {
					iTimePhaseSwitch = getcurrenttime();
				}
				if(valuePhaseSwitchSrc != 4) { // Switch Source if it is not UDP
					pushPhaseSwitchSrc = 1;
				} else {
					pushPhaseSwitchSrc = 0;
				}
			}
		}
		if(reportId == 3) //read report 2
		{
			deviceSerial = i_extractValueFromReport(szBuffer,"\"Serial\": \"");
			if (deviceSerial == SERIAL_NO)
			{
				valueCp = f_extractValueFromReport(szBuffer,"\"P\": ");
				valueCp = valueCp / CONV_POWER; // mW >> kW

				valueMr = f_extractValueFromReport(szBuffer,"\"E total\": ");
				valueMr = valueMr / CONV_ENERGY; // 0.1 Wh >> kWh 

				valueVoltage1 = f_extractValueFromReport(szBuffer,"\"U1\": ");
				valueVoltage2 = f_extractValueFromReport(szBuffer,"\"U2\": ");
				valueVoltage3 = f_extractValueFromReport(szBuffer,"\"U3\": ");
				valueCurrent1 = f_extractValueFromReport(szBuffer,"\"I1\": ");
				valueCurrent2 = f_extractValueFromReport(szBuffer,"\"I2\": ");
				valueCurrent3 = f_extractValueFromReport(szBuffer,"\"I3\": ");
				valueCurrent1 = valueCurrent1/CONV_CURRENT;
				valueCurrent2 = valueCurrent2/CONV_CURRENT;
				valueCurrent3 = valueCurrent3/CONV_CURRENT;
			}
		}
	}
	// Write API-Outputs
	flushApiOutput(valueCac,valueVc,valueCp,valueMr);
	// Write OutputValues
	setoutput(0,valueSetEnergy); // O1	- SetEnergy Value in kWh
	setoutput(1,valuePhaseSwitch); // O2	- PhaseSwitch State
	setoutput(2,valuePhaseSwitchSrc); // O3	- PhaseSwitch Source
	setoutput(3,calcCooldownTime(iTimePhaseSwitch)); // O4	- Cooldown Time to next switch
	setoutput(4,COOLDOWN); // O5 - Cooldown Time Constant
	// Free buffer
	free(szBuffer);
}
