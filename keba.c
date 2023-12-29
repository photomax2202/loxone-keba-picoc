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
#define POWER_TH 4.2 //Power Thereshold Phase switch
#define COOLDOWN 300 //Cooldown Time after PhaseSwitch in s (Keba Manual)

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
float valueUserCurr;
int valuePhaseSwitch;
int valuePhaseSwitchLast;
int valuePhaseSwitchSrc;
int deviceSerial;
float valueVoltage1;
float valueVoltage2;
float valueVoltage3;
float valueCurrent1;
float valueCurrent2;
float valueCurrent3;
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
iTimeReport2 = getcurrenttime();
iTimeReport3 = getcurrenttime() + 1;
iTimePhaseSwitch = getcurrenttime() - 300;


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
	// Set API-Connector Command to FunctionBlock Input with Integer Parameter
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
	// Set API-Connector Command to FunctionBlock Input with Float Parameter
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
	valueTp = valueTp*1000;
	// Read Input Values from ProgrammBlock
	pushSetEnergy = getinput(0);
	pushSetEnergy = pushSetEnergy*10000;
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
		if (i <= POWER_TH) {
			valueUserCurr = i/(valueVoltage1*sqrt(3))/sqrt(3);
		} else {
			valueUserCurr = i/valueVoltage1;
		}
		sprintf(currUserBuffer,"curr %d",(int)valueUserCurr);
		if (valueTpLast != valueTp) {
			sendBuffer(currUserBuffer);	
		}
	}
	free(currUserBuffer);
}

void setSetEnergy(float i) {
	// Set setEnergy Command
	char energyBuffer[BUFF_SIZE];
	if(pushSetEnergyLast != pushSetEnergy)
	{
		sprintf(energyBuffer,"setenergy %f",i);
		sendBuffer(energyBuffer);
	}
	free(energyBuffer);
}

while(1)
{
	getInputValues(1);
	setEnableCharging(valueCa);
	setUserCurrent(valueTp);
	setSetEnergy(valueSetEnergy);
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
		//printf("Received Report%f",reportId);
		if(reportId == 2)
		{
			//setoutputtext(1,szBuffer);
			deviceSerial = i_extractValueFromReport(szBuffer,"\"Serial\": \"");
			if (deviceSerial == SERIAL_NO)
			{
				valueCac = i_extractValueFromReport(szBuffer,"\"Enable sys\": ");
				valueVc = i_extractValueFromReport(szBuffer,"\"Plug\": ");
				if (valueVc > 4){
					valueVc = 1;
				} else {
					valueVc = 0;
				}
				valueUserCurr  = f_extractValueFromReport(szBuffer,"\"Curr user\": ");
				valuePhaseSwitchLast = valuePhaseSwitch;
				valuePhaseSwitch  = i_extractValueFromReport(szBuffer,"\"X2 phaseSwitch\": ");
				valuePhaseSwitchSrc  = i_extractValueFromReport(szBuffer,"\"X2 phaseSwitch source\": ");
				valueSetEnergy  = f_extractValueFromReport(szBuffer,"\"Setenergy\": ");
				valueSetEnergy = valueSetEnergy / 10000;

				if (valuePhaseSwitchLast != valuePhaseSwitch){
					iTimePhaseSwitch = getcurrenttime();
				}
			}
		}
		if(reportId == 3){
			//setoutputtext(2,szBuffer);
			deviceSerial = i_extractValueFromReport(szBuffer,"\"Serial\": \"");
			if (deviceSerial == SERIAL_NO)
			{
				valueCp = f_extractValueFromReport(szBuffer,"\"P\": ");
				valueCp = valueCp / 1000000;

				valueMr = f_extractValueFromReport(szBuffer,"\"E total\": ");
				valueMr = valueMr / 10000;
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
