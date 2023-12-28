/*
Integration Keba P30 Wallbox übe UDP Kommunikation
Signalquellen
Vc	- report 2 > Plug >= 5
Cp	- report 3 > P
Mr	- report 3 > E total
Cac	- report 2 > Enable sys
*/
// server-address und port bitte anpassen
// define Constants
#define SERIAL_NO 20420166
#define STREAM_ADRESS "/dev/udp/192.168.98.29/7090"
#define BUFF_SIZE 512
#define UDP_PAUSE 100
#define DEBUG_OUTPUT false

// define global variabels
// function Variables
int nCnt;
int nEvents;
//Process Values
int valueVc;
int valueCa;
float valueTp;
int valueCaLast;
float valueTpLast;
float valueCp;
float valueMr;
int valueCac;
float reportId;
float valueUserCurr;
int valuePhaseSwitch;
int valuePhaseSwitchSrc;
int deviceSerial;
float valueVoltage1;
float valueVoltage2;
float valueVoltage3;
float valueCurrent1;
float valueCurrent2;
float valueCurrent3;
// timerStorage
unsigned int iTimeReport2;
unsigned int iTimeReport3;

// init global objects
STREAM* udpStream = stream_create(STREAM_ADRESS,0,0); // create udp stream
iTimeReport2 = getcurrenttime();
iTimeReport3 = getcurrenttime() + 1;


void sendBuffer(char *str) {
	stream_write(udpStream,str,strlen(str)); // write to output buffer
	stream_flush(udpStream); // flush output buffer
	if(DEBUG_OUTPUT)
	{
		printf("Flush UDP-Stream: %s",str);
	}
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
	char apiBuffer[BUFF_SIZE];
	sprintf(apiBuffer,"SET(Wb2;%s;%f)",str,flValue);
	if(DEBUG_OUTPUT)
	{
		printf("API-Output Command: %s",apiBuffer);
	}
	setoutputtext(0,apiBuffer);
	free(apiBuffer);
}

void i_setApiOutput(char *str,int iValue)
{
	char apiBuffer[BUFF_SIZE];
	sprintf(apiBuffer,"SET(Wb2;%s;%d)",str,iValue);
	if(DEBUG_OUTPUT)
	{
		printf("API-Output Command: %s",apiBuffer);
	}
	setoutputtext(0,apiBuffer);
	free(apiBuffer);
}

void getApiOutput(char *strOutput)
{
	char apiBuffer[BUFF_SIZE];
	//GETOUTPUT(FunctionBlock;Output;[Value1:Text1];[Value2:Text2];...)
	sprintf(apiBuffer,"GETOUTPUT(Wb2;%s)",strOutput);
	if(DEBUG_OUTPUT)
	{
		printf("API-Input Command: %s",apiBuffer);
	}
	setoutputtext(0,apiBuffer);
	free(apiBuffer);
}

void getInputValues(int i) {
	valueCaLast = valueCa;
	valueTpLast = valueTp;	
	valueCa = (int)getinput(0);
	valueTp = getinput(1);
	if(DEBUG_OUTPUT)
	{
		printf("Read Input 0: %f",(float)valueCa);
		printf("Read Input 1: %f",valueTp);
	}
}

void setEnableCharging(int i) {
	char szBuffer[BUFF_SIZE];
	if(valueCaLast != valueCa)
	{
		sprintf(szBuffer,"ena %d",valueCa);
		sendBuffer(szBuffer);
		valueCac = valueCa;
	}
	free(szBuffer);
}

while(1)
{
	getInputValues(1);
	setEnableCharging(1);
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
	if(DEBUG_OUTPUT)
	{
		printf("Buffer received: \n%s",szBuffer);	
	}
	if(nCnt > 0) //check received bytes
	{
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
				valueUserCurr  = f_extractValueFromReport(szBuffer,"\"Curr user\": ");
				valuePhaseSwitch  = i_extractValueFromReport(szBuffer,"\"X2 phaseSwitch\": ");
				valuePhaseSwitchSrc  = i_extractValueFromReport(szBuffer,"\"X2 phaseSwitch source\": ");
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
	i_setApiOutput("Cac",valueCac);
	if (valueVc > 4){
		i_setApiOutput("Vc",1);
	} else {
		i_setApiOutput("Vc",0);
	}
	f_setApiOutput("Cp",valueCp);
	f_setApiOutput("Mr",valueMr);
	free(szBuffer);
}
