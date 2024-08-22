#ifndef	DWIN_UART_H
#define	DWIN_UART_H
#include "string.h"
#include <arduino.h>


/******************* commad head *******************/
#define FHONE   (0x5A)
#define FHTWO   (0xA5)
#define FHLENG  (0x06)

/******************* Register and Variable addr*******************/
#define	RegAddr_W	0x80
#define	RegAddr_R	0x81
#define	VarAddr_W	0x82
#define	VarAddr_R	0x83

/******************* Page addr *******************/
#define	ExchangePageBase    ((unsigned long)0x5A010000)     // the first page ID. other page = first page ID + relevant num;
#define	ExchangepageAddr	0x0084

/*********************************/

#define SizeofDatabuf		46

/************struct**************/
typedef struct DataBuf
{
    unsigned char len;  
    unsigned char head[2];
    unsigned char command;
    unsigned long addr;
    unsigned long bytelen;
    unsigned short data[32];
    unsigned char reserv[4];
} DB;


class UARTDATA {
  public:
    UARTDATA();
    int UART_RecData();
    void UART_SndData(void);
    void UART_SndData(const String &, unsigned long, unsigned char = VarAddr_W);
    void UART_SndData(const char[], unsigned long, unsigned char = VarAddr_W);
    void UART_SndData(const char[], unsigned long, int,unsigned char = VarAddr_W);
    void UART_SndData(char, unsigned long, unsigned char = VarAddr_W);
    void UART_SndData(unsigned char*, unsigned long, unsigned char = VarAddr_W);
    void UART_SndData(int, unsigned long, unsigned char = VarAddr_W);
    void UART_SndData(float, unsigned long, unsigned char = VarAddr_W);
    void UART_SndData(unsigned int,unsigned long, unsigned char = VarAddr_W);
    void UART_SndData(long,unsigned long, unsigned char = VarAddr_W);
    void UART_SndData(unsigned long,unsigned long, unsigned char = VarAddr_W);
    void UART_Init();
    
    DB recdat;
    DB snddat;
  private:
    unsigned char databuf[SizeofDatabuf];
  };

extern UARTDATA dwinlcd;

unsigned short dwin_get_currentpage(void);
void dwin_page_set(int page_num);
void dwin_var_set(int var_val,  unsigned long var_addr);
void dwin_icon_set(int icon_numb,  unsigned long icon_addr);
void dwin_str_set(const char *str,  unsigned long var_addr);
void dwin_unicode_set(const char *str,  unsigned long var_addr, int len);

#endif // DWIN_UART_H
