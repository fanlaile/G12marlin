#include <wstring.h>
#include <stdio.h>
#include <string.h>
#include <Arduino.h>
#include "dwin_uart.h"
#include "../../inc/MarlinConfig.h"

UARTDATA dwinlcd;

UARTDATA::UARTDATA()
{
	recdat.head[0] = snddat.head[0] = FHONE;
	recdat.head[1] = snddat.head[1] = FHTWO;
	memset(databuf,0, sizeof(databuf));
}

int UARTDATA::UART_RecData()
{
	static int recnum = 0;
	while(MYSERIAL1.available() > 0 && (recnum < SizeofDatabuf))
	{
		delay(1);
		databuf[recnum] = MYSERIAL1.read();
		// SERIAL_ECHOLNPAIR("\r\ndatabuf: ", databuf[recnum]);//用于测试，打印出触摸屏反馈的字节数据，调试触摸屏响应问题
		if(databuf[0] == FHONE)
		{
			recnum++;
		}
		else if(databuf[0] == FHTWO)
		{
			databuf[0] = FHONE;
			databuf[1] = FHTWO;
			recnum += 2;
		}
		else if(databuf[0] == FHLENG)
		{
			databuf[0] = FHONE;
			databuf[1] = FHTWO;
			databuf[2] = FHLENG;
			recnum += 3;
		}
		else if(databuf[0] == VarAddr_R)
		{
			databuf[0] = FHONE;
			databuf[1] = FHTWO;
			databuf[2] = FHLENG;
			databuf[3] = VarAddr_R;
			recnum += 4;
		}
		else
		{
			recnum = 0;
		}
	}

	// receive nothing  	
	if(recnum < 1)
	{
		return -1;
	}
	else  if((recdat.head[0] == databuf[0]) && (recdat.head[1] == databuf[1]) && recnum > 2)
	{
		recdat.len = databuf[2];
		recdat.command = databuf[3];
		if(recdat.len == 0x03 && (recdat.command == 0x82 || recdat.command == 0x80) && (databuf[4] == 0x4F) && (databuf[5] == 0x4B))  //response for writing byte
		{   
			memset(databuf,0, sizeof(databuf));
			recnum = 0;
			return -1;
		}
		else if(recdat.command == 0x83)
		{
			// response for reading the data from the variate
			recdat.addr = databuf[4];
			recdat.addr = (recdat.addr << 8 ) | databuf[5];
			recdat.bytelen = databuf[6];
			for(unsigned int i = 0;i < recdat.bytelen;i+=2)
			{
				recdat.data[i/2]= databuf[7+i];
				recdat.data[i/2]= (recdat.data[i/2] << 8 )| databuf[8+i];
			}
		}
		else if(recdat.command == 0x81)
		{
			// response for reading the page from the register
			recdat.addr = databuf[4];
			recdat.bytelen = databuf[5];
			for(unsigned int i = 0;i < recdat.bytelen;i++)
			{
				recdat.data[i]= databuf[6+i];
				// recdat.data[i]= (recdat.data[i] << 8 )| databuf[7+i];
			}
		}
	}
	else
	{
		memset(databuf,0, sizeof(databuf));
		recnum = 0;
		// receive the wrong data
		return -1;
	}
	memset(databuf,0, sizeof(databuf));
	recnum = 0;
	return 2;
}


void UARTDATA::UART_SndData(void)
{
	if((snddat.head[0] == FHONE) && (snddat.head[1] == FHTWO) && snddat.len >= 3)
	{
	databuf[0] = snddat.head[0];
	databuf[1] = snddat.head[1];
	databuf[2] = snddat.len;
	databuf[3] = snddat.command;
	// to write data to the register
	if(snddat.command ==0x80)
	{
	databuf[4] = snddat.addr;
	for(int i =0;i <(snddat.len - 2);i++)
	{
	databuf[5+i] = snddat.data[i];
	}
	}
	else if(snddat.len == 3 && (snddat.command ==0x81))
	{
	// to read data from the register
	databuf[4] = snddat.addr;
	databuf[5] = snddat.bytelen;
	}
	else if(snddat.command ==0x82)
	{
	// to write data to the variate
	databuf[4] = snddat.addr >> 8;
	databuf[5] = snddat.addr & 0xFF;
	for(int i =0;i <(snddat.len - 3);i += 2)
	{
	databuf[6 + i] = snddat.data[i/2] >> 8;
	databuf[7 + i] = snddat.data[i/2] & 0xFF;
	}
	}
	else if(snddat.len == 4 && (snddat.command ==0x83))
	{
	// to read data from the variate
	databuf[4] = snddat.addr >> 8;
	databuf[5] = snddat.addr & 0xFF;
	databuf[6] = snddat.bytelen;
	}
	
	for (uint8_t i=0; i < (snddat.len + 3);i++)
	{
		MYSERIAL1.write(databuf[i]);
		//delayMicroseconds(1);
	}
	
	memset(&snddat,0,sizeof(snddat));
	memset(databuf,0, sizeof(databuf));
	snddat.head[0] = FHONE;
	snddat.head[1] = FHTWO;
	}
}

