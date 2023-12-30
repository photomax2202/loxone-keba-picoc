/*
Integration Keba P30 Wallbox über UDP Kommunikation
Signal Source > get value from UDP-Report > send to API-Connector
Vc	- report 2 > Plug >= 5
Cp	- report 3 > P
Mr	- report 3 > E total
Cac	- report 2 > Enable sys

Input Value from Wb2 FB I/O Connector
I12	- Enable Charging
I13	- Target Power in kW
Using API-Connector if possible
T1	- Reserved for API-Input

I1	- SetEnergy in kWh

O1	- SetEnergy Value in kWh
O2	- PhaseSwitch State
O3	- PhaseSwitch Source
O4	- Cooldown Time to next switch

Function:
Polling "report 2" & "report 3" every 5s with 1s offset between reports.


Knowing Issues:
Vc Signal is pushed to API-Connector but Wb2 FB does not process correctly
*/
// server-address und port bitte anpassen
// define Constants
#define SERIAL_NO 20420166
#define STREAM_ADRESS "/dev/udp/192.168.98.29/7090"
#define BUFF_SIZE 512
#define UDP_PAUSE 100 
#define DEBUG_OUTPUT false
#define POWER_TH 4200 //Power Thereshold Phase switch in W
#define COOLDOWN 300 //Cooldown Time after PhaseSwitch in s (Keba Manual)
#define CONV_POWER 1000000 // Constant for Convert Power mW <> kW
#define CONV_ENERGY 10000 // Constant for Convert Energy 0.1 mWh <> kWh
#define CONV_CURRENT 1000 // Constant for Convert Current 1000 mA <> A
#define CONV_PF 10 // Constant for Convert Power Factor 0..1000 <> 0..100%
#define MULTI_CONTROL true

// define global variabels
int nCnt;
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
pushPhaseSwitchSrc = 0;
pushSetEnergy = 99;	// Initial Value for pushing new value
iTimeReport2 = getcurrenttime();
iTimeReport3 = getcurrenttime() + 1;
iTimePhaseSwitch = getcurrenttime();


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

void f_setApiOutput(char *str,float flValue)
{
	// Set API-Connector Command to FunctionBlock Input with Float Parameter
	char f_apiBuffer[BUFF_SIZE];
	sprintf(f_apiBuffer,"SET(Wb2;%s;%f)",str,flValue);
	if(DEBUG_OUTPUT)
	{
		printf("API-Output Command: %s",f_apiBuffer);
	}
	setoutputtext(0,f_apiBuffer);
	free(f_apiBuffer);
}

void i_setApiOutput(char *str,int iValue)
{
	// Set API-Connector Command to FunctionBlock Input with Integer Parameter
	char i_apiBuffer[BUFF_SIZE];
	sprintf(i_apiBuffer,"SET(Wb2;%s;%d)",str,iValue);
	if(DEBUG_OUTPUT)
	{
		printf("API-Output Command: %s",i_apiBuffer);
	}
	setoutputtext(0,i_apiBuffer);
	free(i_apiBuffer);
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
		printf("Read Input 12: %d",valueCa);
		printf("Read Input 13: %f",valueTp);
		printf("Read Input 1: %f",pushSetEnergy);
	}
}

void setEnableCharging(int i) {
	// Set Enable Signal 1/0
	char enaBuffer[BUFF_SIZE];
	sprintf(enaBuffer,"ena %d",i);
	if((i == 0) && (valuePhaseSwitch == 1) && (calcCooldownTime(iTimePhaseSwitch) == 0)) {
		sendBuffer("x2 0");
		iTimePhaseSwitch = getcurrenttime();
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
		// switch to three phases if i is over POWER_TH and Cooldown Time in 0
		if (i <= POWER_TH) {
			if (calcCooldownTime(iTimePhaseSwitch) == 0) {
				sendBuffer("x2 1");
				iTimePhaseSwitch = getcurrenttime();
			}
		} else {
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

while(1)
{
	if(pushPhaseSwitchSrc == 1) {
		sendBuffer("x2src 4");
	}
	getInputValues(1);
	setEnableCharging(valueCa);
	setUserCurrent(valueTp);
	setSetEnergy(pushSetEnergy);
	char szBuffer[BUFF_SIZE];
	if ((getcurrenttime()- iTimeReport2) > 5)
	{
		iTimeReport2 = getcurrenttime();
		sendBuffer("report 2");
	} else if ((getcurrenttime()- iTimeReport3) > 5) {
		iTimeReport3 = getcurrenttime();
		sendBuffer("report 3");
	}
	szBuffer = "";
	nCnt = stream_read(udpStream,szBuffer,BUFF_SIZE,UDP_PAUSE); // read stream to buffer
	sleep(UDP_PAUSE);
	if(nCnt > 0) //check received bytes
	{
		if(DEBUG_OUTPUT)
		{
			printf("Buffer received: \n%s",szBuffer);	
		}
		reportId = i_extractValueFromReport(szBuffer,"\"ID\": \"");
		if(reportId == 2)
		{
			deviceSerial = i_extractValueFromReport(szBuffer,"\"Serial\": \"");
			if (deviceSerial == SERIAL_NO)
			{
				valueCac = i_extractValueFromReport(szBuffer,"\"Enable sys\": ");
				if((valueCac != valueCa) && (!MULTI_CONTROL)) {
					valueCa = valueCac; // Stop Charging if valueCa != valueCac
				}
				valueVc = i_extractValueFromReport(szBuffer,"\"Plug\": ");
				if (valueVc > 4){ // if Plug > 4 Vehicle is connected
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
				if(valuePhaseSwitchSrc != 4) { // Switch Source if its not UDP
					pushPhaseSwitchSrc = 1;
				} else {
					pushPhaseSwitchSrc = 0;
				}
			}
		}
		if(reportId == 3){
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
	i_setApiOutput("Cac",valueCac);
	i_setApiOutput("Vc",valueVc);
	f_setApiOutput("Cp",valueCp);
	f_setApiOutput("Mr",valueMr);
	// Write OutputValues
	setoutput(0,valueSetEnergy); // O1	- SetEnergy Value in kWh
	setoutput(1,valuePhaseSwitch); // O2	- PhaseSwitch State
	setoutput(2,valuePhaseSwitchSrc); // O3	- PhaseSwitch Source
	setoutput(3,calcCooldownTime(iTimePhaseSwitch)); // O4	- Cooldown Time to next switch
	// Free buffer
	free(szBuffer);
}
