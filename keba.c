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
#define IP_ADRESS "*IPADRESSE*"
#define SERIAL_NO "1"
#define STREAM_ADRESS "/dev/udp/*IPADRESSE*/7090"
#define BUFF_SIZE 512
#define UDP_PAUSE 100
#define MAX_C_POWER 11

// define global variabels
char strAdressStream;
int nCnt;
float valueVc;
float valueCp;
float valueMr;
float valueCac;
float reportId;
unsigned int iTimeReport2;
unsigned int iTimeReport3;

// init global objects
sprintf(strAdressStream,"/dev/udp/%s/7090",UDP_PAUSE);
STREAM* udpStream = stream_create(STREAM_ADRESS,0,0); // create udp stream
iTimeReport2 = getcurrenttime();
iTimeReport3 = getcurrenttime() + 1;


void sendBuffer(char *str) {
	stream_write(udpStream,str,strlen(str)); // write to output buffer
	stream_flush(udpStream); // flush output buffer
	//printf(str);
}

float extractValueFromReport(char *str,char *strfind)
{
	char strBufferOne[BUFF_SIZE];
	char strBufferTwo[BUFF_SIZE];
	char strBufferOut[BUFF_SIZE];
	char* p;
	
	strBufferOne = strstrskip(str,strfind);
	strBufferTwo = strstr(strBufferOne,",");
	strncpy(strBufferOut,strBufferOne,strlen(strBufferOne)-strlen(strBufferTwo));
	p = strstr(strBufferOut,"\"");
	//printf(strBufferOut);
	if(p!=NULL)
	{
		strBufferTwo = "";
		strBufferOut = "";
		strBufferTwo = strstr(strBufferOne,"\"");
		strncpy(strBufferOut,strBufferOne,strlen(strBufferOne)-strlen(strBufferTwo));
	}
	//printf("Index: %s Value: %s",strfind,strBufferOut);
	return batof(strBufferOut);
	free(strBufferOne);
	free(strBufferTwo);
	free(strBufferOut);
	free(p);
}

void setApiOutput(char *str,float flValue)
{
	char apiBuffer[BUFF_SIZE];
	sprintf(apiBuffer,"SET(Wb2;%s;%f)",str,flValue);
	//printf(apiBuffer);
	setoutputtext(0,apiBuffer);
	free(apiBuffer);
}

while(1)
{
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
		reportId = extractValueFromReport(szBuffer,"\"ID\": \"");
		//printf("Received Report%f",reportId);
		if(reportId == 2)
		{
			setoutputtext(1,szBuffer);
			valueCac = extractValueFromReport(szBuffer,"\"Enable sys\": ");

			valueVc = extractValueFromReport(szBuffer,"\"Plug\": ");

		}
		if(reportId == 3){
			setoutputtext(2,szBuffer);

			valueCp = extractValueFromReport(szBuffer,"\"P\": ");
			valueCp = valueCp / 1000000;

			valueMr = extractValueFromReport(szBuffer,"\"E total\": ");
			valueMr = valueMr / 10000;
		}
	}
	setApiOutput("Cac",valueCac);
	if (valueVc > 4){
		setApiOutput("Vc",1);
	} else {
		setApiOutput("Vc",0);
	}
	setApiOutput("Cp",valueCp);
	setApiOutput("Mr",valueMr);
	setApiOutput("Lm1",MAX_C_POWER);
	free(szBuffer);
}