void UARTDATA::UART_SndData(const String &s, unsigned long addr, unsigned char cmd /*= VarAddr_W*/)
{
	if(s.length() < 1)
	{
		return;
	}
	UART_SndData(s.c_str(), addr, cmd);
}

void UARTDATA::UART_SndData(const char *str, unsigned long addr, unsigned char cmd/*= VarAddr_W*/)
{
	int len = strlen(str);
	if( len > 0)
	{
		databuf[0] = FHONE;
		databuf[1] = FHTWO;
		databuf[2] = 3+len;
		databuf[3] = cmd;
		databuf[4] = addr >> 8;
		databuf[5] = addr & 0x00FF;
		for(int i =0;i <len ;i++)
		{
			databuf[6 + i] = str[i];
		}

		for (uint8_t i=0; i < (len + 6);i++)
		{
			MYSERIAL1.write(databuf[i]);
			//delayMicroseconds(1);
		}

		memset(databuf,0, sizeof(databuf));
	}
}

void UARTDATA::UART_SndData(const char *str, unsigned long addr, int len,unsigned char cmd/*= VarAddr_W*/)
{
	if( len > 0)
	{
		databuf[0] = FHONE;
		databuf[1] = FHTWO;
		databuf[2] = 3+len;
		databuf[3] = cmd;
		databuf[4] = addr >> 8;
		databuf[5] = addr & 0x00FF;
		for(int i =0;i <len ;i++)
		{
			databuf[6 + i] = str[i];
		}

		for (uint8_t i=0; i < (len + 6);i++)
		{
			MYSERIAL1.write(databuf[i]);
			//delayMicroseconds(1);
		}

		memset(databuf,0, sizeof(databuf));
	}
}

void UARTDATA::UART_SndData(char c, unsigned long addr, unsigned char cmd/*= VarAddr_W*/)
{
	snddat.command = cmd;
	snddat.addr = addr;
	snddat.data[0] = (unsigned long)c;
	snddat.data[0] = snddat.data[0] << 8;
	snddat.len = 5;
	UART_SndData();
}

void UARTDATA::UART_SndData(unsigned char* str, unsigned long addr, unsigned char cmd){UART_SndData((char *)str, addr, cmd);}

void UARTDATA::UART_SndData(int n, unsigned long addr, unsigned char cmd/*= VarAddr_W*/)
{
	if(cmd == VarAddr_W )
	{
		if(n > 0xFFFF)
		{
			snddat.data[0] = n >> 16;
			snddat.data[1] = n & 0xFFFF;
			snddat.len = 7;
		}
		else
		{
			snddat.data[0] = n;
			snddat.len = 5;
		}
	}
	else if(cmd == RegAddr_W)
	{
		snddat.data[0] = n;
		snddat.len = 3;
	}
	else if(cmd == VarAddr_R)
	{
		snddat.bytelen = n;
		snddat.len = 4;
	}
	snddat.command = cmd;
	snddat.addr = addr;
	UART_SndData();
}

void UARTDATA::UART_SndData(unsigned int n, unsigned long addr, unsigned char cmd){ UART_SndData((int)n, addr, cmd); }

void UARTDATA::UART_SndData(float n, unsigned long addr, unsigned char cmd){ UART_SndData((int)n, addr, cmd); }

void UARTDATA::UART_SndData(long n, unsigned long addr, unsigned char cmd){ UART_SndData((unsigned long)n, addr, cmd); }

void UARTDATA::UART_SndData(unsigned long n, unsigned long addr, unsigned char cmd/*= VarAddr_W*/)
{
	if(cmd == VarAddr_W )
	{
		if(n > 0xFFFF)
		{
			snddat.data[0] = n >> 16;
			snddat.data[1] = n & 0xFFFF;
			snddat.len = 7;
		}
		else
		{
			snddat.data[0] = n;
			snddat.len = 5;
		}
	}
	else if(cmd == VarAddr_R)
	{
		snddat.bytelen = n;
		snddat.len = 4;
	}
	snddat.command = cmd;
	snddat.addr = addr;
	UART_SndData();
}

unsigned short record_current_page = 0;
unsigned short dwin_get_currentpage(void){ return record_current_page;}

void dwin_page_set(int page_num)
{
	record_current_page = page_num;
	dwinlcd.UART_SndData(ExchangePageBase + page_num, ExchangepageAddr);
}

void dwin_var_set(int var_val,  unsigned long var_addr)
{
	dwinlcd.UART_SndData(var_val, var_addr);
}

void dwin_icon_set(int icon_numb,  unsigned long icon_addr)
{
	dwinlcd.UART_SndData(icon_numb, icon_addr);
}

void dwin_str_set(const char *str,  unsigned long var_addr)
{
	dwinlcd.UART_SndData(str, var_addr);
}

void dwin_unicode_set(const char *str,  unsigned long var_addr, int len)
{
	dwinlcd.UART_SndData(str, var_addr, len);
}
