#include <wstring.h>
#include <stdio.h>
#include <string.h>
#include <Arduino.h>

#include "../../MarlinCore.h"
#include "../../inc/MarlinConfig.h"
#include "../../sd/cardreader.h"
#include "../../module/printcounter.h"
#include "../../libs/duration_t.h"
#include "../../module/temperature.h"
#include "../../gcode/queue.h"
#include "../../module/motion.h"
#include "../../module/planner.h"
#include "../../feature/babystep.h"
#include "../../module/probe.h"

#include "lcd_dwin_ct300.h"

#if ENABLED(DWIN_CREALITY_RTS)

  // 音频 ========================
  #define RECORD_NORMAL_PUSH    0x0E
  #define RECORD_UNNORMAL_PUSH  0x12
  #define RECORD_PRINT_START    0x09
  #define RECORD_PRINT_FINISH   0x06
  #define RECORD_DISK_POPUP     0x02
  #define RECORD_DISK_INSERT    0x04
  #define RECORD_POP_WINDOW     0x07
  #define RECORD_DEVICE_START   0x0D
  #define RECORD_ACTION_FINISH  0x03
  #define RECORD_DEVICE_START_DURATION 8000 /*ms*/

  /*音频播放 recordNum:录音序号*/
  void Audio_Play(unsigned char recordNum)
  {
    // if((recordNum!=RECORD_DEVICE_START) && ((HAL_GetTick()-DeviceStart_PlayDelay)<RECORD_DEVICE_START_DURATION)) return;
    // if(Config_ReadVolume() == 0) return;
    // AUDIO_IIC_SDA(0);
    // Delay_Ms(4);
    // Audio_SendData(recordNum, 200);
  }

  /*音量设置 volume:音量(0~15)*/
  void Audio_SetVolume(unsigned char volume)
  {
    // if(volume > 15) return;
    // Config_WriteVolume(volume);
    // AUDIO_IIC_SDA(0);
    // Delay_Ms(4);
    // Audio_SendData(0xE0 + volume, 200);
  }

  /*音量获取(0~15)*/
  unsigned char Audio_GetVolume(void)
  {
    // return Config_ReadVolume();
  }

  // 内部声明 ==============================================================================
  void HMI_Init(void);
  void HMI_UpdateProcess(void);
  void HMI_UI_Process(void);
  void HMI_MCU_Process(void);
  void HMI_MCU_Request(unsigned char cmd);
  void HMI_SetVp_UpdateTime(unsigned short int vp, unsigned int time);
  bool HMI_IsVp_AttainSetTime(unsigned short int vp, unsigned int time);
  void HMI_SetStandBy(bool state);
  bool HMI_IsStandBy(void);
  
  void UART_LCD_StartPage(void);
  void UART_LCD_MainPage(void);
  void MCU_StopPrint(void);
  void UI_StopPrint(void);
  void MCU_PauseResumePrint(void);
  void UI_PauseResumePrint(void);
  unsigned int Calculate_RemainTime(unsigned char timeType);
  void UART_LCD_Head0Page(void);
  #if EXTRUDERS > 1
    void UART_LCD_Head1Page(void);
  #endif
  void UART_LCD_ControlPage(void);
  void UART_LCD_Filament0_InPage(void);
  void UART_LCD_Filament0_OutPage(void);
  #if EXTRUDERS > 1
    void UART_LCD_Filament1_InPage(void);
    void UART_LCD_Filament1_OutPage(void);
  #endif

  void UART_LCD_SettingPage(void);
  void UART_LCD_AboutPage(void);
  void UART_LCD_LanguagePage(void);
  void UI_LanguageChange(unsigned char language);
  void UART_LCD_FilePage(void);
  void MCU_FlipOver_FileList(unsigned char page, unsigned char fNum_PerPage);
  void UI_FlipOver_FileList(unsigned char page);
  void MCU_StartPrint(char *filename);
  void UI_StartPrint(char *filename);
  void UI_PreviewShow(char *filename);
  void UART_LCD_WindowPage_FilamentLack(void);
  void UART_LCD_WindowPage_PowerRecover(void);
  void UART_LCD_WindowPage_TempError(void);
  void MCU_PowerRecoverPrint(bool print);
  void UART_LCD_StandbyPage(void);

  // 内部变量 ==============================================================================
  // 触控类型 ===
  typedef struct
  {
    unsigned int Value;
    unsigned char Type;
  }DWIN_ReceivedTypeDef;

  unsigned char DWIN_SendBuf[260] = {0x5A, 0xA5, 0x00, 0x82};

  DWIN_ReceivedTypeDef DWIN_receivedType;

  // HMI ===
  #define MCU_REQUEST_MAX_NUM  3
  /*人机交互进入休眠时间(s)*/
  #define HMI_STANDBY_DEACTIVE_TIME  120

  /*界面函数指针*/
  typedef void (*pageFunction)(void);

  pageFunction currentPage = UART_LCD_StartPage;
  pageFunction window_lastPage, standby_lastpage;
  bool refreshFlag = true;
  unsigned int Vp_UpdateTime[10];
  static unsigned char HMI_RequestCommands[MCU_REQUEST_MAX_NUM];
  static unsigned char HMI_Commands_RIndex=0, HMI_Commands_WIndex=0, HMI_Commands_Length=0;
  /*非活跃状态自动关闭相关时间变量*/
  unsigned int previous_millis_lcd=0, lcd_inactive_time=HMI_STANDBY_DEACTIVE_TIME*1000;
  bool hmi_standby = false;

  // 文件界面 ===
  typedef struct
  {
    char filename[FILENAME_LENGTH];
    unsigned char month;
    unsigned char day;
    unsigned char hour;
    unsigned char min;
  }PrintFile_InfoTypeDef;

  unsigned char File_CurrentPage=0, File_TotalPage=0;
  PrintFile_InfoTypeDef printFile_Info[7];

  enum {
    RTC_TIME_HOUR,
    RTC_TIME_MIN,
  };


  // 临时处理 ==========================================================================
  // 配置存储 ========================
  typedef struct
  {
    unsigned char System_Language;
    unsigned char Head_Num;
    unsigned char LCD_Luminance;
    unsigned char Audio_Volume;
    float         Z_Axis_Offset;
  }Config_StoreTypeDef;

  Config_StoreTypeDef Config_StoreStruct = {0, 1, 100, 3, 0.0};

  /*读取配置机器语言*/
  unsigned char Config_ReadLanguage(void)
  {
    return Config_StoreStruct.System_Language;
  }

  /*写入配置机器语言 language:语言*/
  void Config_WriteLanguage(unsigned char language)
  {
    if(language <= 7)	Config_StoreStruct.System_Language = language;
  }

  /*读取配置喷头个数(1~2)*/
  unsigned char Config_ReadHeadNum(void)
  {
    return Config_StoreStruct.Head_Num;
  }

  /*写入配置喷头个数 num:个数*/
  void Config_WriteHeadNum(unsigned char num)
  {
    if(num==1 || num==2) Config_StoreStruct.Head_Num = num;
  }

  /*读取配置屏幕亮度*/
  unsigned char Config_ReadLuminance(void)
  {
    return Config_StoreStruct.LCD_Luminance;
  }

  /*写入配置屏幕亮度 luminance:亮度*/
  void Config_WriteLuminance(unsigned char luminance)
  {
    if(luminance <= 100) Config_StoreStruct.LCD_Luminance = luminance;
  }

  /*读取配置系统音量(0~15)*/
  unsigned char Config_ReadVolume(void)
  {
    return Config_StoreStruct.Audio_Volume;
  }

  /*写入配置系统音量 volume:音量*/
  void Config_WriteVolume(unsigned char volume)
  {
    if(volume <= 15) Config_StoreStruct.Audio_Volume = volume;
  }

  /*读取配置Z轴偏移*/
  float Config_ReadZAxisOff(void)
  {
    return Config_StoreStruct.Z_Axis_Offset;
  }

  /*写入配置Z轴偏移 axisOff:偏移*/
  void Config_WriteZAxisOff(float axisOff)
  {
    Config_StoreStruct.Z_Axis_Offset = axisOff;
  }

  // 存储设备 ===
  /*储存设备是否开始打印*/
  bool StorageDevice_IsStartPrint(void)
  {
    return card.isPrinting();
  }

  uint8_t get_gcode_num(void)
  {
    if(!card.isMounted()){
      return 0;
    }

    uint16_t fileCnt = card.get_num_Files(); // 获取gcode文件总数
    card.getWorkDirName(); // 获取根目录

    uint8_t gcode_count = 0;
    for(unsigned char count = 0; count < fileCnt; count++)
    {
      card.selectFileByIndex(fileCnt - 1 - count); // 选择文件
      char *pointFilename = card.longFilename; // 获取文件名
      int filenamelen = strlen(card.longFilename); // 获取文件长度
      if(filenamelen > LONG_FILENAME_LENGTH) continue; // 文件名太长

      // 判断gcode文件
      int j = 1;
      while((strncmp(&pointFilename[j], ".gcode", 6) && strncmp(&pointFilename[j], ".GCODE", 6)) 
        && ((j ++) < filenamelen)); // 判断是否为gcode文件
      if(j >= filenamelen)
      {
        continue;  
      }

      gcode_count++;
    }
    
    return gcode_count;
  }

  /*储存设备获取指定数量g文件信息 num:获取的g文件数量 start:从第start个g文件开始获取 *file:文件信息指针 返回值:实际获取文件数*/
  unsigned char StorageDevice_GetFilesInfo(unsigned char num, unsigned char start, PrintFile_InfoTypeDef *file)
  {
    if(!card.isMounted()){
      return 0;
    }

    uint16_t fileCnt = card.get_num_Files(); // 获取gcode文件总数
    card.getWorkDirName(); // 获取根目录
    if(card.filename[0] != '/')
    {
      card.cdup();
    }

    uint8_t gcode_count = 0;
    for(unsigned char count = 0; count < (start+num); count++)
    {
      card.selectFileByIndex(fileCnt - 1 - count); // 选择文件
      char *pointFilename = card.filename; // 获取文件名
      int filenamelen = strlen(pointFilename); // 获取文件长度
      if(filenamelen > LONG_FILENAME_LENGTH) continue;
      int j = 1;
      while((strncmp(&pointFilename[j], ".GCO", 6) && strncmp(&pointFilename[j], ".gco", 6)) 
        && ((j ++) < filenamelen)); // 判断是否为gcode文件
      if(j >= filenamelen)
      {
        continue;  
      }

      gcode_count++;
      if(gcode_count >= start)
      {
        strcpy(file[gcode_count-start].filename, pointFilename);
        file[gcode_count-start].month = (0 >> 5) & 0x0F;
        file[gcode_count-start].day = 0 & 0x1F;
        file[gcode_count-start].hour = (0 >> 11) & 0x1F;
        file[gcode_count-start].min = (0 >> 5) & 0x3F;
      }
    }
    
    return (gcode_count - start);
  }


  // 触控类型定义 ==========================================================================
  #define TOUCH_TYPE_NO_TOUCH          0x00
  #define TOUCH_TYPE_HEAD_0_JUMP       0x01
  #define TOUCH_TYPE_HEAD_1_JUMP       0x02
  #define TOUCH_TYPE_HEAD_TEMP_SET     0x03
  #define TOUCH_TYPE_HEAD_BACK         0x04
  #define TOUCH_TYPE_BED_TEMP_SET      0x05
  #define TOUCH_TYPE_STOP              0x06
  #define TOUCH_TYPE_PAUSE_START       0x07
  #define TOUCH_TYPE_HOME_PAGE         0x08
  #define TOUCH_TYPE_CONTROL_PAGE      0x09
  #define TOUCH_TYPE_FILE_PAGE         0x0A
  #define TOUCH_TYPE_SETTING_PAGE      0x0B
  #define TOUCH_TYPE_FAN_PUSH          0x0C
  #define TOUCH_TYPE_AXIS_UNIT         0x0D
  #define TOUCH_TYPE_MOTOR_FREE        0x0E
  #define TOUCH_TYPE_AXIS_X_PLUS       0x0F
  #define TOUCH_TYPE_AXIS_X_SUB        0x10
  #define TOUCH_TYPE_AXIS_Y_PLUS       0x11
  #define TOUCH_TYPE_AXIS_Y_SUB        0x12
  #define TOUCH_TYPE_AXIS_Z_PLUS       0x13
  #define TOUCH_TYPE_AXIS_Z_SUB        0x14
  #define TOUCH_TYPE_AXIS_XY_HOME      0x15
  #define TOUCH_TYPE_AXIS_Z_HOME       0x16
  #define TOUCH_TYPE_INOUT_SELECT      0x17
  #define TOUCH_TYPE_INOUT_IN_JUMP     0x18
  #define TOUCH_TYPE_INOUT_OUT_JUMP    0x19
  #define TOUCH_TYPE_INOUT_LENGTH      0x1A
  #define TOUCH_TYPE_INOUT_CONFIRM     0x1B
  #define TOUCH_TYPE_INOUT_CANCEL      0x1C
  #define TOUCH_TYPE_FILE_FLIP_PRE     0x1D
  #define TOUCH_TYPE_FILE_FLIP_NEXT    0x1E
  #define TOUCH_TYPE_FILE_SELECT       0x1F
  #define TOUCH_TYPE_FILE_PRINT        0x20
  #define TOUCH_TYPE_BOX_FAN_SET       0x21
  #define TOUCH_TYPE_BOX_LED_SET       0x22
  #define TOUCH_TYPE_LCD_LUMINANCE     0x23
  #define TOUCH_TYPE_SYSTEM_VOLUME     0x24
  #define TOUCH_TYPE_Z_MAKE_UP_PLUS    0x25
  #define TOUCH_TYPE_Z_MAKE_UP_SUB     0x26
  #define TOUCH_TYPE_SPEED_RATE        0x27
  #define TOUCH_TYPE_HEAD_NUM          0x28
  #define TOUCH_TYPE_LANGUAGE_JUMP     0x29
  #define TOUCH_TYPE_LANGUAGE_SELECT   0x2A
  #define TOUCH_TYPE_LANGUAGE_CONFIRM  0x2B
  #define TOUCH_TYPE_ABOUT_JUMP        0x2C
  #define TOUCH_TYPE_ABOUT_BACK        0x2D
  #define TOUCH_TYPE_WINDOW_CONFIRM    0x2E
  #define TOUCH_TYPE_WINDOW_CANCEL     0x2F
  #define TOUCH_TYPE_WAKE_UP           0x30


  static inline void dwin_uart_write(unsigned char *buf, int len) {
    // LOOP_L_N(n, len) { MYSERIAL1.write(buf[n]); delayMicroseconds(1); }
    LOOP_L_N(n, len) { MYSERIAL1.write(buf[n]); }
  }

  /*接收数据解析*/
  DWIN_ReceivedTypeDef DWIN_ReceiveAnalyze(void)
  {
    DWIN_ReceivedTypeDef receivedType = {0, TOUCH_TYPE_NO_TOUCH};
    unsigned short int addr, value;
    static bool recflag = false;
    static int recnum = 0;
    static unsigned char databuf[16];

    // 校验数据帧
    if (MYSERIAL1.available() > 0 && recnum < (signed)sizeof(databuf)) {
      databuf[recnum++] = MYSERIAL1.read();

      #define RECV_DEBUG
      #if defined(RECV_DEBUG)
        char buf[16];
        sprintf_P(buf, PSTR("%02x "), databuf[recnum - 1]);
        serialprintPGM(buf);
      #endif

      if(recnum == 1 && databuf[0] != 0x5A) {
        recnum = 0;
      }
      else if(recnum == 2 && databuf[1] != 0xA5) {
        recnum = 0;
      }
      else if(recnum >= 3 && databuf[2] == (recnum - 3)) {
        recflag = true;
        recnum = 0;

        #if defined(RECV_DEBUG)
          serialprintPGM("\n");
          SERIAL_ECHO_MSG("dwin rx ok");
        #endif
      }
    }

    if(recflag == true)
    {
      previous_millis_lcd = millis();
      addr = ((unsigned short int)databuf[4] << 8) | databuf[5];
      value = ((unsigned short int)databuf[7] << 8) | databuf[8];
      if(addr == 0x1100)
      {
        switch(value)
        {
          case 0x0001: receivedType.Type = TOUCH_TYPE_HEAD_0_JUMP; break;
          case 0x0002: receivedType.Type = TOUCH_TYPE_HEAD_1_JUMP; break;
          case 0x0003: receivedType.Type = TOUCH_TYPE_STOP; break;
          case 0x0004: receivedType.Type = TOUCH_TYPE_PAUSE_START; break;
          case 0x0005: receivedType.Type = TOUCH_TYPE_HOME_PAGE; break;
          case 0x0006: receivedType.Type = TOUCH_TYPE_CONTROL_PAGE; break;
          case 0x0007: receivedType.Type = TOUCH_TYPE_FILE_PAGE; break;
          case 0x0008: receivedType.Type = TOUCH_TYPE_SETTING_PAGE; break;
          case 0x0009: receivedType.Type = TOUCH_TYPE_FAN_PUSH; break;
          case 0x000A: receivedType.Type = TOUCH_TYPE_HEAD_BACK; break;
          case 0x000B: receivedType.Type = TOUCH_TYPE_MOTOR_FREE; break;
          case 0x000C: receivedType.Type = TOUCH_TYPE_AXIS_X_PLUS; break;
          case 0x000D: receivedType.Type = TOUCH_TYPE_AXIS_X_SUB; break;
          case 0x000E: receivedType.Type = TOUCH_TYPE_AXIS_Y_PLUS; break;
          case 0x000F: receivedType.Type = TOUCH_TYPE_AXIS_Y_SUB; break;
          case 0x0010: receivedType.Type = TOUCH_TYPE_AXIS_Z_PLUS; break;
          case 0x0011: receivedType.Type = TOUCH_TYPE_AXIS_Z_SUB; break;
          case 0x0012: receivedType.Type = TOUCH_TYPE_AXIS_XY_HOME; break;
          case 0x0013: receivedType.Type = TOUCH_TYPE_AXIS_Z_HOME; break;
          case 0x0014: receivedType.Type = TOUCH_TYPE_INOUT_SELECT; break;
          case 0x0015: receivedType.Type = TOUCH_TYPE_INOUT_IN_JUMP; break;
          case 0x0016: receivedType.Type = TOUCH_TYPE_INOUT_OUT_JUMP; break;
          case 0x0017: receivedType.Type = TOUCH_TYPE_FILE_PRINT; break;
          case 0x0018: receivedType.Type = TOUCH_TYPE_FILE_FLIP_PRE; break;
          case 0x0019: receivedType.Type = TOUCH_TYPE_FILE_FLIP_NEXT; break;
          case 0x001A: receivedType.Type = TOUCH_TYPE_Z_MAKE_UP_PLUS; break;
          case 0x001B: receivedType.Type = TOUCH_TYPE_Z_MAKE_UP_SUB; break;
          case 0x001C: receivedType.Type = TOUCH_TYPE_LANGUAGE_JUMP; break;
          case 0x001D: receivedType.Type = TOUCH_TYPE_ABOUT_JUMP; break;
          case 0x001E: receivedType.Type = TOUCH_TYPE_INOUT_CONFIRM; break;
          case 0x001F: receivedType.Type = TOUCH_TYPE_INOUT_CANCEL; break;
          case 0x0020: receivedType.Type = TOUCH_TYPE_LANGUAGE_CONFIRM; break;
          case 0x0021: receivedType.Type = TOUCH_TYPE_ABOUT_BACK; break;
          case 0x0022: receivedType.Type = TOUCH_TYPE_WINDOW_CONFIRM; break;
          case 0x0023: receivedType.Type = TOUCH_TYPE_WINDOW_CANCEL; break;
          default: receivedType.Type = TOUCH_TYPE_NO_TOUCH; break;
        }
      }
      else if(addr == 0x0014)
      {
        if(value == 0x0011) receivedType.Type = TOUCH_TYPE_WAKE_UP;
      }
      else
      {
        switch(addr)
        {
          case 0x1102: receivedType.Type = TOUCH_TYPE_FILE_SELECT; break;
          case 0x1104: receivedType.Type = TOUCH_TYPE_LCD_LUMINANCE; break;
          case 0x1106: receivedType.Type = TOUCH_TYPE_SYSTEM_VOLUME; break;
          case 0x1108: receivedType.Type = TOUCH_TYPE_HEAD_TEMP_SET; break;
          case 0x110A: receivedType.Type = TOUCH_TYPE_BED_TEMP_SET; break;
          case 0x110C: receivedType.Type = TOUCH_TYPE_INOUT_LENGTH; break;
          case 0x110E: receivedType.Type = TOUCH_TYPE_SPEED_RATE; break;
          case 0x8D3C: receivedType.Type = TOUCH_TYPE_AXIS_UNIT; break;
          case 0x8F31: receivedType.Type = TOUCH_TYPE_BOX_FAN_SET; break;
          case 0x8F32: receivedType.Type = TOUCH_TYPE_BOX_LED_SET; break;
          case 0x8F3D: receivedType.Type = TOUCH_TYPE_HEAD_NUM; break;
          case 0x8F47: receivedType.Type = TOUCH_TYPE_LANGUAGE_SELECT; break;
          default: receivedType.Type = TOUCH_TYPE_NO_TOUCH; break;
        }
        receivedType.Value = value;
      }
      recflag = false;
    }
    return receivedType;
  }

  /*整型变量刷新 addr:变量地址 value:变量 len:变量字节长度*/
  void DWIN_IntRefresh(unsigned short int addr, unsigned int value, unsigned char len)
  {
    DWIN_SendBuf[2] = 3 + len;
    DWIN_SendBuf[4] = addr >> 8;
    DWIN_SendBuf[5] = addr;
    if(len == 2)
    {
      DWIN_SendBuf[6] = value >> 8;
      DWIN_SendBuf[7] = value;
    }
    else
    {
      DWIN_SendBuf[6] = value >> 24;
      DWIN_SendBuf[7] = value >> 16;
      DWIN_SendBuf[8] = value >> 8;
      DWIN_SendBuf[9] = value;
    }

    dwin_uart_write(DWIN_SendBuf, len + 6);
  }

  /*数组变量刷新 addr:变量地址 *buf:数组指针 len:数组长度*/
  void DWIN_BufRefresh(unsigned short int addr, unsigned char *buf, unsigned char len)
  {
    unsigned char zero = 0;
    DWIN_SendBuf[2] = 3 + len;
    if(len%2 != 0) DWIN_SendBuf[2]++;
    DWIN_SendBuf[4] = addr >> 8;
    DWIN_SendBuf[5] = addr;

    dwin_uart_write(DWIN_SendBuf, 6);
    dwin_uart_write(buf, len);
    if(len%2 != 0)
    {
      dwin_uart_write(&zero, 1);
    }
  }

  /*浮点型变量刷新 addr:变量地址 value:四字节单精度变量*/
  void DWIN_FloatRefresh(unsigned short int addr, float value)
  {
    unsigned char *fvalue = (unsigned char*)&value;
    DWIN_SendBuf[2] = 7;
    DWIN_SendBuf[4] = addr >> 8;
    DWIN_SendBuf[5] = addr;
    DWIN_SendBuf[6] = fvalue[3];
    DWIN_SendBuf[7] = fvalue[2];
    DWIN_SendBuf[8] = fvalue[1];
    DWIN_SendBuf[9] = fvalue[0];

    dwin_uart_write(DWIN_SendBuf, 10);
  }

  /*字符串刷新 addr:变量地址 *string:字符串指针*/
  void DWIN_StringRefresh(unsigned short int addr, char *string)
  {
    unsigned char len = strlen(string);
    DWIN_SendBuf[2] = 3 + len;
    DWIN_SendBuf[4] = addr >> 8;
    DWIN_SendBuf[5] = addr;
    strcpy((char *)&DWIN_SendBuf[6], string);

    dwin_uart_write(DWIN_SendBuf, len + 6);
  }

  /*----------------------------------------------系统变量函数----------------------------------------------*/
  /*设定背光亮度 luminance:亮度(0~100)*/
  void DWIN_Backlight_SetLuminance(unsigned char luminance)
  {
    DWIN_IntRefresh(0x0082, (unsigned short int)luminance<<8, 2);
  }

  /*页面切换 pageID:页面ID*/
  void DWIN_SwitchPage(unsigned char pageID)
  {
    DWIN_IntRefresh(0x0084, 0x5A010000|pageID, 4);
  }

  /*音频播放 musicID:音频ID volume:音量(0x00~0x40)*/
  void DWIN_PlayMusic(unsigned char musicID, unsigned char volume)
  {
    DWIN_IntRefresh(0x00A0, 0x00010000|((unsigned int)musicID<<24)|((unsigned int)volume<<8), 4);
  }

  /*----------------------------------------------触控变量函数----------------------------------------------*/
  /*增量调节设定下限 sp:描述指针 limit:下限*/
  void DWIN_Increment_SetLowerLimit(unsigned short int sp, unsigned short int limit)
  {
    DWIN_IntRefresh(sp+0x18, limit, 2);
  }

  /*增量调节设定上限 sp:描述指针 limit:上限*/
  void DWIN_Increment_SetUpperLimit(unsigned short int sp, unsigned short int limit)
  {
    DWIN_IntRefresh(sp+0x1A, limit, 2);
  }

  /*----------------------------------------------显示变量函数----------------------------------------------*/
  /*变量显示设定起始坐标 sp:描述指针 startX:起始X坐标 startY:起始Y坐标*/
  void DWIN_Variate_SetCoord(unsigned short int sp, unsigned short int startX, unsigned short int startY)
  {
    DWIN_IntRefresh(sp+0x01, (unsigned int)startX<<16 | startY, 4);
  }

  /*变量显示改变颜色 sp:描述指针 color:颜色*/
  void DWIN_Variate_SetColor(unsigned short int sp, unsigned short int color)
  {
    DWIN_IntRefresh(sp+0x03, color, 2);
  }

  /*变量显示更改字体大小 sp:描述指针 font_ID:ASII字符字库位置 font_XDots:字体X方向点阵数*/
  void DWIN_Variate_SetFontSize(unsigned short int sp, unsigned char font_ID, unsigned char font_XDots)
  {
    DWIN_IntRefresh(sp+0x04, (unsigned short int)font_ID<<8 | font_XDots, 2);
  }

  /*变量显示更改变量指针 sp:描述指针 vp:变量指针*/
  void DWIN_Variate_SetVp(unsigned short int sp, unsigned short int vp)
  {
    DWIN_IntRefresh(sp, vp, 2);
  }

  /*文本显示设定起始坐标 sp:描述指针 startX:起始X坐标 startY:起始Y坐标*/
  void DWIN_Text_SetCoord(unsigned short int sp, unsigned short int startX, unsigned short int startY)
  {
    DWIN_IntRefresh(sp+0x01, (unsigned int)startX<<16 | startY, 4);
  }

  /*文本显示改变颜色 sp:描述指针 color:颜色*/
  void DWIN_Text_SetColor(unsigned short int sp, unsigned short int color)
  {
    DWIN_IntRefresh(sp+0x03, color, 2);
  }

  /*文本显示更改字库位置 sp:描述指针 font0_ID:ASII字符字库位置 font1_ID:非ASCII字符字库位置*/
  void DWIN_Text_SetFontID(unsigned short int sp, unsigned char font0_ID, unsigned char font1_ID)
  {
    DWIN_IntRefresh(sp+0x09, (unsigned short int)font0_ID<<8 | font1_ID, 2);
  }

  /*文本显示更改字体大小 sp:描述指针 font_XDots:字体X方向点阵数 font_YDots:字体Y方向点阵数*/
  void DWIN_Text_SetFontSize(unsigned short int sp, unsigned char font_XDots, unsigned char font_YDots)
  {
    DWIN_IntRefresh(sp+0x0A, (unsigned short int)font_XDots<<8 | font_YDots, 2);
  }

  /*文本显示清空文本 vp:变量指针 len:文本长度*/
  void DWIN_Text_Clear(unsigned short int vp, unsigned char len)
  {
    DWIN_SendBuf[2] = 3 + len*2;
    DWIN_SendBuf[4] = vp >> 8;
    DWIN_SendBuf[5] = vp;
    memset(&DWIN_SendBuf[6], 0x20, len*2);

    dwin_uart_write(DWIN_SendBuf, len * 2 + 6);
  }

  /*隐藏文本/数据变量 sp:描述指针*/
  void DWIN_Hide(unsigned short int sp)
  {
    DWIN_IntRefresh(sp, 0xFF00, 2);
  }

  /*显示文本/数据变量 vp:变量指针 sp:描述指针*/
  void DWIN_Show(unsigned short int vp, unsigned short int sp)
  {
    DWIN_IntRefresh(sp, vp, 2);
  }

  // HMI =============================================================================

  /*背景图片ID*/
  #define UPDATE_PAGE_ID     0x00
  #define START_PAGE_ID      0x01
  #define MAIN_PAGE_ID       0x02
  #define HEAD_PAGE_ID       0x03
  #define CONTROL_PAGE_ID    0x04
  #define FILE_PAGE_ID       0x05
  #define SETTING_PAGE_ID    0x06
  #define FILAMENT_PAGE_ID   0x08
  #define LANGUAGE_PAGE_ID   0x09
  #define ABOUT_PAGE_ID      0x0A
  #define WINDOWE_PAGE_ID    0x0B
  #define STANDBY_PAGE_ID    0x10

  /*变量指针*/
  #define LCD_LUMINANCE_TOUCH_VP       0x1104  /*屏幕亮度++/--触摸指针*/
  #define SYSTEM_VOLUME_TOUCH_VP       0x1106  /*系统音量++/--触摸指针*/
  #define START_PROCESS_ICON_VP        0x14BD  /*开机进度位变量*/
  #define HOME_PAGE_TITLE_VP           0x14BE  /*首页页面文本图标*/
  #define CONTROL_PAGE_TITLE_VP        0x14BF  /*机器控制页面文本图标*/
  #define FILE_PAGE_TITLE_VP           0x14C0  /*模型打印页面文本图标*/
  #define SETTING_PAGE_TITLE_VP        0x14C1  /*设置页面文本图标*/
  #define HEAD0_CIRCULAR_ICON_VP       0x14C2  /*喷头0圆形变量图标*/
  #define HEAD1_CIRCULAR_ICON_VP       0x14C3  /*喷头1圆形变量图标*/
  #define BED_CIRCULAR_ICON_VP         0x14C4  /*热床圆形变量图标*/
  #define HEAD0_SET_TEMP_VP            0x14C5  /*喷头0设定温度*/
  #define HEAD0_CURRENT_TEMP_VP        0x14C9  /*喷头0实际温度*/
  #define HEAD1_SET_TEMP_VP            0x14CD  /*喷头1设定温度*/
  #define HEAD1_CURRENT_TEMP_VP        0x14D1  /*喷头1实际温度*/
  #define BED_SET_TEMP_VP              0x14D5  /*热床设定温度*/
  #define BED_CURRENT_TEMP_VP          0x14D9  /*热床实际温度*/
  #define HEAD0_TITLE_VP               0x14DD  /*喷头0文本图标*/
  #define HEAD1_TITLE_VP               0x14DE  /*喷头1文本图标*/
  #define BED_TITLE_VP                 0x14DF  /*热床文本图标*/
  #define PRINT_FILE_TITLE_VP          0x14E0  /*当前文件文本图标*/
  #define PRINT_TIME_TITLE_VP          0x14E1  /*打印时间文本图标*/
  #define PRINT_PROCESS_TITLE_VP       0x14E2  /*打印进度文本图标*/
  #define PRINT_REMAIN_TITLE_VP        0x14E3  /*预计剩余文本图标*/
  #define PRINT_FILE_TEXT_VP           0x14E4  /*当前文件名称显示*/
  #define PRINT_TIME_HOUR_VP           0x1516  /*打印时间小时*/
  #define PRINT_TIME_MIN_VP            0x151A  /*打印时间分钟*/
  #define PRINT_TIME_SEC_VP            0x151E  /*打印时间秒*/
  #define PRINT_REMAIN_HOUR_VP         0x1522  /*预计剩余小时*/
  #define PRINT_REMAIN_MIN_VP          0x1526  /*预计剩余分钟*/
  #define PRINT_PROCESS_VP             0x152A  /*打印进度数据变量*/
  #define PRINT_PROCESS_ICON_VP        0x152E  /*打印进度变量图标*/
  #define STOP_ICON_VP                 0x152F  /*停止变量图标*/
  #define PAUSE_RESUME_ICON_VP         0x1530  /*继续暂停变量图标*/
  #define STOP_TITLE_VP                0x1531  /*停止文本图标*/
  #define PAUSE_RESUME_TITLE_VP        0x1532  /*继续暂停文本图标*/
  #define MODEL_PREVIEW_VP             0x1534  /*首页预览图(30k)*/
  #define HEAD_ANNULAR_ICON_VP         0x8D33  /*喷头环形变量图标*/
  #define HEAD_TEMP_TITLE_VP           0x8D34  /*喷头温度设置文本图标*/
  #define HEAD_FAN_TITLE_VP            0x8D35  /*喷头风扇设置文本图标*/
  #define HEAD_FAN_TEXT_VP             0x8D36  /*喷头风扇状态文本图标*/
  #define HEAD_PAGE_BACK_VP            0x8D37  /*喷头页面返回文本图标*/
  #define HEAD_FAN_ICON_VP             0x8D38  /*喷头风扇状态变量图标*/
  #define AXIS_MOVE_TITLE_VP           0x8D39  /*移动轴文本图标*/
  #define AXIS_UNIT_TITLE_VP           0x8D3A  /*移动单位文本图标*/
  #define MOTOR_FREE_TITLE_VP          0x8D3B  /*释放电机文本图标*/
  #define AXIS_UNIT_ICON_VP            0x8D3C  /*轴移动单位位变量*/
  #define AXIS_X_COORD_VP              0x8D3D  /*X轴坐标*/
  #define AXIS_Y_COORD_VP              0x8D41  /*Y轴坐标*/
  #define AXIS_Z_COORD_VP              0x8D45  /*Z轴坐标*/
  #define INOUT_TITLE_VP               0x8D49  /*进退料文本图标*/
  #define INOUT_SELECT_TITLE_VP        0x8D4A  /*切换喷头文本图标*/
  #define INOUT_IN_TITLE_VP            0x8D4B  /*进料文本图标*/
  #define INOUT_OUT_TITLE_VP           0x8D4C  /*退料文本图标*/
  #define INOUT_SELECT_ICON_VP         0x8D4D  /*切换喷头变量图标*/
  #define FILE_SELECT_ICON_VP          0x8D4E  /*模型文件选中框位变量*/
  #define FILE1_TEXT_VP                0x8D4F  /*模型文件名1显示*/
  #define FILE2_TEXT_VP                0x8D81  /*模型文件名2显示*/
  #define FILE3_TEXT_VP                0x8DB3  /*模型文件名3显示*/
  #define FILE4_TEXT_VP                0x8DE5  /*模型文件名4显示*/
  #define FILE5_TEXT_VP                0x8E17  /*模型文件名5显示*/
  #define FILE6_TEXT_VP                0x8E49  /*模型文件名6显示*/
  #define FILE7_TEXT_VP                0x8E7B  /*模型文件名7显示*/
  #define FILE1_MONTH_VP               0x8EAD  /*模型文件1日期月*/
  #define FILE2_MONTH_VP               0x8EB1  /*模型文件2日期月*/
  #define FILE3_MONTH_VP               0x8EB5  /*模型文件3日期月*/
  #define FILE4_MONTH_VP               0x8EB9  /*模型文件4日期月*/
  #define FILE5_MONTH_VP               0x8EBD  /*模型文件5日期月*/
  #define FILE6_MONTH_VP               0x8EC1  /*模型文件6日期月*/
  #define FILE7_MONTH_VP               0x8EC5  /*模型文件7日期月*/
  #define FILE1_DAY_VP                 0x8EC9  /*模型文件1日期日*/
  #define FILE2_DAY_VP                 0x8ECD  /*模型文件2日期日*/
  #define FILE3_DAY_VP                 0x8ED1  /*模型文件3日期日*/
  #define FILE4_DAY_VP                 0x8ED5  /*模型文件4日期日*/
  #define FILE5_DAY_VP                 0x8ED9  /*模型文件5日期日*/
  #define FILE6_DAY_VP                 0x8EDD  /*模型文件6日期日*/
  #define FILE7_DAY_VP                 0x8EE1  /*模型文件7日期日*/
  #define FILE1_HOUR_VP                0x8EE5  /*模型文件1日期小时*/
  #define FILE2_HOUR_VP                0x8EE9  /*模型文件2日期小时*/
  #define FILE3_HOUR_VP                0x8EED  /*模型文件3日期小时*/
  #define FILE4_HOUR_VP                0x8EF1  /*模型文件4日期小时*/
  #define FILE5_HOUR_VP                0x8EF5  /*模型文件5日期小时*/
  #define FILE6_HOUR_VP                0x8EF9  /*模型文件6日期小时*/
  #define FILE7_HOUR_VP                0x8EFD  /*模型文件7日期小时*/
  #define FILE1_MIN_VP                 0x8F01  /*模型文件1日期分钟*/
  #define FILE2_MIN_VP                 0x8F05  /*模型文件2日期分钟*/
  #define FILE3_MIN_VP                 0x8F09  /*模型文件3日期分钟*/
  #define FILE4_MIN_VP                 0x8F0D  /*模型文件4日期分钟*/
  #define FILE5_MIN_VP                 0x8F11  /*模型文件5日期分钟*/
  #define FILE6_MIN_VP                 0x8F15  /*模型文件6日期分钟*/
  #define FILE7_MIN_VP                 0x8F19  /*模型文件7日期分钟*/
  #define FILE_CURRENT_PAGE_VP         0x8F1D  /*模型文件页分子*/
  #define FILE_TOTAL_PAGE_VP           0x8F21  /*模型文件页分母*/
  #define FILE_PREVIOUS_PAGE_TITLE_VP  0x8F25  /*模型文件上一页图标*/
  #define FILE_NEXT_PAGE_TITLE_VP      0x8F26  /*模型文件下一页图标*/
  #define FILE_PRINT_TITLE_VP          0x8F27  /*模型文件打印图标*/
  #define BOX_FAN_TITLE_VP             0x8F28  /*机箱风扇文本图标*/
  #define BOX_LED_TITLE_VP             0x8F29  /*照明控制文本图标*/
  #define LCD_LUMINANCE_TITLE_VP       0x8F2A  /*屏幕亮度文本图标*/
  #define SYSTEM_VOLUME_TITLE_VP       0x8F2B  /*系统音量文本图标*/
  #define Z_MAKE_UP_TITLE_VP           0x8F2C  /*Z轴补偿文本图标*/
  #define PRINT_SPEED_RATE_TITLE_VP    0x8F2D  /*打印速率文本图标*/
  #define HEAD_NUM_TITLE_VP            0x8F2E  /*喷头个数文本图标*/
  #define SYSTEM_LANGUAGE_TITLE_VP     0x8F2F  /*语言选择文本图标*/
  #define ABOUT_TITLE_VP               0x8F30  /*关于本机文本图标*/
  #define BOX_FAN_ICON_VP              0x8F31  /*机箱风扇状态位变量*/
  #define BOX_LED_ICON_VP              0x8F32  /*照明状态位变量*/
  #define LCD_LUMINANCE_ICON_VP        0x8F33  /*屏幕亮度位变量*/
  #define SYSTEM_VOLUME_ICON_VP        0x8F34  /*系统音量位变量*/
  #define Z_MAKE_UP_VP                 0x8F35  /*Z轴补偿变量*/
  #define PRINT_SPEED_RATE_VP          0x8F39  /*打印速率变量*/
  #define HEAD_NUM_ICON_VP             0x8F3D  /*喷头个数位变量*/
  #define SYSTEM_LANGUAGE_TEXT_VP      0x8F3E  /*当前语言文本图标*/
  #define INOUT_WARNING_TITLE_VP       0x8F3F  /*进退料页面提示文本图标*/
  #define INOUT_LENGTH_TITLE_VP        0x8F40  /*当前进退料长度文本图标*/
  #define INOUT_LENGTH_VP              0x8F41  /*进退料长度数据变量*/
  #define INOUT_CONFIRM_TITLE_VP       0x8F45  /*进退料页面确认文本图标*/
  #define INOUT_CANCEL_TITLE_VP        0x8F46  /*进退料页面取消文本图标*/
  #define SYSTEM_LANGUAGE_ICON_VP      0x8F47  /*系统语言位变量*/
  #define LANGUAGE_CONFIRM_TITLE_VP    0x8F48  /*系统语言确认文本图标*/
  #define ABOUT1_TITLE_VP              0x8F49  /*关于本机1标题图标*/
  #define ABOUT2_TITLE_VP              0x8F4A  /*关于本机2标题图标*/
  #define ABOUT3_TITLE_VP              0x8F4B  /*关于本机3标题图标*/
  #define ABOUT4_TITLE_VP              0x8F4C  /*关于本机4标题图标*/
  #define ABOUT1_TEXT_VP               0x8F4D  /*关于本机1文本*/
  #define ABOUT2_TEXT_VP               0x8F7F  /*关于本机2文本*/
  #define ABOUT3_TEXT_VP               0x8FB1  /*关于本机3文本*/
  #define ABOUT4_TEXT_VP               0x8FE3  /*关于本机4文本*/
  #define ABOUT_BACK_TITLE_VP          0x9015  /*关于本机返回文本图标*/
  #define WINDOW_TITLE_VP              0x9016  /*弹窗标题文本图标*/
  #define WINDOW_TEXT_VP               0x9017  /*弹窗提示文本图标*/
  #define WINDOW_CONFIRM_TITLE_VP      0x9018  /*弹窗确定文本显示*/
  #define WINDOW_CANCEL_TITLE_VP       0x9019  /*弹窗取消文本显示*/
  #define STANDBY_LOGO_ICON_VP         0x901A  /*待机Logo图标*/
  #define STANDBY_ARROW_ICON_VP        0x901C  /*待机箭头图标*/

  /*MCU命令类型*/
  #define REQUEST_NOTHING           0x00
  #define REQUEST_UPDATE_FILE_LIST  0x01
  #define REQUEST_CLEAR_FILE_LIST   0x02
  #define REQUEST_FINISH_PRINT      0x03
  #define REQUEST_ABORT_PRINT       0x04
  #define REQUEST_FILAMENT_LACK     0x05
  #define REQUEST_POWER_RECOVER     0x06
  #define REQUEST_TEMPER_ERROR      0x07
  #define REQUEST_HMI_STANDBY       0x08

  /*控件字体状态颜色*/
  #define WIDGET_TEXT_ENABLE_COLOR   0xFFFF
  #define WIDGET_TEXT_DISABLE_COLOR  0x632C
  #define WIDGET_TEXT_BLUE_COLOR     0x45FA

  /*界面初始化*/
  void HMI_Init(void)
  {
    HMI_SetVp_UpdateTime(HEAD0_CURRENT_TEMP_VP, 0);
    HMI_SetVp_UpdateTime(HEAD1_CURRENT_TEMP_VP, 0);
    HMI_SetVp_UpdateTime(BED_CURRENT_TEMP_VP, 0);
    HMI_SetVp_UpdateTime(PRINT_TIME_SEC_VP, 0);
    HMI_SetVp_UpdateTime(PRINT_REMAIN_MIN_VP, 0);
    HMI_SetVp_UpdateTime(AXIS_X_COORD_VP, 0);
    if(Config_ReadHeadNum() < 2)
    {
      DWIN_IntRefresh(HEAD1_CIRCULAR_ICON_VP, 0, 2);
      //修改喷头1实际温度和设定温度地址
      DWIN_Hide(HEAD1_CURRENT_TEMP_VP); /*喷头1实际温度*/
      DWIN_Hide(HEAD1_SET_TEMP_VP); 		/*喷头1设定温度*/
      //修改喷头个数文本错误
      DWIN_IntRefresh(HEAD_NUM_ICON_VP, 0x01, 2);
    }
    else DWIN_IntRefresh(HEAD_NUM_ICON_VP, 0x02, 2);
    
    DWIN_IntRefresh(PRINT_TIME_HOUR_VP, 1000, 2);
    DWIN_IntRefresh(PRINT_TIME_MIN_VP, 100, 2);
    DWIN_IntRefresh(PRINT_TIME_SEC_VP, 100, 2);
    DWIN_IntRefresh(PRINT_REMAIN_HOUR_VP, 1000, 2);
    DWIN_IntRefresh(PRINT_REMAIN_MIN_VP, 100, 2);
    DWIN_IntRefresh(AXIS_UNIT_ICON_VP, 0x04, 2);
    DWIN_IntRefresh(STOP_ICON_VP, 0x0F, 2);
    DWIN_IntRefresh(PAUSE_RESUME_ICON_VP, 0x0F, 2);
    DWIN_Text_SetColor(0xE630, WIDGET_TEXT_DISABLE_COLOR); /*X坐标显示*/
    DWIN_Text_SetColor(0xE660, WIDGET_TEXT_DISABLE_COLOR); /*Y坐标显示*/
    DWIN_Text_SetColor(0xE690, WIDGET_TEXT_DISABLE_COLOR); /*Z坐标显示*/
    DWIN_IntRefresh(INOUT_SELECT_ICON_VP, 0x0F, 2);
    DWIN_IntRefresh(BOX_FAN_ICON_VP, 0x02, 2);
    DWIN_IntRefresh(BOX_LED_ICON_VP, 0x01, 2);
    DWIN_IntRefresh(LCD_LUMINANCE_TOUCH_VP, (Config_ReadLuminance()-10)/18, 2);
    DWIN_IntRefresh(LCD_LUMINANCE_ICON_VP, (0x03E0>>((Config_ReadLuminance()-10)/18))&0x001F, 2);
    DWIN_IntRefresh(SYSTEM_VOLUME_TOUCH_VP, Config_ReadVolume()/3, 2);
    DWIN_IntRefresh(SYSTEM_VOLUME_ICON_VP, (0x03E0>>(Config_ReadVolume()/3))&0x001F, 2);
    DWIN_FloatRefresh(Z_MAKE_UP_VP, Config_ReadZAxisOff());
    DWIN_IntRefresh(SYSTEM_LANGUAGE_ICON_VP, 0x01<<Config_ReadLanguage(), 2);
    DWIN_StringRefresh(ABOUT1_TEXT_VP, (char *)MACHINE_NAME);
    DWIN_StringRefresh(ABOUT2_TEXT_VP, (char *)SHORT_BUILD_VERSION);
    DWIN_StringRefresh(ABOUT3_TEXT_VP, (char *)PROTOCOL_VERSION);
    DWIN_StringRefresh(ABOUT4_TEXT_VP, (char *)MARLIN_WEBSITE_URL);
    UI_LanguageChange(Config_ReadLanguage());
  }

  /*界面更新进程*/
  void HMI_UpdateProcess(void)
  {
    static bool inserted_flag = false;
    if(Sd2Card::isInserted() != inserted_flag) {
      inserted_flag = Sd2Card::isInserted();
      if(inserted_flag == true) {
        HMI_MCU_Request(REQUEST_UPDATE_FILE_LIST);
      }
      else {
        HMI_MCU_Request(REQUEST_CLEAR_FILE_LIST);
      }
    }

    DWIN_receivedType = DWIN_ReceiveAnalyze();
    HMI_MCU_Process();
    HMI_UI_Process();
  }

  /*UI自动更新进程*/
  void HMI_UI_Process(void)
  {
    (*currentPage)();
  }

  /*MCU主动更新UI内容*/
  void HMI_MCU_Process(void)
  {
    unsigned char fileNum;
    if(HMI_Commands_Length)
    {
      switch(HMI_RequestCommands[HMI_Commands_RIndex])
      {
        case REQUEST_UPDATE_FILE_LIST:
        {
          fileNum = get_gcode_num();
          if(fileNum)
          {
            File_CurrentPage = 1;
            File_TotalPage = (fileNum+6) / 7;
            DWIN_IntRefresh(FILE_TOTAL_PAGE_VP, File_TotalPage, 2);
            MCU_FlipOver_FileList(1, 7);
            UI_FlipOver_FileList(1);
          }
          else
          {
            File_CurrentPage = 0;
            File_TotalPage = 0;
            DWIN_IntRefresh(FILE_TOTAL_PAGE_VP, File_TotalPage, 2);
            MCU_FlipOver_FileList(0, 7);
            UI_FlipOver_FileList(0);
          }
        }break;
        case REQUEST_CLEAR_FILE_LIST:
        {
          File_CurrentPage = 0;
          File_TotalPage = 0;
          DWIN_IntRefresh(FILE_TOTAL_PAGE_VP, File_TotalPage, 2);
          MCU_FlipOver_FileList(0, 7);
          UI_FlipOver_FileList(0);
        }break;
        case REQUEST_FINISH_PRINT:
        {
          Audio_Play(RECORD_PRINT_FINISH);

          duration_t time = duration_t(print_job_timer.duration());
          DWIN_IntRefresh(PRINT_TIME_HOUR_VP, time.hour(), 2);
          DWIN_IntRefresh(PRINT_TIME_MIN_VP, time.minute(), 2);
          DWIN_IntRefresh(PRINT_TIME_SEC_VP, time.second(), 2);
          DWIN_IntRefresh(PRINT_REMAIN_HOUR_VP, /* Calculate_RemainTime(RTC_TIME_HOUR)+1000 */ 0, 2);
          DWIN_IntRefresh(PRINT_REMAIN_MIN_VP, /* Calculate_RemainTime(RTC_TIME_MIN)+100 */ 0, 2);
          
          DWIN_IntRefresh(PRINT_PROCESS_VP, 100, 2);
          DWIN_IntRefresh(PRINT_PROCESS_ICON_VP, 49, 2);
          DWIN_IntRefresh(STOP_ICON_VP, 0x0F, 2);
          DWIN_IntRefresh(PAUSE_RESUME_ICON_VP, 0x0F, 2);
          DWIN_IntRefresh(STOP_TITLE_VP, (Config_ReadLanguage()+8), 2);
          DWIN_IntRefresh(PAUSE_RESUME_TITLE_VP, (Config_ReadLanguage()+16), 2);
          DWIN_IntRefresh(CONTROL_PAGE_TITLE_VP, (Config_ReadLanguage()+8), 2);
        }break;
        case REQUEST_ABORT_PRINT:
        {
          DWIN_IntRefresh(STOP_ICON_VP, 0x0F, 2);
          DWIN_IntRefresh(PAUSE_RESUME_ICON_VP, 0x0F, 2);
          DWIN_IntRefresh(STOP_TITLE_VP, (Config_ReadLanguage()+8), 2);
          DWIN_IntRefresh(PAUSE_RESUME_TITLE_VP, (Config_ReadLanguage()+16), 2);
          DWIN_IntRefresh(CONTROL_PAGE_TITLE_VP, (Config_ReadLanguage()+8), 2);
        }break;
        case REQUEST_FILAMENT_LACK:
        {
          Audio_Play(RECORD_POP_WINDOW);
          window_lastPage = currentPage;
          currentPage = UART_LCD_WindowPage_FilamentLack;
          refreshFlag = true;
        }break;
        case REQUEST_POWER_RECOVER:
        {
          Audio_Play(RECORD_POP_WINDOW);
          window_lastPage = currentPage;
          currentPage = UART_LCD_WindowPage_PowerRecover;
          refreshFlag = true;
        }break;
        case REQUEST_TEMPER_ERROR:
        {
          Audio_Play(RECORD_POP_WINDOW);
          window_lastPage = currentPage;
          currentPage = UART_LCD_WindowPage_TempError;
          refreshFlag = true;
        }break;
        case REQUEST_HMI_STANDBY:
        {
          standby_lastpage = currentPage;
          currentPage = UART_LCD_StandbyPage;
          refreshFlag = true;
        }break;
      }
      HMI_Commands_Length--;
      HMI_Commands_RIndex = (HMI_Commands_RIndex+1) % MCU_REQUEST_MAX_NUM;
    }
  }

  /*MCU更新命令入列*/
  void HMI_MCU_Request(unsigned char cmd)
  {
    if(HMI_Commands_Length < MCU_REQUEST_MAX_NUM)
    {
      HMI_RequestCommands[HMI_Commands_WIndex] = cmd;
      HMI_Commands_WIndex = (HMI_Commands_WIndex+1) % MCU_REQUEST_MAX_NUM;
      HMI_Commands_Length++;
    }
  }

  /*对指定变量设定更新时间 vp:变量地址 time:更新时间(ms)*/
  void HMI_SetVp_UpdateTime(unsigned short int vp, unsigned int time)
  {
    unsigned int upDate = millis() + time;
    switch(vp)
    {
      case HEAD0_CURRENT_TEMP_VP: Vp_UpdateTime[0] = upDate; break;
      case HEAD1_CURRENT_TEMP_VP: Vp_UpdateTime[1] = upDate; break;
      case BED_CURRENT_TEMP_VP: Vp_UpdateTime[2] = upDate; break;
      case PRINT_TIME_SEC_VP: Vp_UpdateTime[3] = upDate; break;
      case PRINT_REMAIN_MIN_VP: Vp_UpdateTime[4] = upDate; break;
      case AXIS_X_COORD_VP: Vp_UpdateTime[5] = upDate; break;
    }
  }

  /*查询指定变量是否到达更新时间 若到达则重设时间 vp:变量地址 time:更新时间(ms)*/
  bool HMI_IsVp_AttainSetTime(unsigned short int vp, unsigned int time)
  {
    unsigned int upDate = 0;
    switch(vp)
    {
      case HEAD0_CURRENT_TEMP_VP: upDate = Vp_UpdateTime[0]; break;
      case HEAD1_CURRENT_TEMP_VP: upDate = Vp_UpdateTime[1]; break;
      case BED_CURRENT_TEMP_VP: upDate = Vp_UpdateTime[2]; break;
      case PRINT_TIME_SEC_VP: upDate = Vp_UpdateTime[3]; break;
      case PRINT_REMAIN_MIN_VP: upDate = Vp_UpdateTime[4]; break;
      case AXIS_X_COORD_VP: upDate = Vp_UpdateTime[5]; break;
      default: return false;
    }
    if(upDate <= millis())
    {
      HMI_SetVp_UpdateTime(vp, time);
      return true;
    }
    return false;
  }

  /*设置人机交互待机模式 state:true,进入待机;false,退出待机*/
  void HMI_SetStandBy(bool state)
  {
    hmi_standby = state;
  }

  /*查询人机交互是否处于待机*/
  bool HMI_IsStandBy(void)
  {
    return hmi_standby;
  }

  // 开机界面 =====================================================================
  /*开机界面处理*/
  void UART_LCD_StartPage(void)
  {
    static unsigned char process = 0;
    /*仅刷一次变量*/
    if(refreshFlag)
    {
      Audio_Play(RECORD_DEVICE_START);
      DWIN_SwitchPage(START_PAGE_ID);
      DWIN_Backlight_SetLuminance(Config_ReadLuminance());
      refreshFlag = RESET;
    }
    /*固定刷新变量*/
    DWIN_IntRefresh(START_PROCESS_ICON_VP, (0xFFC00>>process++)&0x03FF, 2);
    if(process >= 11)
    {
      currentPage = UART_LCD_MainPage;
      refreshFlag = SET;
    }
    safe_delay(100);
  }

  // 主界面 =====================================================================
  /*首页界面处理*/
  void UART_LCD_MainPage(void)
  {
    static unsigned int temp_Head0SetTemp=0xFFFF, temp_BedSetTemp=0xFFFF;
  #if EXTRUDERS > 1
    static unsigned int temp_Head1SetTemp=0xFFFF;
  #endif
    static unsigned int temp_print_hour=0xFFFF, temp_print_min=0xFFFF;
    static unsigned char temp_print_process = 0xFF;
    unsigned int temp_Temper;
    /*仅刷一次变量*/
    if(refreshFlag)
    {
      DWIN_IntRefresh(HOME_PAGE_TITLE_VP, Config_ReadLanguage(), 2);
      DWIN_SwitchPage(MAIN_PAGE_ID);
      refreshFlag = RESET;
    }
    /*触控刷新变量*/
    if(DWIN_receivedType.Type != TOUCH_TYPE_NO_TOUCH)
    {
      switch(DWIN_receivedType.Type)
      {
        /*喷头0页面跳转*/
        case TOUCH_TYPE_HEAD_0_JUMP:
        {
          Audio_Play(RECORD_NORMAL_PUSH);
          currentPage = UART_LCD_Head0Page;
          refreshFlag = SET;
        }break;
      #if EXTRUDERS > 1
        /*喷头1页面跳转*/
        case TOUCH_TYPE_HEAD_1_JUMP:
        {
          Audio_Play(RECORD_NORMAL_PUSH);
          currentPage = UART_LCD_Head1Page;
          refreshFlag = SET;
        }break;
      #endif
        /*热床温度设定*/
        case TOUCH_TYPE_BED_TEMP_SET:
        {
          Audio_Play(RECORD_NORMAL_PUSH);
          temp_Temper = _MIN(DWIN_receivedType.Value, BED_MAXTEMP);
          thermalManager.setTargetBed(temp_Temper);
        }break;
        /*停止打印*/
        case TOUCH_TYPE_STOP:
        {
          UI_StopPrint();
          MCU_StopPrint();				
        }break;
        /*暂停继续打印*/
        case TOUCH_TYPE_PAUSE_START:
        {
          UI_PauseResumePrint();
          MCU_PauseResumePrint();
        }break;
        /*机器控制页面跳转*/
        case TOUCH_TYPE_CONTROL_PAGE:
        {
          if(!StorageDevice_IsStartPrint())
          {
            Audio_Play(RECORD_NORMAL_PUSH);
            DWIN_IntRefresh(HOME_PAGE_TITLE_VP, (Config_ReadLanguage()+8), 2);
            currentPage = UART_LCD_ControlPage;
            refreshFlag = true;
          }
          else Audio_Play(RECORD_UNNORMAL_PUSH);
        }break;
        /*打印文件页面跳转*/
        case TOUCH_TYPE_FILE_PAGE:
        {
          Audio_Play(RECORD_NORMAL_PUSH);
          DWIN_IntRefresh(HOME_PAGE_TITLE_VP, (Config_ReadLanguage()+8), 2);
          currentPage = UART_LCD_FilePage;
          refreshFlag = SET;
        }break;
        /*设置页面跳转*/
        case TOUCH_TYPE_SETTING_PAGE:
        {
          Audio_Play(RECORD_NORMAL_PUSH);
          DWIN_IntRefresh(HOME_PAGE_TITLE_VP, (Config_ReadLanguage()+8), 2);
          currentPage = UART_LCD_SettingPage;
          refreshFlag = SET;
        }break;
      }
    }
    /*固定刷新变量*/
    if(HMI_IsVp_AttainSetTime(HEAD0_CURRENT_TEMP_VP, 500))
    {
      DWIN_IntRefresh(HEAD0_CURRENT_TEMP_VP, (unsigned int)thermalManager.degHotend(0), 2);
      DWIN_IntRefresh(HEAD0_CIRCULAR_ICON_VP, constrain((unsigned int)thermalManager.degHotend(0)*50/HEATER_0_MAXTEMP,1,50), 2);
    }
    if(temp_Head0SetTemp != (unsigned int)thermalManager.degTargetHotend(0))
    {
      temp_Head0SetTemp = (unsigned int)thermalManager.degTargetHotend(0);
      DWIN_IntRefresh(HEAD0_SET_TEMP_VP, temp_Head0SetTemp, 2);
    }
  #if EXTRUDERS > 1
    if(Config_StoreStruct.Head_Num > 1)
    {
      if(HMI_IsVp_AttainSetTime(HEAD1_CURRENT_TEMP_VP, 500))
      {
        DWIN_IntRefresh(HEAD1_CURRENT_TEMP_VP, (unsigned int)thermalManager.degHotend(Temper_Head1), 2);
        DWIN_IntRefresh(HEAD1_CIRCULAR_ICON_VP, constrain((unsigned int)thermalManager.degHotend(Temper_Head1)*50/HEATER_0_MAXTEMP,1,50), 2);
      }
      if(temp_Head1SetTemp != (unsigned int)thermalManager.degTargetHotend(1))
      {
        temp_Head1SetTemp = (unsigned int)thermalManager.degTargetHotend(1);
        DWIN_IntRefresh(HEAD1_SET_TEMP_VP, temp_Head1SetTemp, 2);
      }
    }
  #endif
    if(HMI_IsVp_AttainSetTime(BED_CURRENT_TEMP_VP, 800))
    {
      DWIN_IntRefresh(BED_CURRENT_TEMP_VP, (unsigned int)thermalManager.degBed(), 2);
      DWIN_IntRefresh(BED_CIRCULAR_ICON_VP, constrain((unsigned int)thermalManager.degBed()*50/BED_MAXTEMP,1,50), 2);
    }
    if(temp_BedSetTemp != (unsigned int)thermalManager.degTargetBed())
    {
      temp_BedSetTemp = (unsigned int)thermalManager.degTargetBed();
      DWIN_IntRefresh(BED_SET_TEMP_VP, temp_BedSetTemp, 2);
    }
    if(card.isPrinting())
    {
      duration_t time = duration_t(print_job_timer.duration());
      unsigned int hour = time.hour() % 24;
      unsigned int minute = time.minute() % 60;
      unsigned int second = time.second() % 60;

      if(temp_print_hour != hour)
      {
        temp_print_hour = hour;
        DWIN_IntRefresh(PRINT_TIME_HOUR_VP, temp_print_hour+1000, 2);
      }
      if(temp_print_min != minute)
      {
        temp_print_min = minute;
        DWIN_IntRefresh(PRINT_TIME_MIN_VP, temp_print_min+100, 2);
      }
      if(HMI_IsVp_AttainSetTime(PRINT_TIME_SEC_VP, 500)) DWIN_IntRefresh(PRINT_TIME_SEC_VP, second+100, 2);
      if(HMI_IsVp_AttainSetTime(PRINT_REMAIN_MIN_VP, 20000))
      {
        DWIN_IntRefresh(PRINT_REMAIN_HOUR_VP, Calculate_RemainTime(RTC_TIME_HOUR)+1000, 2);
        DWIN_IntRefresh(PRINT_REMAIN_MIN_VP, Calculate_RemainTime(RTC_TIME_MIN)+100, 2);
      }
      if(temp_print_process != card.percentDone())
      {
        temp_print_process = card.percentDone();
        DWIN_IntRefresh(PRINT_PROCESS_VP, temp_print_process, 2);
        DWIN_IntRefresh(PRINT_PROCESS_ICON_VP, constrain(temp_print_process/2, 0, 49), 2);
      }
    }
  }

  /*MCU停止打印处理*/
  void MCU_StopPrint(void)
  {
    // char buf[30];
    // if(StorageDevice_IsStartPrint())
    // {
    //   Audio_Play(RECORD_NORMAL_PUSH);
    //   StorageDevice_StopPrint();
    //   Printer_ClearCommand();
    //   Stepper_QuickStop();
    //   StorageDevice_CloseFile();
    // #if POWEROFF_SAVE_FILE
    //   StorageDevice_DeleteFile(PowerOff_FileName);
    // #endif
    //   Heater_AutoShutdown();
    //   thermalManager.setTargetHotend(0, (Heater_TypeDef)active_extruder);
    //   thermalManager.setTargetBed(0);
    //   thermalManager.set_fan_speed(Fan_Model0, 0);
    //   queue.enqueue_one_now("M140 S0");
    //   queue.enqueue_one_now("M104 S0");
    //   sprintf_P(buf, PSTR("G1 F300 Z%f"), destination[Z_AXIS]+20);
    //   queue.enqueue_one_now(buf);
    //   queue.enqueue_one_now("G28 X Y");
    //   queue.enqueue_one_now("M84");
    //   Printer_Abort = true;
    // }

    if(StorageDevice_IsStartPrint())
    {
      card.flag.abort_sd_printing = true; // Let the main loop handle SD abort
    }
  }

  /*UI停止打印处理*/
  void UI_StopPrint(void)
  {
    if(StorageDevice_IsStartPrint())
    {
      DWIN_Text_Clear(PRINT_FILE_TEXT_VP, 30);
      DWIN_IntRefresh(PRINT_TIME_HOUR_VP, 1000, 2);
      DWIN_IntRefresh(PRINT_TIME_MIN_VP, 100, 2);
      DWIN_IntRefresh(PRINT_TIME_SEC_VP, 100, 2);
      DWIN_IntRefresh(PRINT_REMAIN_HOUR_VP, 1000, 2);
      DWIN_IntRefresh(PRINT_REMAIN_MIN_VP, 100, 2);
      DWIN_IntRefresh(PRINT_PROCESS_VP, 0, 2);
      DWIN_IntRefresh(PRINT_PROCESS_ICON_VP, 0, 2);
      DWIN_IntRefresh(STOP_ICON_VP, 0x0F, 2);
      DWIN_IntRefresh(PAUSE_RESUME_ICON_VP, 0x0F, 2);
      DWIN_IntRefresh(STOP_TITLE_VP, (Config_ReadLanguage()+8), 2);
      DWIN_IntRefresh(PAUSE_RESUME_TITLE_VP, (Config_ReadLanguage()+16), 2);
      DWIN_IntRefresh(CONTROL_PAGE_TITLE_VP, (Config_ReadLanguage()+8), 2);
      UI_PreviewShow(NULL);
    }
  }

  /*MCU暂停/继续打印处理*/
  void MCU_PauseResumePrint(void)
  {
    Audio_Play(RECORD_NORMAL_PUSH);
    if(card.isPrinting())
    {
      // StorageDevice_PausePrint();
      // queue.enqueue_one_now("G1 F1000 X20 Y150");
      // RTC_PauseTime();

      card.pauseSDPrint();
    }
    else
    {
      // StorageDevice_StartPrint();
      // RTC_ResumeTime();

      card.startFileprint();
    }
  }

  /*UI暂停/继续处理*/
  void UI_PauseResumePrint(void)
  {
    if(StorageDevice_IsStartPrint())
    {
      if(card.isPrinting())
      {
        DWIN_IntRefresh(PAUSE_RESUME_ICON_VP, 0, 2);
        DWIN_IntRefresh(PAUSE_RESUME_TITLE_VP, Config_ReadLanguage(), 2);
      }
      else
      {
        DWIN_IntRefresh(PAUSE_RESUME_ICON_VP, 1, 2);
        DWIN_IntRefresh(PAUSE_RESUME_TITLE_VP, (Config_ReadLanguage()+8), 2);
      }
    }
  }

  /*计算剩余打印时间(分钟为取余结果) timeType:时间类型(RTC_TIME_HOUR,小时; RTC_TIME_MIN,分钟)*/
  unsigned int Calculate_RemainTime(unsigned char timeType)
  {
    uint32_t remain_minute, elapsed_minute;
    uint8_t print_progress;
    duration_t time = duration_t(print_job_timer.duration());
    unsigned int hour = time.hour() % 24;
    unsigned int minute = time.minute() % 60;

    //获取打印进度
    print_progress = card.percentDone();
    if(print_progress > 100) print_progress = 100;
    //如果没有有效进度，则将剩余时间设置为最大999:99
    if(print_progress == 0) {
      if(timeType == RTC_TIME_HOUR) return 999;
      else return 99;
    }
    else {
      elapsed_minute = hour * 60 + minute;
      remain_minute = (uint32_t)((100.0 / print_progress - 1) * elapsed_minute);
      if(timeType == RTC_TIME_HOUR) return remain_minute / 60;
      else return remain_minute % 60;
    }
  }

  // 喷头界面 =====================================================================
  /*喷头0界面处理*/
  void UART_LCD_Head0Page(void)
  {
    static unsigned int temp_Head0SetTemp = 0xFFFF;
    unsigned int temp_Temper;
    /*仅刷一次变量*/
    if(refreshFlag)
    {
      DWIN_IntRefresh(HEAD_FAN_ICON_VP, thermalManager.fan_speed[0] ? 0 : 1, 2);
      DWIN_IntRefresh(HEAD_FAN_TEXT_VP, thermalManager.fan_speed[0] ? (Config_ReadLanguage()+8) : Config_ReadLanguage(), 2);
      DWIN_Variate_SetVp(0xE5D0, HEAD0_CURRENT_TEMP_VP);  /*实际温度指向喷头0*/
      DWIN_Variate_SetVp(0xE600, HEAD0_SET_TEMP_VP);  /*设定温度指向喷头0*/
      DWIN_SwitchPage(HEAD_PAGE_ID);
      refreshFlag = RESET;
    }
    /*触控刷新变量*/
    if(DWIN_receivedType.Type != TOUCH_TYPE_NO_TOUCH)
    {
      switch(DWIN_receivedType.Type)
      {
        /*喷头温度设定*/
        case TOUCH_TYPE_HEAD_TEMP_SET:
        {
          Audio_Play(RECORD_NORMAL_PUSH);
          temp_Temper = _MIN(DWIN_receivedType.Value, HEATER_0_MAXTEMP);
          thermalManager.setTargetHotend(temp_Temper, 0);
        }break;
        /*风扇控制*/
        case TOUCH_TYPE_FAN_PUSH:
        {
          Audio_Play(RECORD_NORMAL_PUSH);
          thermalManager.set_fan_speed(0, thermalManager.fan_speed[0] ? 0 : 255);
          DWIN_IntRefresh(HEAD_FAN_ICON_VP, thermalManager.fan_speed[0] ? 0 : 1, 2);
          DWIN_IntRefresh(HEAD_FAN_TEXT_VP, thermalManager.fan_speed[0] ? (Config_ReadLanguage()+8) : Config_ReadLanguage(), 2);
        }break;
        /*喷头页面返回*/
        case TOUCH_TYPE_HEAD_BACK:
        {
          Audio_Play(RECORD_NORMAL_PUSH);
          currentPage = UART_LCD_MainPage;
          refreshFlag = SET;
        }break;
      }
    }
    /*固定刷新变量*/
    if(HMI_IsVp_AttainSetTime(HEAD0_CURRENT_TEMP_VP, 500))
    {
      DWIN_IntRefresh(HEAD0_CURRENT_TEMP_VP, (unsigned int)thermalManager.degHotend(0), 2);
      if(labs((int)thermalManager.degHotend(0)-(int)thermalManager.degHotend(0)) > TEMP_HYSTERESIS)
        DWIN_IntRefresh(HEAD_ANNULAR_ICON_VP, constrain((unsigned int)thermalManager.degHotend(0)*50/HEATER_0_MAXTEMP,1,50)-1, 2);
      else
        DWIN_IntRefresh(HEAD_ANNULAR_ICON_VP, constrain((unsigned int)thermalManager.degHotend(0)*50/HEATER_0_MAXTEMP,2,50)+48, 2);
    }
    if(temp_Head0SetTemp != (unsigned int)thermalManager.degTargetHotend(0))
    {
      temp_Head0SetTemp = (unsigned int)thermalManager.degTargetHotend(0);
      DWIN_IntRefresh(HEAD0_SET_TEMP_VP, temp_Head0SetTemp, 2);
    }
  }

  #if EXTRUDERS > 1
  /*喷头1界面处理*/
  void UART_LCD_Head1Page(void)
  {
    unsigned int temp_Temper;
    static unsigned int temp_Head1SetTemp = 0xFFFF;
    /*仅刷一次变量*/
    if(refreshFlag)
    {
      DWIN_IntRefresh(HEAD_FAN_ICON, Fan_GetSpeed(Fan_Model1) ? 0 : 1, 2);
      DWIN_StringRefresh(HEAD_FAN_TEXT, Fan_GetSpeed(Fan_Model1) ? "点击关闭" : "点击开启");
      DWIN_SwitchPage(HEAD1_PAGE_ID);
      refreshFlag = RESET;
    }
    /*触控刷新变量*/
    if(DWIN_receivedType.Type != TOUCH_TYPE_NO_TOUCH)
    {
      switch(DWIN_receivedType.Type)
      {
        /*喷头1温度设定*/
        case TOUCH_TYPE_HEAD_1_SET:
        {
          Audio_Play(RECORD_NORMAL_PUSH);
          temp_Temper = _MIN(DWIN_receivedType.Value, HEATER_0_MAXTEMP);
          thermalManager.setTargetHotend(temp_Temper, 1);
        }break;
        /*风扇控制*/
        case TOUCH_TYPE_FAN_PUSH:
        {
          Audio_Play(RECORD_NORMAL_PUSH);
          thermalManager.set_fan_speed(Fan_Model1, Fan_GetSpeed(Fan_Model1) ? 0 : 255);
          DWIN_IntRefresh(HEAD_FAN_ICON, Fan_GetSpeed(Fan_Model1) ? 0 : 1, 2);
          DWIN_StringRefresh(HEAD_FAN_TEXT, Fan_GetSpeed(Fan_Model1) ? "点击关闭" : "点击开启");
        }break;
        /*喷头页面返回*/
        case TOUCH_TYPE_HEAD_BACK:
        {
          Audio_Play(RECORD_NORMAL_PUSH);
          currentPage = UART_LCD_MainPage;
          refreshFlag = SET;
        }break;
      }
    }
    /*固定刷新变量*/
    if(HMI_IsVp_AttainSetTime(HEAD1_CURRENT_TEMP, 500))
    {
      DWIN_IntRefresh(HEAD1_CURRENT_TEMP, (unsigned int)thermalManager.degHotend(Temper_Head1), 2);
      if(labs((int)thermalManager.degHotend(Temper_Head1)-(int)thermalManager.degTargetHotend(1)) > TEMP_HYSTERESIS)
        DWIN_IntRefresh(HEAD1_ANNULAR_ICON_VP, constrain((unsigned int)thermalManager.degHotend(Temper_Head1)*50/HEATER_0_MAXTEMP,1,50)-1, 2);
      else
        DWIN_IntRefresh(HEAD1_ANNULAR_ICON_VP, constrain((unsigned int)thermalManager.degHotend(Temper_Head1)*50/HEATER_0_MAXTEMP,2,50)+48, 2);
    }
    if(temp_Head1SetTemp != (unsigned int)thermalManager.degTargetHotend(1))
    {
      temp_Head1SetTemp = (unsigned int)thermalManager.degTargetHotend(1);
      DWIN_IntRefresh(HEAD1_SET_TEMP, temp_Head1SetTemp, 2);
    }
  }
  #endif

  // 机器控制界面 =====================================================================
  /*机器控制界面处理*/
  void UART_LCD_ControlPage(void)
  {
    static float axis_unit = 10.0;
    static unsigned char headNum = 1;
    char buf[30];
    char strfloat[16];
    /*仅刷一次变量*/
    if(refreshFlag)
    {
      DWIN_IntRefresh(CONTROL_PAGE_TITLE_VP, Config_ReadLanguage(), 2);
      DWIN_SwitchPage(CONTROL_PAGE_ID);
      refreshFlag = RESET;
    }
    /*触控刷新变量*/
    if(DWIN_receivedType.Type != TOUCH_TYPE_NO_TOUCH)
    {
      memset(buf, 0, 30);
      switch(DWIN_receivedType.Type)
      {
        /*轴移动单位选择*/
        case TOUCH_TYPE_AXIS_UNIT:
        {
          Audio_Play(RECORD_NORMAL_PUSH);
          axis_unit = ((DWIN_receivedType.Value&0x0001) ? (0.1) : (DWIN_receivedType.Value&0x0002 ? 1.0 : 10.0));
        }break;
        /*轴电机释放*/
        case TOUCH_TYPE_MOTOR_FREE:
        {
          Audio_Play(RECORD_NORMAL_PUSH);
          queue.enqueue_one_now("M84");
        }break;
        /*轴X+*/
        case TOUCH_TYPE_AXIS_X_PLUS:
        {
          Audio_Play(RECORD_NORMAL_PUSH);
          sprintf_P(buf, PSTR("G1 F1000 X%s"), dtostrf(current_position.x+axis_unit, 4, 2, strfloat));
          queue.enqueue_one_now(buf);
        }break;
        /*轴X-*/
        case TOUCH_TYPE_AXIS_X_SUB:
        {
          Audio_Play(RECORD_NORMAL_PUSH);
          sprintf_P(buf, PSTR("G1 F1000 X%s"), dtostrf(current_position.x-axis_unit, 4, 2, strfloat));
          queue.enqueue_one_now(buf);
        }break;
        /*轴Y+*/
        case TOUCH_TYPE_AXIS_Y_PLUS:
        {
          Audio_Play(RECORD_NORMAL_PUSH);
          sprintf_P(buf, PSTR("G1 F1000 Y%s"), dtostrf(current_position.y+axis_unit, 4, 2, strfloat));
          queue.enqueue_one_now(buf);
        }break;
        /*轴Y-*/
        case TOUCH_TYPE_AXIS_Y_SUB:
        {
          Audio_Play(RECORD_NORMAL_PUSH);
          sprintf_P(buf, PSTR("G1 F1000 Y%s"), dtostrf(current_position.y-axis_unit, 4, 2, strfloat));
          queue.enqueue_one_now(buf);
        }break;
        /*轴Z+*/
        case TOUCH_TYPE_AXIS_Z_PLUS:
        {
          Audio_Play(RECORD_NORMAL_PUSH);
          sprintf_P(buf, PSTR("G1 F500 Z%s"), dtostrf(current_position.z+axis_unit, 4, 2, strfloat));
          queue.enqueue_now_P(buf);
        }break;
        /*轴Z-*/
        case TOUCH_TYPE_AXIS_Z_SUB:
        {
          Audio_Play(RECORD_NORMAL_PUSH);
          sprintf_P(buf, PSTR("G1 F500 Z%s"), dtostrf(current_position.z-axis_unit, 4, 2, strfloat));
          queue.enqueue_now_P(buf);
        }break;
        /*XY轴回原点*/
        case TOUCH_TYPE_AXIS_XY_HOME:
        {
          Audio_Play(RECORD_NORMAL_PUSH);
          sprintf_P(buf, PSTR("G28 X Y"));
          queue.enqueue_one_now(buf);
        }break;
        /*Z轴回原点*/
        case TOUCH_TYPE_AXIS_Z_HOME:
        {
          Audio_Play(RECORD_NORMAL_PUSH);
          sprintf_P(buf, PSTR("G28 Z"));
          queue.enqueue_one_now(buf);
        }break;
        /*进退料喷头选择*/
        case TOUCH_TYPE_INOUT_SELECT:
        {
        #if EXTRUDERS > 1
          headNum = headNum%2 + 1;
          DWIN_IntRefresh(INOUT_SELECT_ICON, (headNum>1) ? 0 : 1, 2);
        #else
          Audio_Play(RECORD_UNNORMAL_PUSH);
        #endif
        }break;
        /*进料页面跳转*/
        case TOUCH_TYPE_INOUT_IN_JUMP:
        {
          Audio_Play(RECORD_NORMAL_PUSH);
          if(headNum == 1) currentPage = UART_LCD_Filament0_InPage;
        #if EXTRUDERS > 1
          if(headNum > 1) currentPage = UART_LCD_Filament1_InPage;
        #endif
          refreshFlag = SET;
        }break;
        /*退料页面跳转*/
        case TOUCH_TYPE_INOUT_OUT_JUMP:
        {
          Audio_Play(RECORD_NORMAL_PUSH);
          if(headNum == 1)  currentPage = UART_LCD_Filament0_OutPage;
        #if EXTRUDERS > 1
          if(headNum > 1) currentPage = UART_LCD_Filament1_OutPage;
        #endif
          refreshFlag = SET;
        }break;
        /*首页面跳转*/
        case TOUCH_TYPE_HOME_PAGE:
        {
          Audio_Play(RECORD_NORMAL_PUSH);
          DWIN_IntRefresh(CONTROL_PAGE_TITLE_VP, (Config_ReadLanguage()+8), 2);
          currentPage = UART_LCD_MainPage;
          refreshFlag = SET;
        }break;
        /*打印文件页面跳转*/
        case TOUCH_TYPE_FILE_PAGE:
        {
          Audio_Play(RECORD_NORMAL_PUSH);
          DWIN_IntRefresh(CONTROL_PAGE_TITLE_VP, (Config_ReadLanguage()+8), 2);
          currentPage = UART_LCD_FilePage;
          refreshFlag = SET;
        }break;
        /*设置页面跳转*/
        case TOUCH_TYPE_SETTING_PAGE:
        {
          Audio_Play(RECORD_NORMAL_PUSH);
          DWIN_IntRefresh(CONTROL_PAGE_TITLE_VP, (Config_ReadLanguage()+8), 2);
          currentPage = UART_LCD_SettingPage;
          refreshFlag = SET;
        }break;
      }
    }
    /*固定刷新变量*/
    if(HMI_IsVp_AttainSetTime(AXIS_X_COORD_VP, 300))
    {
      if(!axes_should_home(1 << X_AXIS))
      {
        DWIN_Text_SetColor(0xE630, WIDGET_TEXT_ENABLE_COLOR); /*X坐标显示*/
        DWIN_FloatRefresh(AXIS_X_COORD_VP, current_position.x);
      }
      else DWIN_Text_SetColor(0xE630, WIDGET_TEXT_DISABLE_COLOR);
      if(!axes_should_home(1 << Y_AXIS))
      {
        DWIN_Text_SetColor(0xE660, WIDGET_TEXT_ENABLE_COLOR); /*Y坐标显示*/
        DWIN_FloatRefresh(AXIS_Y_COORD_VP, current_position.y);
      }
      else DWIN_Text_SetColor(0xE660, WIDGET_TEXT_DISABLE_COLOR);
      if(!axes_should_home(1 << Z_AXIS))
      {
        DWIN_Text_SetColor(0xE690, WIDGET_TEXT_ENABLE_COLOR); /*Z坐标显示*/
        DWIN_FloatRefresh(AXIS_Z_COORD_VP, current_position.z);
      }
      else DWIN_Text_SetColor(0xE690, WIDGET_TEXT_DISABLE_COLOR);
    }
  }

  // 喷头退料界面 =====================================================================
  /*喷头0进料界面处理*/
  void UART_LCD_Filament0_InPage(void)
  {
    static unsigned int temp_Head0SetTemp = 0xFFFF;
    static unsigned char inLen = 10;
    static bool tempAvailable = false;
    char buf[30];
    unsigned int temp_Temper;
    static FlagStatus inFlag = RESET;
    /*仅刷一次变量*/
    if(refreshFlag)
    {
      inLen = 10;
      DWIN_IntRefresh(INOUT_WARNING_TITLE_VP, Config_ReadLanguage(), 2);
      DWIN_IntRefresh(INOUT_LENGTH_TITLE_VP, Config_ReadLanguage(), 2);
      DWIN_IntRefresh(INOUT_LENGTH_VP, inLen, 2);
      DWIN_Variate_SetVp(0xE5D0, HEAD0_CURRENT_TEMP_VP);  /*实际温度指向喷头0*/
      DWIN_Variate_SetVp(0xE600, HEAD0_SET_TEMP_VP);  /*设定温度指向喷头0*/
      DWIN_SwitchPage(FILAMENT_PAGE_ID);
      refreshFlag = RESET;
    }
    /*触控刷新变量*/
    if(DWIN_receivedType.Type != TOUCH_TYPE_NO_TOUCH)
    {
      switch(DWIN_receivedType.Type)
      {
        /*喷头温度设定*/
        case TOUCH_TYPE_HEAD_TEMP_SET:
        {
          Audio_Play(RECORD_NORMAL_PUSH);
          temp_Temper = _MIN(DWIN_receivedType.Value, HEATER_0_MAXTEMP);
          thermalManager.setTargetHotend(temp_Temper, 0);
        }break;
        /*进退料长度设定*/
        case TOUCH_TYPE_INOUT_LENGTH:
        {
          Audio_Play(RECORD_NORMAL_PUSH);
          inLen = DWIN_receivedType.Value;
          DWIN_IntRefresh(INOUT_LENGTH_VP, inLen, 2);
        }break;
        /*进退料确认按键*/
        case TOUCH_TYPE_INOUT_CONFIRM:
        {
          if(tempAvailable && inFlag==RESET)
          {
            Audio_Play(RECORD_NORMAL_PUSH);
            memset(buf, 0, 30);
            char strfloat[16];
            sprintf_P(buf, PSTR("G1 F120 E%s"), dtostrf(current_position.e+(float)inLen, 4, 2, strfloat));
            queue.enqueue_one_now(buf);
            DWIN_IntRefresh(INOUT_CONFIRM_TITLE_VP, (Config_ReadLanguage()+8), 2);
            inFlag = SET;
          }
        }break;
        /*进退料取消按键*/
        case TOUCH_TYPE_INOUT_CANCEL:
        {
          Audio_Play(RECORD_NORMAL_PUSH);
          currentPage = UART_LCD_ControlPage;
          refreshFlag = SET;
          //喷嘴目标温度设置为0
          thermalManager.setTargetHotend(0, 0);
  #if HEATER_HEAD_1_USED
          thermalManager.setTargetHotend(0, 1);
  #endif
  #if HEATER_HEAD_2_USED
          thermalManager.setTargetHotend(0, Heater_Head2);
  #endif
        }break;
      }
      return;
    }
    /*固定刷新变量*/
    if(HMI_IsVp_AttainSetTime(HEAD0_CURRENT_TEMP_VP, 500))
    {
      DWIN_IntRefresh(HEAD0_CURRENT_TEMP_VP, (unsigned int)thermalManager.degHotend(0), 2);
      if(labs((int)thermalManager.degHotend(0)-(int)thermalManager.degTargetHotend(0)) > TEMP_HYSTERESIS)
      {
        DWIN_IntRefresh(HEAD_ANNULAR_ICON_VP, constrain((unsigned int)thermalManager.degHotend(0)*50/HEATER_0_MAXTEMP,1,50)-1, 2);
        tempAvailable = false;
        DWIN_IntRefresh(INOUT_CONFIRM_TITLE_VP, (Config_ReadLanguage()+8), 2);
      }
      else
      {
        DWIN_IntRefresh(HEAD_ANNULAR_ICON_VP, constrain((unsigned int)thermalManager.degHotend(0)*50/HEATER_0_MAXTEMP,2,50)+48, 2);
        if((unsigned int)thermalManager.degHotend(0) >= EXTRUDE_MINTEMP)
        {
          tempAvailable = true;
          if(inFlag == RESET) DWIN_IntRefresh(INOUT_CONFIRM_TITLE_VP, Config_ReadLanguage(), 2);
        }
      }
    }
    if(temp_Head0SetTemp != (unsigned int)thermalManager.degTargetHotend(0))
    {
      temp_Head0SetTemp = (unsigned int)thermalManager.degTargetHotend(0);
      DWIN_IntRefresh(HEAD0_SET_TEMP_VP, temp_Head0SetTemp, 2);
    }
    if(inFlag==SET && !planner.has_blocks_queued())
    {
      inFlag = RESET;
      Audio_Play(RECORD_ACTION_FINISH);
      DWIN_IntRefresh(INOUT_CONFIRM_TITLE_VP, Config_ReadLanguage(), 2);
    }
  }

  /*喷头0退料界面处理*/
  void UART_LCD_Filament0_OutPage(void)
  {
    static unsigned int temp_Head0SetTemp = 0xFFFF;
    static unsigned char outLen = 10;
    static bool tempAvailable = false;
    char buf[30];
    unsigned int temp_Temper;
    static FlagStatus outFlag = RESET;
    /*仅刷一次变量*/
    if(refreshFlag)
    {
      outLen = 10;
      DWIN_IntRefresh(INOUT_WARNING_TITLE_VP, (Config_ReadLanguage()+8), 2);
      DWIN_IntRefresh(INOUT_LENGTH_TITLE_VP, (Config_ReadLanguage()+8), 2);
      DWIN_IntRefresh(INOUT_LENGTH_VP, outLen, 2);
      DWIN_Variate_SetVp(0xE5D0, HEAD0_CURRENT_TEMP_VP);  /*实际温度指向喷头0*/
      DWIN_Variate_SetVp(0xE600, HEAD0_SET_TEMP_VP);  /*设定温度指向喷头0*/
      DWIN_SwitchPage(FILAMENT_PAGE_ID);
      refreshFlag = RESET;
    }
    /*触控刷新变量*/
    if(DWIN_receivedType.Type != TOUCH_TYPE_NO_TOUCH)
    {
      switch(DWIN_receivedType.Type)
      {
        /*喷头温度设定*/
        case TOUCH_TYPE_HEAD_TEMP_SET:
        {
          Audio_Play(RECORD_NORMAL_PUSH);
          temp_Temper = _MIN(DWIN_receivedType.Value, HEATER_0_MAXTEMP);
          thermalManager.setTargetHotend((float)temp_Temper, 0);
        }break;
        /*进退料长度设定*/
        case TOUCH_TYPE_INOUT_LENGTH:
        {
          Audio_Play(RECORD_NORMAL_PUSH);
          outLen = DWIN_receivedType.Value;
          DWIN_IntRefresh(INOUT_LENGTH_VP, outLen, 2);
        }break;
        /*进退料确认按键*/
        case TOUCH_TYPE_INOUT_CONFIRM:
        {
          if(tempAvailable && outFlag==RESET)
          {
            Audio_Play(RECORD_NORMAL_PUSH);
            memset(buf, 0, 30);
            char strfloat[16];
            sprintf_P(buf, PSTR("G1 F120 E%s"), dtostrf(current_position.e-(float)outLen, 4, 2, strfloat));
            queue.enqueue_one_now(buf);
            DWIN_IntRefresh(INOUT_CONFIRM_TITLE_VP, (Config_ReadLanguage()+8), 2);
            outFlag = SET;
          }
        }break;
        /*进退料取消按键*/
        case TOUCH_TYPE_INOUT_CANCEL:
        {
          Audio_Play(RECORD_NORMAL_PUSH);
          currentPage = UART_LCD_ControlPage;
          refreshFlag = SET;
        }break;
      }
      return;
    }
    /*固定刷新变量*/
    if(HMI_IsVp_AttainSetTime(HEAD0_CURRENT_TEMP_VP, 500))
    {
      DWIN_IntRefresh(HEAD0_CURRENT_TEMP_VP, (unsigned int)thermalManager.degHotend(0), 2);
      if(labs((int)thermalManager.degHotend(0)-(int)thermalManager.degTargetHotend(0)) > TEMP_HYSTERESIS)
      {
        DWIN_IntRefresh(HEAD_ANNULAR_ICON_VP, constrain((unsigned int)thermalManager.degHotend(0)*50/HEATER_0_MAXTEMP,1,50)-1, 2);
        tempAvailable = false;
        DWIN_IntRefresh(INOUT_CONFIRM_TITLE_VP, (Config_ReadLanguage()+8), 2);
      }
      else
      {
        DWIN_IntRefresh(HEAD_ANNULAR_ICON_VP, constrain((unsigned int)thermalManager.degHotend(0)*50/HEATER_0_MAXTEMP,2,50)+48, 2);
        if((unsigned int)thermalManager.degHotend(0) >= EXTRUDE_MINTEMP)
        {
          tempAvailable = true;
          if(outFlag == RESET) DWIN_IntRefresh(INOUT_CONFIRM_TITLE_VP, Config_ReadLanguage(), 2);
        }
      }
    }
    if(temp_Head0SetTemp != (unsigned int)thermalManager.degTargetHotend(0))
    {
      temp_Head0SetTemp = (unsigned int)thermalManager.degTargetHotend(0);
      DWIN_IntRefresh(HEAD0_SET_TEMP_VP, temp_Head0SetTemp, 2);
    }
    if(outFlag==SET && !planner.has_blocks_queued())
    {
      outFlag = RESET;
      Audio_Play(RECORD_ACTION_FINISH);
      DWIN_IntRefresh(INOUT_CONFIRM_TITLE_VP, Config_ReadLanguage(), 2);
    }
  }

  #if EXTRUDERS > 1
  /*喷头1进料界面处理*/
  void UART_LCD_Filament1_InPage(void)
  {
    static unsigned int temp_Head1SetTemp = 0xFFFF;
    static unsigned char inLen = 10;
    static bool tempAvailable = false;
    char buf[30];
    unsigned int temp_Temper;
    static FlagStatus inFlag = RESET;
    /*仅刷一次变量*/
    if(refreshFlag)
    {
      inLen = 10;
      DWIN_IntRefresh(INOUT_WARNING_TITLE_VP, Config_ReadLanguage(), 2);
      DWIN_IntRefresh(INOUT_LENGTH_TITLE_VP, Config_ReadLanguage(), 2);
      DWIN_IntRefresh(INOUT_LENGTH_VP, inLen, 2);
      DWIN_Variate_SetVp(0xE5D0, HEAD1_CURRENT_TEMP_VP);  /*实际温度指向喷头1*/
      DWIN_Variate_SetVp(0xE600, HEAD1_SET_TEMP_VP);  /*设定温度指向喷头1*/
      DWIN_SwitchPage(FILAMENT_PAGE_ID);
      refreshFlag = RESET;
    }
    /*触控刷新变量*/
    if(DWIN_receivedType.Type != TOUCH_TYPE_NO_TOUCH)
    {
      switch(DWIN_receivedType.Type)
      {
        /*喷头温度设定*/
        case TOUCH_TYPE_HEAD_TEMP_SET:
        {
          Audio_Play(RECORD_NORMAL_PUSH);
          temp_Temper = _MIN(DWIN_receivedType.Value, HEATER_0_MAXTEMP);
          thermalManager.setTargetHotend((float)temp_Temper, 1);
        }break;
        /*进退料长度设定*/
        case TOUCH_TYPE_INOUT_LENGTH:
        {
          Audio_Play(RECORD_NORMAL_PUSH);
          inLen = DWIN_receivedType.Value;
        }break;
        /*进退料确认按键*/
        case TOUCH_TYPE_INOUT_CONFIRM:
        {
          if(tempAvailable && inFlag==RESET)
          {
            Audio_Play(RECORD_NORMAL_PUSH);
            memset(buf, 0, 30);
            sprintf_P(buf, PSTR("G1 F120 E%f"), current_position[E1_AXIS]+(float)inLen );
            queue.enqueue_one_now(buf);
            DWIN_IntRefresh(INOUT_CONFIRM_TITLE_VP, (Config_ReadLanguage()+8), 2);
            inFlag = SET;
          }
        }break;
        /*进退料取消按键*/
        case TOUCH_TYPE_INOUT_CANCEL:
        {
          Audio_Play(RECORD_NORMAL_PUSH);
          currentPage = UART_LCD_ControlPage;
          refreshFlag = SET;
        }break;
      }
      return;
    }
    /*固定刷新变量*/
    if(HMI_IsVp_AttainSetTime(HEAD1_CURRENT_TEMP_VP, 500))
    {
      DWIN_IntRefresh(HEAD1_CURRENT_TEMP_VP, (unsigned int)thermalManager.degHotend(Temper_Head1), 2);
      if(labs((int)thermalManager.degHotend(Temper_Head1)-(int)thermalManager.degTargetHotend(1)) > TEMP_HYSTERESIS)
      {
        DWIN_IntRefresh(HEAD_ANNULAR_ICON_VP, constrain((unsigned int)thermalManager.degHotend(Temper_Head1)*50/HEATER_0_MAXTEMP,1,50)-1, 2);
        tempAvailable = false;
        DWIN_IntRefresh(INOUT_CONFIRM_TITLE_VP, (Config_ReadLanguage()+8), 2);
      }
      else
      {
        DWIN_IntRefresh(HEAD_ANNULAR_ICON_VP, constrain((unsigned int)thermalManager.degHotend(Temper_Head1)*50/HEATER_0_MAXTEMP,2,50)+48, 2);
        if((unsigned int)thermalManager.degHotend(Temper_Head1) >= EXTRUDE_MINTEMP)
        {
          tempAvailable = true;
          if(inFlag == RESET) DWIN_IntRefresh(INOUT_CONFIRM_TITLE_VP, Config_ReadLanguage(), 2);
        }
      }
    }
    if(temp_Head1SetTemp != (unsigned int)thermalManager.degTargetHotend(1))
    {
      temp_Head1SetTemp = (unsigned int)thermalManager.degTargetHotend(1);
      DWIN_IntRefresh(HEAD1SET_TEMP_VP, temp_Head1SetTemp, 2);
    }
    if(inFlag==SET && !has_blocks_queued())
    {
      inFlag = RESET;
      Audio_Play(RECORD_ACTION_FINISH);
      DWIN_IntRefresh(INOUT_CONFIRM_TITLE_VP, Config_ReadLanguage(), 2);
    }
  }

  /*喷头1退料界面处理*/
  void UART_LCD_Filament1_OutPage(void)
  {
    static unsigned int temp_Head1SetTemp = 0xFFFF;
    static unsigned char outLen = 10;
    static bool tempAvailable = false;
    char buf[30];
    unsigned int temp_Temper;
    static FlagStatus outFlag = RESET;
    /*仅刷一次变量*/
    if(refreshFlag)
    {
      outLen = 10;
      DWIN_IntRefresh(INOUT_WARNING_TITLE_VP, (Config_ReadLanguage()+8), 2);
      DWIN_IntRefresh(INOUT_LENGTH_TITLE_VP, (Config_ReadLanguage()+8), 2);
      DWIN_IntRefresh(INOUT_LENGTH_VP, outLen, 2);
      DWIN_Variate_SetVp(0xE5D0, HEAD1_CURRENT_TEMP_VP);  /*实际温度指向喷头1*/
      DWIN_Variate_SetVp(0xE600, HEAD1SET_TEMP_VP);  /*设定温度指向喷头1*/
      DWIN_SwitchPage(FILAMENT_PAGE_ID);
      refreshFlag = RESET;
    }
    /*触控刷新变量*/
    if(DWIN_receivedType.Type != TOUCH_TYPE_NO_TOUCH)
    {
      switch(DWIN_receivedType.Type)
      {
        /*喷头温度设定*/
        case TOUCH_TYPE_HEAD_TEMP_SET:
        {
          Audio_Play(RECORD_NORMAL_PUSH);
          temp_Temper = _MIN(DWIN_receivedType.Value, HEATER_0_MAXTEMP);
          thermalManager.setTargetHotend((float)temp_Temper, 1);
        }break;
        /*进退料长度设定*/
        case TOUCH_TYPE_INOUT_LENGTH:
        {
          Audio_Play(RECORD_NORMAL_PUSH);
          outLen = DWIN_receivedType.Value;
        }break;
        /*进退料确认按键*/
        case TOUCH_TYPE_INOUT_CONFIRM:
        {
          if(tempAvailable && outFlag==RESET)
          {
            Audio_Play(RECORD_NORMAL_PUSH);
            memset(buf, 0, 30);
            sprintf_P(buf, PSTR("G1 F120 E%f"), current_position[E1_AXIS]-(float)outLen);
            queue.enqueue_one_now(buf);
            DWIN_IntRefresh(INOUT_CONFIRM_TITLE_VP, (Config_ReadLanguage()+8), 2);
            outFlag = SET;
          }
        }break;
        /*进退料取消按键*/
        case TOUCH_TYPE_INOUT_CANCEL:
        {
          Audio_Play(RECORD_NORMAL_PUSH);
          currentPage = UART_LCD_ControlPage;
          refreshFlag = SET;
        }break;
      }
      return;
    }
    /*固定刷新变量*/
    if(HMI_IsVp_AttainSetTime(HEAD1_CURRENT_TEMP_VP, 500))
    {
      DWIN_IntRefresh(HEAD1_CURRENT_TEMP_VP, (unsigned int)thermalManager.degHotend(Temper_Head1), 2);
      if(labs((int)thermalManager.degHotend(Temper_Head1)-(int)thermalManager.degTargetHotend(1)) > TEMP_HYSTERESIS)
      {
        DWIN_IntRefresh(HEAD_ANNULAR_ICON_VP, constrain((unsigned int)thermalManager.degHotend(Temper_Head1)*50/HEATER_0_MAXTEMP,1,50)-1, 2);
        tempAvailable = false;
        DWIN_IntRefresh(INOUT_CONFIRM_TITLE_VP, (Config_ReadLanguage()+8), 2);
      }
      else
      {
        DWIN_IntRefresh(HEAD_ANNULAR_ICON_VP, constrain((unsigned int)thermalManager.degHotend(Temper_Head1)*50/HEATER_0_MAXTEMP,2,50)+48, 2);
        if((unsigned int)thermalManager.degHotend(Temper_Head1) >= EXTRUDE_MINTEMP)
        {
          tempAvailable = true;
          if(outFlag == RESET) DWIN_IntRefresh(INOUT_CONFIRM_TITLE_VP, Config_ReadLanguage(), 2);
        }
      }
    }
    if(temp_Head1SetTemp != (unsigned int)thermalManager.degTargetHotend(1))
    {
      temp_Head1SetTemp = (unsigned int)thermalManager.degTargetHotend(1);
      DWIN_IntRefresh(HEAD1_SET_TEMP_VP, temp_Head1SetTemp, 2);
    }
    if(outFlag==SET && !has_blocks_queued())
    {
      outFlag = RESET;
      Audio_Play(RECORD_ACTION_FINISH);
      DWIN_IntRefresh(INOUT_CONFIRM_TITLE_VP, Config_ReadLanguage(), 2);
    }
  }
  #endif

  // 设置界面 =====================================================================
  /*设置界面处理*/
  void UART_LCD_SettingPage(void)
  {
    static FlagStatus settingChange_Flag = RESET;
    float axis_offset;
    /*仅刷一次变量*/
    if(refreshFlag)
    {
      DWIN_IntRefresh(SETTING_PAGE_TITLE_VP, Config_ReadLanguage(), 2);
      DWIN_SwitchPage(SETTING_PAGE_ID);
      refreshFlag = RESET;
    }
    /*触控刷新变量*/
    if(DWIN_receivedType.Type != TOUCH_TYPE_NO_TOUCH)
    {
      switch(DWIN_receivedType.Type)
      {
        /*机箱风扇设置*/
        case TOUCH_TYPE_BOX_FAN_SET:
        {
          Audio_Play(RECORD_NORMAL_PUSH);
          switch(DWIN_receivedType.Value)
          {
            // case 0x0001: Fan_SetStatus(Fan_Box, FAN_AUTO); break;
            // case 0x0002: Fan_SetStatus(Fan_Box, FAN_OFF); thermalManager.set_fan_speed(0, thermalManager.fan_speed[0] ? 0 : 255); break;
            // case 0x0004: Fan_SetStatus(Fan_Box, FAN_ON); break;
          }
        }break;
        /*机箱照明设置*/
        case TOUCH_TYPE_BOX_LED_SET:
        {
          Audio_Play(RECORD_NORMAL_PUSH);
          switch(DWIN_receivedType.Value)
          {
            // case 0x0001: Light_Control(DISABLE); break;
            // case 0x0002: Light_Control(ENABLE); break;
          }
        }break;
        /*显示亮度设置*/
        case TOUCH_TYPE_LCD_LUMINANCE:
        {
          Audio_Play(RECORD_NORMAL_PUSH);
          DWIN_IntRefresh(LCD_LUMINANCE_ICON_VP, (0x03E0>>DWIN_receivedType.Value)&0x001F, 2);
          Config_WriteLuminance(10+18*DWIN_receivedType.Value);
          DWIN_Backlight_SetLuminance(Config_ReadLuminance());
          settingChange_Flag = SET;
        }break;
        /*系统音量设置*/
        case TOUCH_TYPE_SYSTEM_VOLUME:
        {
          Config_WriteVolume(3*DWIN_receivedType.Value);
          Audio_SetVolume(Config_ReadVolume());
          DWIN_IntRefresh(SYSTEM_VOLUME_ICON_VP, (0x03E0>>DWIN_receivedType.Value)&0x001F, 2);
          Audio_Play(RECORD_NORMAL_PUSH);
          settingChange_Flag = SET;
        }break;
        /*Z轴补偿+*/
        case TOUCH_TYPE_Z_MAKE_UP_PLUS:
        {
          constexpr float ZOFFSET = 0.100001f;
          axis_offset = Config_ReadZAxisOff() + ZOFFSET;
          if(axis_offset > (Z_PROBE_OFFSET_RANGE_MAX+0.005)) Audio_Play(RECORD_UNNORMAL_PUSH);
          else
          {
            Audio_Play(RECORD_NORMAL_PUSH);
            // probe.offset.z = axis_offset;
            babystep.add_mm(Z_AXIS, ZOFFSET);
            Config_WriteZAxisOff(axis_offset);
            DWIN_FloatRefresh(Z_MAKE_UP_VP, axis_offset);
            settingChange_Flag = SET;
          }
        }break;
        /*Z轴补偿-*/
        case TOUCH_TYPE_Z_MAKE_UP_SUB:
        {
          constexpr float ZOFFSET = -0.100001f;
          axis_offset = Config_ReadZAxisOff() + ZOFFSET;
          if(axis_offset < Z_PROBE_OFFSET_RANGE_MIN-0.005) Audio_Play(RECORD_UNNORMAL_PUSH);
          else
          {
            if(axis_offset < Z_PROBE_OFFSET_RANGE_MIN) axis_offset = Z_PROBE_OFFSET_RANGE_MIN;
            Audio_Play(RECORD_NORMAL_PUSH);
            // probe.offset.z = axis_offset;
            babystep.add_mm(Z_AXIS, ZOFFSET);
            Config_WriteZAxisOff(axis_offset);
            DWIN_FloatRefresh(Z_MAKE_UP_VP, axis_offset);
            settingChange_Flag = SET;
          }
        }break;
        /*打印速率设置*/
        case TOUCH_TYPE_SPEED_RATE:
        {
          Audio_Play(RECORD_NORMAL_PUSH);
          feedrate_percentage = constrain(DWIN_receivedType.Value, 10, 300);
          DWIN_IntRefresh(PRINT_SPEED_RATE_VP, feedrate_percentage, 2);
        }break;
        /*喷头数量设置*/
        case TOUCH_TYPE_HEAD_NUM:
        {
          Audio_Play(RECORD_NORMAL_PUSH);
          switch(DWIN_receivedType.Value)
          {
            case 0x0001: Config_WriteHeadNum(2); break;
            case 0x0002: Config_WriteHeadNum(1); break;
          }
          settingChange_Flag = SET;
        }break;
        /*系统语言页面跳转*/
        case TOUCH_TYPE_LANGUAGE_JUMP:
        {
          Audio_Play(RECORD_NORMAL_PUSH);
          currentPage = UART_LCD_LanguagePage;
          refreshFlag = SET;
          settingChange_Flag = SET;
        }break;
        /*关于本机页面跳转*/
        case TOUCH_TYPE_ABOUT_JUMP:
        {
          Audio_Play(RECORD_NORMAL_PUSH);
          currentPage = UART_LCD_AboutPage;
          refreshFlag = SET;
        }break;
        /*首页面跳转*/
        case TOUCH_TYPE_HOME_PAGE:
        {
          Audio_Play(RECORD_NORMAL_PUSH);
          if(settingChange_Flag)
          {
            settingChange_Flag = RESET;
            // Config_SaveSettings();
          }
          DWIN_IntRefresh(SETTING_PAGE_TITLE_VP, (Config_ReadLanguage()+8), 2);
          currentPage = UART_LCD_MainPage;
          refreshFlag = SET;
        }break;
        /*机器控制页面跳转*/
        case TOUCH_TYPE_CONTROL_PAGE:
        {
          if(!StorageDevice_IsStartPrint())
          {
            Audio_Play(RECORD_NORMAL_PUSH);
            DWIN_IntRefresh(SETTING_PAGE_TITLE_VP, (Config_ReadLanguage()+8), 2);
            currentPage = UART_LCD_ControlPage;
            refreshFlag = SET;
          }
          else Audio_Play(RECORD_UNNORMAL_PUSH);
        }break;
        /*打印文件页面跳转*/
        case TOUCH_TYPE_FILE_PAGE:
        {
          Audio_Play(RECORD_NORMAL_PUSH);
          DWIN_IntRefresh(SETTING_PAGE_TITLE_VP, (Config_ReadLanguage()+8), 2);
          currentPage = UART_LCD_FilePage;
          refreshFlag = SET;
        }break;
      }
    }
  }

  // 关于本机界面 =====================================================================
  /*关于本机界面处理*/
  void UART_LCD_AboutPage(void)
  {
    /*仅刷一次变量*/
    if(refreshFlag)
    {
      DWIN_SwitchPage(ABOUT_PAGE_ID);
      refreshFlag = RESET;
    }
    /*触控刷新变量*/
    if(DWIN_receivedType.Type != TOUCH_TYPE_NO_TOUCH)
    {
      switch(DWIN_receivedType.Type)
      {
        /*关于本机返回*/
        case TOUCH_TYPE_ABOUT_BACK:
        {
          Audio_Play(RECORD_NORMAL_PUSH);
          currentPage = UART_LCD_SettingPage;
          refreshFlag = SET;
        }break;
      }
    }
  }

  // 语言界面 =====================================================================
  /*语言界面处理*/
  void UART_LCD_LanguagePage(void)
  {
    static unsigned char languageCount = 0;
    /*仅刷一次变量*/
    if(refreshFlag)
    {
    	DWIN_SwitchPage(LANGUAGE_PAGE_ID);
    	refreshFlag = false;
    }
    /*触控刷新变量*/
    if(DWIN_receivedType.Type != TOUCH_TYPE_NO_TOUCH)
    {
    	switch(DWIN_receivedType.Type)
    	{
    		/*系统语言选择*/
    		case TOUCH_TYPE_LANGUAGE_SELECT:
    		{
    			Audio_Play(RECORD_NORMAL_PUSH);
    			languageCount = 0;
    			while(DWIN_receivedType.Value>>languageCount != 0x0001) languageCount++;
    			DWIN_IntRefresh(LANGUAGE_CONFIRM_TITLE_VP, languageCount, 2);
    		}break;
    		/*系统语言确认*/
    		case TOUCH_TYPE_LANGUAGE_CONFIRM:
    		{
    			Audio_Play(RECORD_NORMAL_PUSH);
    			Config_WriteLanguage(languageCount);
    			UI_LanguageChange(Config_ReadLanguage());
    			currentPage = UART_LCD_SettingPage;
    			refreshFlag = true;
    		}break;
    	}
    }
  }

  /*UI语言切换处理*/
  void UI_LanguageChange(unsigned char language)
  {
    unsigned char temp;
    /*底部标签*/
    DWIN_IntRefresh(HOME_PAGE_TITLE_VP, (language+8), 2);
    if(StorageDevice_IsStartPrint()) DWIN_IntRefresh(CONTROL_PAGE_TITLE_VP, (language+16), 2);
    else DWIN_IntRefresh(CONTROL_PAGE_TITLE_VP, (language+8), 2);
    DWIN_IntRefresh(FILE_PAGE_TITLE_VP, (language+8), 2);
    DWIN_IntRefresh(SETTING_PAGE_TITLE_VP, (language+8), 2);
    /*首页文本*/
    DWIN_IntRefresh(HEAD0_TITLE_VP, language, 2);
    if(Config_ReadHeadNum() > 1) DWIN_IntRefresh(HEAD1_TITLE_VP, language, 2);
    else DWIN_IntRefresh(HEAD1_TITLE_VP, (language+8), 2);
    DWIN_IntRefresh(BED_TITLE_VP, language, 2);
    DWIN_IntRefresh(PRINT_FILE_TITLE_VP, language, 2);
    DWIN_IntRefresh(PRINT_TIME_TITLE_VP, language, 2);
    DWIN_IntRefresh(PRINT_PROCESS_TITLE_VP, language, 2);
    DWIN_IntRefresh(PRINT_REMAIN_TITLE_VP, language, 2);
    if(StorageDevice_IsStartPrint())
    {
    	DWIN_IntRefresh(STOP_TITLE_VP, language, 2);
    	if(card.isPrinting()) DWIN_IntRefresh(PAUSE_RESUME_TITLE_VP, (language+8), 2);
    	else DWIN_IntRefresh(PAUSE_RESUME_TITLE_VP, language, 2);
    }
    else
    {
    	DWIN_IntRefresh(STOP_TITLE_VP, (language+8), 2);
    	DWIN_IntRefresh(PAUSE_RESUME_TITLE_VP, (language+16), 2);
    }
    /*喷头页文本*/
    DWIN_IntRefresh(HEAD_TEMP_TITLE_VP, language, 2);
    DWIN_IntRefresh(HEAD_FAN_TITLE_VP, language, 2);
    DWIN_IntRefresh(HEAD_PAGE_BACK_VP, language, 2);
    /*控制页文本*/
    DWIN_IntRefresh(AXIS_MOVE_TITLE_VP, language, 2);
    DWIN_IntRefresh(AXIS_UNIT_TITLE_VP, language, 2);
    DWIN_IntRefresh(MOTOR_FREE_TITLE_VP, language, 2);
    DWIN_IntRefresh(INOUT_TITLE_VP, language, 2);
    DWIN_IntRefresh(INOUT_SELECT_TITLE_VP, language, 2);
    DWIN_IntRefresh(INOUT_IN_TITLE_VP, language, 2);
    DWIN_IntRefresh(INOUT_OUT_TITLE_VP, language, 2);
    /*文件页文本*/
    temp = get_gcode_num();
    if(temp)
    {
    	File_CurrentPage = 1;
    	File_TotalPage = (temp+6) / 7;
    	DWIN_IntRefresh(FILE_TOTAL_PAGE_VP, File_TotalPage, 2);
    	MCU_FlipOver_FileList(1, 7);
    	UI_FlipOver_FileList(1);
    }
    else
    {
    	File_CurrentPage = 0;
    	File_TotalPage = 0;
    	DWIN_IntRefresh(FILE_TOTAL_PAGE_VP, File_TotalPage, 2);
    	MCU_FlipOver_FileList(0, 7);
    	UI_FlipOver_FileList(0);
    }
    /*设置页文本*/
    DWIN_IntRefresh(BOX_FAN_TITLE_VP, language, 2);
    DWIN_IntRefresh(BOX_LED_TITLE_VP, language, 2);
    DWIN_IntRefresh(LCD_LUMINANCE_TITLE_VP, language, 2);
    DWIN_IntRefresh(SYSTEM_VOLUME_TITLE_VP, language, 2);
    DWIN_IntRefresh(Z_MAKE_UP_TITLE_VP, language, 2);
    DWIN_IntRefresh(PRINT_SPEED_RATE_TITLE_VP, language, 2);
    DWIN_IntRefresh(HEAD_NUM_TITLE_VP, language, 2);
    DWIN_IntRefresh(SYSTEM_LANGUAGE_TITLE_VP, language, 2);
    DWIN_IntRefresh(ABOUT_TITLE_VP, language, 2);
    DWIN_IntRefresh(SYSTEM_LANGUAGE_TEXT_VP, language, 2);
    /*进退料页文本*/
    DWIN_IntRefresh(INOUT_CANCEL_TITLE_VP, language, 2);
    /*关于页文本*/
    DWIN_IntRefresh(ABOUT1_TITLE_VP, language, 2);
    DWIN_IntRefresh(ABOUT2_TITLE_VP, language, 2);
    DWIN_IntRefresh(ABOUT3_TITLE_VP, language, 2);
    DWIN_IntRefresh(ABOUT4_TITLE_VP, language, 2);
    DWIN_IntRefresh(ABOUT_BACK_TITLE_VP, language, 2);
  }

  // 文件界面 =====================================================================

  /*模型文件界面处理*/
  void UART_LCD_FilePage(void)
  {
    static unsigned char file_SelectNum = 0xFF;
    /*仅刷一次变量*/
    if(refreshFlag)
    {
      file_SelectNum = 0xFF;
      DWIN_IntRefresh(FILE_PAGE_TITLE_VP, Config_ReadLanguage(), 2);
      DWIN_IntRefresh(FILE_SELECT_ICON_VP, 0, 2);
      DWIN_IntRefresh(FILE_PRINT_TITLE_VP, (Config_ReadLanguage()+8), 2);
      DWIN_SwitchPage(FILE_PAGE_ID);
      refreshFlag = false;
    }
    /*触控刷新变量*/
    if(DWIN_receivedType.Type != TOUCH_TYPE_NO_TOUCH)
    {
      switch(DWIN_receivedType.Type)
      {
        /*模型文件选择*/
        case TOUCH_TYPE_FILE_SELECT:
        {
          if(DWIN_receivedType.Value<=6 && strlen(printFile_Info[DWIN_receivedType.Value].filename) && !StorageDevice_IsStartPrint())
          {
            Audio_Play(RECORD_NORMAL_PUSH);
            file_SelectNum = DWIN_receivedType.Value;
            DWIN_IntRefresh(FILE_SELECT_ICON_VP, 0x0001<<file_SelectNum, 2);
            DWIN_IntRefresh(FILE_PRINT_TITLE_VP, Config_ReadLanguage(), 2);
          }
        }break;
        /*模型文件上页*/
        case TOUCH_TYPE_FILE_FLIP_PRE:
        {
          if(File_CurrentPage > 1)
          {
            Audio_Play(RECORD_NORMAL_PUSH);
            file_SelectNum = 0xFF;
            File_CurrentPage--;
            MCU_FlipOver_FileList(File_CurrentPage, 7);
            UI_FlipOver_FileList(File_CurrentPage);
          }
        }break;
        /*模型文件下页*/
        case TOUCH_TYPE_FILE_FLIP_NEXT:
        {
          if(File_CurrentPage < File_TotalPage)
          {
            Audio_Play(RECORD_NORMAL_PUSH);
            file_SelectNum = 0xFF;
            File_CurrentPage++;
            MCU_FlipOver_FileList(File_CurrentPage, 7);
            UI_FlipOver_FileList(File_CurrentPage);
          }
        }break;
        /*模型文件打印*/
        case TOUCH_TYPE_FILE_PRINT:
        {
          if(file_SelectNum<=6 && strlen(printFile_Info[file_SelectNum].filename) && !StorageDevice_IsStartPrint())
          {
            Audio_Play(RECORD_PRINT_START);
            MCU_StartPrint(printFile_Info[file_SelectNum].filename);
            UI_StartPrint(printFile_Info[file_SelectNum].filename);
            DWIN_IntRefresh(FILE_PAGE_TITLE_VP, (Config_ReadLanguage()+8), 2);
            currentPage = UART_LCD_MainPage;
            refreshFlag = SET;
          }
        }break;
        /*首页面跳转*/
        case TOUCH_TYPE_HOME_PAGE:
        {
          Audio_Play(RECORD_NORMAL_PUSH);
          DWIN_IntRefresh(FILE_PAGE_TITLE_VP, (Config_ReadLanguage()+8), 2);
          currentPage = UART_LCD_MainPage;
          refreshFlag = SET;
        }break;
        /*机器控制页面跳转*/
        case TOUCH_TYPE_CONTROL_PAGE:
        {
          if(!StorageDevice_IsStartPrint())
          {
            Audio_Play(RECORD_NORMAL_PUSH);
            DWIN_IntRefresh(FILE_PAGE_TITLE_VP, (Config_ReadLanguage()+8), 2);
            currentPage = UART_LCD_ControlPage;
            refreshFlag = SET;
          }
          else Audio_Play(RECORD_UNNORMAL_PUSH);
        }break;
        /*设置页面跳转*/
        case TOUCH_TYPE_SETTING_PAGE:
        {
          Audio_Play(RECORD_NORMAL_PUSH);
          DWIN_IntRefresh(FILE_PAGE_TITLE_VP, (Config_ReadLanguage()+8), 2);
          currentPage = UART_LCD_SettingPage;
          refreshFlag = SET;
        }break;
      }
    }
  }

  /*MCU模型文件列表翻页处理 page:当前页号(为0时直接清空所有内容) fNum_PerPage:每页最多文件数*/
  void MCU_FlipOver_FileList(unsigned char page, unsigned char fNum_PerPage)
  {
    unsigned char fNum, count;
    memset(printFile_Info, 0, sizeof(PrintFile_InfoTypeDef)*7);
    if(page)
    {
      fNum = StorageDevice_GetFilesInfo(fNum_PerPage, (page-1)*fNum_PerPage+1, printFile_Info);
      if(fNum)
      {
        for(count=0; count<fNum_PerPage; count++)
        {
          DWIN_Text_Clear(FILE1_TEXT_VP+50*count, 50);
          if(count < fNum) /*文件存在->刷新并显示相关变量*/
          {
            DWIN_StringRefresh(FILE1_TEXT_VP+50*count, printFile_Info[count].filename);
            DWIN_IntRefresh(FILE1_MONTH_VP+4*count, printFile_Info[count].month+100, 2);
            DWIN_Show(FILE1_MONTH_VP+4*count, 0xE810+0x30*count);
            DWIN_IntRefresh(FILE1_DAY_VP+4*count, printFile_Info[count].day+100, 2);
            DWIN_Show(FILE1_DAY_VP+4*count, 0xE960+0x30*count);
            DWIN_IntRefresh(FILE1_HOUR_VP+4*count, printFile_Info[count].hour+100, 2);
            DWIN_Show(FILE1_HOUR_VP+4*count, 0xEAB0+0x30*count);
            DWIN_IntRefresh(FILE1_MIN_VP+4*count, printFile_Info[count].min+100, 2);
            DWIN_Show(FILE1_MIN_VP+4*count, 0xEC00+0x30*count);
          }
          else /*文件不存在->隐藏相关变量*/
          {
            DWIN_Hide(0xE810+0x30*count);
            DWIN_Hide(0xE960+0x30*count);
            DWIN_Hide(0xEAB0+0x30*count);
            DWIN_Hide(0xEC00+0x30*count);
          }
        }
      }
    }
    else
    {
      for(count=0; count<fNum_PerPage; count++)
      {
        DWIN_Text_Clear(FILE1_TEXT_VP+50*count, 50);
        DWIN_Hide(0xE810+0x30*count);
        DWIN_Hide(0xE960+0x30*count);
        DWIN_Hide(0xEAB0+0x30*count);
        DWIN_Hide(0xEC00+0x30*count);
      }
    }
  }

  /*UI模型文件列表翻页处理 page:当前页号(为0时直接清空)*/
  void UI_FlipOver_FileList(unsigned char page)
  {
    DWIN_IntRefresh(FILE_SELECT_ICON_VP, 0, 2);
    DWIN_IntRefresh(FILE_CURRENT_PAGE_VP, page, 2);
    DWIN_IntRefresh(FILE_PRINT_TITLE_VP, (Config_ReadLanguage()+8), 2);
    DWIN_IntRefresh(FILE_PREVIOUS_PAGE_TITLE_VP, Config_ReadLanguage(), 2);
    DWIN_IntRefresh(FILE_NEXT_PAGE_TITLE_VP, Config_ReadLanguage(), 2);
    if(page == 0)
    {
      DWIN_IntRefresh(FILE_PREVIOUS_PAGE_TITLE_VP, (Config_ReadLanguage()+8), 2);
      DWIN_IntRefresh(FILE_NEXT_PAGE_TITLE_VP, (Config_ReadLanguage()+8), 2);
    }
    if(page == 1) DWIN_IntRefresh(FILE_PREVIOUS_PAGE_TITLE_VP, (Config_ReadLanguage()+8), 2);
    if(page == File_TotalPage) DWIN_IntRefresh(FILE_NEXT_PAGE_TITLE_VP, (Config_ReadLanguage()+8), 2);
  }

  /*MCU开始打印处理 *filename:打印文件名指针*/
  void MCU_StartPrint(char *filename)
  {
    // StorageDevice_OpenFile(filename, 1);
    // RTC_StartTime();
    // StorageDevice_StartPrint();
    // Printer_Abort = false;

    card.openAndPrintFile(filename);

  }

  /*UI开始打印处理 *filename:打印文件名指针*/
  void UI_StartPrint(char *filename)
  {
    DWIN_Text_Clear(PRINT_FILE_TEXT_VP, 30);
    DWIN_StringRefresh(PRINT_FILE_TEXT_VP, filename);
    DWIN_IntRefresh(STOP_ICON_VP, 0x00, 2);
    DWIN_IntRefresh(PAUSE_RESUME_ICON_VP, 0x01, 2);
    DWIN_IntRefresh(STOP_TITLE_VP, Config_ReadLanguage(), 2);
    DWIN_IntRefresh(PAUSE_RESUME_TITLE_VP, (Config_ReadLanguage()+8), 2);
    DWIN_IntRefresh(CONTROL_PAGE_TITLE_VP, (Config_ReadLanguage()+16), 2);
    DWIN_IntRefresh(PRINT_TIME_HOUR_VP, 1000, 2);
    DWIN_IntRefresh(PRINT_TIME_MIN_VP, 100, 2);
    DWIN_IntRefresh(PRINT_TIME_SEC_VP, 100, 2);
    DWIN_IntRefresh(PRINT_REMAIN_HOUR_VP, 1000, 2);
    DWIN_IntRefresh(PRINT_REMAIN_MIN_VP, 100, 2);
    DWIN_IntRefresh(PRINT_PROCESS_VP, 0, 2);
    DWIN_IntRefresh(PRINT_PROCESS_ICON_VP, 0, 2);
    UI_PreviewShow(NULL);
    UI_PreviewShow(filename);
  }

  /*UI显示文件预览 *filename:打印文件名指针(filename为NULL时取消显示)*/
  void UI_PreviewShow(char *filename)
  {
    // FIL *preFile;
    // char *pname;
    // unsigned char *buf;
    // unsigned short int addr = MODEL_PREVIEW_VP + 2;
    // unsigned int br;
    // if(filename != NULL)
    // {
    //   preFile = (FIL*)Memory_Malloc(MEMORY_IN_BANK, sizeof(FIL));
    //   pname = Memory_Malloc(MEMORY_IN_BANK, GCODE_NAME_MAX_LENGTH*2);
    //   buf = Memory_Malloc(MEMORY_IN_BANK, 240);
    //   memset(pname, 0, GCODE_NAME_MAX_LENGTH);
    //   if(Storage_Device == Storage_SD) strcpy(pname, "0:/PICTURE/");
    //   else if(Storage_Device == Storage_USB) strcpy(pname, "2:/PICTURE/");
    //   strncat(pname, (const char*)filename, strlen(filename)-5);
    //   strcat(pname, "jpg");
    //   if(f_open(preFile, (const TCHAR*)pname, FA_READ|FA_OPEN_EXISTING) == FR_OK)
    //   {
    //     if(f_size(preFile) <= 0x7800) /*判断当前预览文件是否大于30Kb*/
    //     {
    //       while(!f_eof(preFile))
    //       {
    //         f_read(preFile, buf, 240, &br);
    //         DWIN_BufRefresh(addr, buf, br);
    //         addr += br/2;
    //       }
    //       DWIN_IntRefresh(MODEL_PREVIEW_VP+1, f_size(preFile), 2);Delay_Ms(100);
    //       DWIN_IntRefresh(MODEL_PREVIEW_VP, 0x5AA5, 2);
    //     }
    //   }
    //   f_close(preFile);
    //   Memory_Free(MEMORY_IN_BANK, preFile);
    //   Memory_Free(MEMORY_IN_BANK, pname);
    //   Memory_Free(MEMORY_IN_BANK, buf);
    // }
    // else
    // {
    //   DWIN_IntRefresh(MODEL_PREVIEW_VP, 0x00, 2);
    // }
  }

  // 弹窗界面 =====================================================================
  /*断料检测弹窗界面处理*/
  void UART_LCD_WindowPage_FilamentLack(void)
  {
    // /*仅刷一次变量*/
    // if(refreshFlag)
    // {
    //   DWIN_IntRefresh(WINDOW_TITLE_VP, Config_ReadLanguage(), 2);
    //   DWIN_IntRefresh(WINDOW_TEXT_VP, Config_ReadLanguage(), 2);
    //   DWIN_IntRefresh(WINDOW_CONFIRM_TITLE_VP, Config_ReadLanguage(), 2);
    //   DWIN_IntRefresh(WINDOW_CANCEL_TITLE_VP, (Config_ReadLanguage()+8), 2);
    //   DWIN_SwitchPage(WINDOWE_PAGE_ID);
    //   refreshFlag = RESET;
    // }
    // /*触控刷新变量*/
    // if(DWIN_receivedType.Type != TOUCH_TYPE_NO_TOUCH)
    // {
    //   switch(DWIN_receivedType.Type)
    //   {
    //     /*完成按钮*/
    //     case TOUCH_TYPE_WINDOW_CONFIRM:
    //     {
    //       if(Filament_IsLack()) Audio_Play(RECORD_UNNORMAL_PUSH);
    //       else
    //       {
    //         Audio_Play(RECORD_NORMAL_PUSH);
    //         Filament_LackNotarize(true);
    //         if(window_lastPage != UART_LCD_StartPage) currentPage = window_lastPage;
    //         else currentPage = UART_LCD_MainPage;
    //         refreshFlag = SET;
    //       }
    //     }break;
    //     /*不再提示按钮*/
    //     case TOUCH_TYPE_WINDOW_CANCEL:
    //     {
    //       Audio_Play(RECORD_NORMAL_PUSH);
    //       Filament_LackNotarize(true);
    //       Filament_SetEnable(DISABLE);
    //       if(window_lastPage != UART_LCD_StartPage) currentPage = window_lastPage;
    //       else currentPage = UART_LCD_MainPage;
    //       refreshFlag = SET;
    //     }break;
    //   }
    // }
  }

  /*断电续打弹窗界面处理*/
  void UART_LCD_WindowPage_PowerRecover(void)
  {
    // char fileName[GCODE_NAME_MAX_LENGTH];
    // /*仅刷一次变量*/
    // if(refreshFlag)
    // {
    //   DWIN_IntRefresh(WINDOW_TITLE_VP, (Config_ReadLanguage()+8), 2);
    //   DWIN_IntRefresh(WINDOW_TEXT_VP, (Config_ReadLanguage()+8), 2);
    //   DWIN_IntRefresh(WINDOW_CONFIRM_TITLE_VP, (Config_ReadLanguage()+8), 2);
    //   DWIN_IntRefresh(WINDOW_CANCEL_TITLE_VP, Config_ReadLanguage(), 2);
    //   DWIN_SwitchPage(WINDOWE_PAGE_ID);
    //   refreshFlag = RESET;
    // }
    // /*触控刷新变量*/
    // if(DWIN_receivedType.Type != TOUCH_TYPE_NO_TOUCH)
    // {
    //   switch(DWIN_receivedType.Type)
    //   {
    //     /*继续打印按钮*/
    //     case TOUCH_TYPE_WINDOW_CONFIRM:
    //     {
    //       MCU_PowerRecoverPrint(true);
    //       StorageDevice_GetCurrentFile(fileName);
    //       UI_StartPrint(fileName);
    //       if(window_lastPage != UART_LCD_StartPage) currentPage = window_lastPage;
    //       else currentPage = UART_LCD_MainPage;
    //       refreshFlag = SET;
    //     }break;
    //     /*取消按钮*/
    //     case TOUCH_TYPE_WINDOW_CANCEL:
    //     {
    //       MCU_PowerRecoverPrint(false);
    //       if(window_lastPage != UART_LCD_StartPage) currentPage = window_lastPage;
    //       else currentPage = UART_LCD_MainPage;
    //       refreshFlag = SET;
    //     }break;
    //   }
    // }
  }

  /*温度异常弹窗界面处理*/
  void UART_LCD_WindowPage_TempError(void)
  {
    // /*仅刷一次变量*/
    // if(refreshFlag)
    // {
    //   DWIN_IntRefresh(WINDOW_TITLE_VP, (Config_ReadLanguage()+16), 2);
    //   DWIN_IntRefresh(WINDOW_TEXT_VP, (Config_ReadLanguage()+16), 2);
    //   DWIN_IntRefresh(WINDOW_CONFIRM_TITLE_VP, (Config_ReadLanguage()+16), 2);
    //   DWIN_IntRefresh(WINDOW_CANCEL_TITLE_VP, (Config_ReadLanguage()+8), 2);
    //   DWIN_SwitchPage(WINDOWE_PAGE_ID);
    //   refreshFlag = RESET;
    // }
    // /*触控刷新变量*/
    // if(DWIN_receivedType.Type != TOUCH_TYPE_NO_TOUCH)
    // {
    //   switch(DWIN_receivedType.Type)
    //   {
    //     /*确定按钮*/
    //     case TOUCH_TYPE_WINDOW_CONFIRM:
    //     {
    //       if(window_lastPage != UART_LCD_StartPage) currentPage = window_lastPage;
    //       else currentPage = UART_LCD_MainPage;
    //       refreshFlag = SET;
    //     }break;
    //     /*不再提示按钮*/
    //     case TOUCH_TYPE_WINDOW_CANCEL:
    //     {
    //       if(window_lastPage != UART_LCD_StartPage) currentPage = window_lastPage;
    //       else currentPage = UART_LCD_MainPage;
    //       refreshFlag = SET;
    //     }break;
    //   }
    // }
  }

  /*MCU断电续打处理 print:1,续打;2,取消*/
  void MCU_PowerRecoverPrint(bool print)
  {
  // #if POWEROFF_SAVE_FILE
  //   char cmd[MAX_CMD_SIZE];
  //   if(print)
  //   {
  //     sprintf_P(cmd, PSTR("M190 S%d"), PowerOff_InfoStruct.target_temperature_bed);
  //     queue.enqueue_one_now(cmd);
  //     sprintf_P(cmd, PSTR("M109 S%d"), PowerOff_InfoStruct.target_temperature_head[0]);
  //     queue.enqueue_one_now(cmd);
  //     queue.enqueue_one_now("M106 S255");
  //     Audio_Play(RECORD_PRINT_START);
  //     RTC_StartTime();
  //     StorageDevice_StartPrint();
  //     Printer_Abort = false;
  //   }
  //   else
  //   {
  //     PowerOff_Commands_Count = 0;
  //     StorageDevice_DeleteFile(PowerOff_FileName);
  //     StorageDevice_SetPrinting(false);
  //     Printer_ClearCommand();
  //     Stepper_QuickStop();
  //     Heater_AutoShutdown();
  //     StorageDevice_CloseFile();
  //     Printer_Abort = true;
  //     queue.enqueue_one_now(FILE_FINISHED_RELEASECOMMAND);
  //     Audio_Play(RECORD_NORMAL_PUSH);
  //   }
  // #endif
  }

  // 弹窗界面 =====================================================================
  /*待机界面处理*/
  void UART_LCD_StandbyPage(void)
  {
    /*仅刷一次变量*/
    if(refreshFlag)
    {
      DWIN_IntRefresh(STANDBY_LOGO_ICON_VP, 1, 2);
      DWIN_IntRefresh(STANDBY_ARROW_ICON_VP, 1, 2);
      DWIN_SwitchPage(STANDBY_PAGE_ID);
      if(Config_ReadLuminance() >= 20) DWIN_Backlight_SetLuminance(20);
      refreshFlag = RESET;
    }
    /*触控刷新变量*/
    if(DWIN_receivedType.Type != TOUCH_TYPE_NO_TOUCH)
    {
      switch(DWIN_receivedType.Type)
      {
        /*唤醒HMI*/
        case TOUCH_TYPE_WAKE_UP:
        {
          previous_millis_lcd = HAL_GetTick();
          HMI_SetStandBy(false);
          DWIN_Backlight_SetLuminance(Config_ReadLuminance());
          currentPage = standby_lastpage;
          refreshFlag = SET;
        }break;
      }
    }
  }


#endif
