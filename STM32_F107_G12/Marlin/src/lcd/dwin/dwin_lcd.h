#ifndef	DWIN_LCD_H
#define	DWIN_LCD_H

//#include "../../pins/pins.h"
#include "Dwin_uart.h"
#include "../../inc/MarlinConfig.h"
#include <stddef.h>

// 临时引脚定义，这里的引脚没有实际用途，仅为了兼容之前的代码 ================
// 
// CHECKFILEMENT
//
#define CHECKFILEMENT	  true //false 关闭断料检测
#define FIL_RUNOUT_PIN   PB0  //  Z_MIN_PIN //
#define Add_FILAMENT_PIN	PE0  // LIGHT_PIN 

#define VALID_LEVEL  1 //断料检测的有效电平

// ================light set=====================
#define E_STOP_PIN			PB1 //X_MAX_PIN  //
#define RED_LIGHT_PIN 		PE1 // FAN2_PIN  // 
//#define YELLOW_LIGHT_PIN	PE11 //  FAN1_PIN // 
#define GREEN_LIGHT_PIN	PE9 //  FAN_PIN // 
#define BEEP_PIN  			PE1 //FAN2_PIN // 

#define ON true
#define OFF false

enum LIGHT{red=1, yellow, green, red_green};


#define	FanOn            255
#define	FanOff           0

#define	FONT_EEPROM      0
// ******************0x1000-0xFFFF****************************
/*variable addr*/
#define	StartIcon		  	0x1000
#define	MainPageKey			0x1002
#define	PrintingPageKey		0x1004
#define	FeedrateVar			0x1006
#define	PrintscheduleIcon		0x100E
#define	TimehourVar			0x1010
#define	TimeminVar			0x1012
#define	PercentageVar		0x1016

#define	AddFilamentIcon		0x101A
#define	MotorIcon			0x101B
#define	EmergencyIcon		0x101C
#define	FanKeyIcon			0x101E

#define	PLA_ABS_Nzll0TVar	0x1020
#define	PLA_ABS_Nzll1TVar	0x1022
#define	PLA_ABS_BedTVar	0x1024
#define	ZOffsetVar       		0x1026
#define	FlowPercentageVar	0x102A
#define	Nozzle0TempIcon		0x102D
#define	Nozzle1TempIcon		0x102E
#define	BedTempIcon		0x102F

#define	TempPageKey		0x1032
#define	Nozzle0PreheatVar	0x1034
#define	Nozzle0TempVar		0x1036
#define	Nozzle1PreheatVar	0x1038
#define	Nozzle1TempVar		0x103A
#define	BedPreheatVar		0x103C
#define	BedTempVar			0x103E

#define	ControlOtherkey		0x1040
#define	ControlMovekey		0x1042
#define	ControlMoveUnitIcon	0x1043
#define	AuxLevelPageKey		0x1044
#define	AutoLevelPageKey	0x1046
#define	AutolevelIcon		0x1048
#define	DisplayXaxisVar		0x104A
#define	DisplayYaxisVar		0x104C
#define	DisplayZaxisVar		0x104E
#define	FilementUnit0		0x1054
#define 	SettingPageKey		0x1056
#define 	LanguageIcon		0x1057
#define 	NoFilamentPageKey	0x1058
#define 	PopUpKey			0x1059
#define 	PowerlossPageKey	0x105E

#define	MacVersionText		0x1060
#define	SoftVersionText		0x106A
#define	ScreenVerText		0x1074
#define	PrinterSizeText		0x107E
#define	CorpWebsiteText		0x1092 // 0x1092 -0X10B0

#define	AutolevelVaR			0x1100 // 0x1100-0x1200

#define	Main1Title			0x1200
#define	Main2Title			0x1201
#define	Main3Title			0x1202
#define	Printing1Title			0x1203
#define	PrintingStopTitle		0x1204
#define	PrintingPauseTitle	0x1205
#define	PrintingAjustTitle		0x1206
#define	PrintingResumeTitle	0x1207
#define	Printing2Title			0x1208
#define	Printing3Title			0x1209
#define	FilenameTitle			0x120A
#define	TempKeyCool1Title	0x120B
#define	TempAllCool1Title		0x120C
#define	TempKeyCool2Title	0x120D
#define	TempAllCool2Title		0x120E
#define	TempYesNoTitle		0x120F
#define	Autolevel1Title		0x1210
#define	Autolevel2Title		0x1211
#define	Control1Title			0x1212
#define	Control2Title			0x1213
#define	SettingTitle			0x1214
#define	PopUp1Title			0x1215
#define	PopUpInforTitle		0x1216
#define	PopUp2Title			0x1217
#define	PopUp3Title			0x1218
#define	PowerLossTitle		0x1219
#define	EmergencyTitle		0x121A
#define	TestTitle			0x121B
#define ServoAlarmTitle     0x121C

#define	ErrorText			0x1400
#define	ErrorPageKey		0x1420
#define	ErrorPageText		0x1422
#define	TestPageKey			0x1430
#define	XGearRatioVar		0x1432
#define	YGearRatioVar		0x1434
#define	ZGearRatioVar		0x1436
#define	EGearRatioVar		0x1438
#define	XMaxAccelerationVar	0x1442 
#define	YMaxAccelerationVar	0x1444  
#define	ZMaxAccelerationVar	0x1446  
#define	EMaxAccelerationVar	0x1448  
#define	XMaxSpeedVar		0x143A
#define	YMaxSpeedVar		0x143C
#define	ZMaxSpeedVar		0x143E
#define	EMaxSpeedVar		0x1440
#define	XMaxJerkAccVar		0x144A
#define	YMaxJerkAccVar		0x144C
#define	ZMaxJerkAccVar		0x144E
#define	EMaxJerkAccVar		0x1450

#define	PrintingFileText		0x1500

#define	FilePageKey			0x2000
#define	ChooseFileKey		0x2001
#define	ShowChosenFileIcon	0x2010
#define	FilenamesText		0x2100 // 0x2100-0x2600
// **********************************************

// 中英文错误提示内容对应的Unicode值
const char error_sure[4] = {0x78,0x6e,0x8b,0xa4};
const char error_sure_en[14] = {0x00,0x43,0x00,0x6f,0x00,0x6e,0x00,0x66,0x00,0x69,0x00,0x72,0x00,0x6d};
const char error_201[24] = {0x95,0x19,0x8b,0xef,0x00,0x3a,0x00,0x32,0x00,0x30,0x00,0x31,0x00,0x28,0x54,0x7d,0x4e,0xe4,0x8d,0x85,0x65,0xf6,0x00,0x29};
const char error_201_en[40] = {0x00,0x32,0x00,0x30,0x00,0x31,0x00,0x28,0x00,0x43,0x00,0x6f,0x00,0x6d,0x00,0x6d,0x00,0x61,0x00,0x6e,0x00,0x64,0x00,0x20,0x00,0x54,0x00,0x69,0x00,0x6d,0x00,0x65,0x00,0x6f,0x00,0x75,0x00,0x74,0x00,0x29};
const char error_202[24] = {0x95,0x19,0x8b,0xef,0x00,0x3a,0x00,0x32,0x00,0x30,0x00,0x32,0x00,0x28,0x56,0xde,0x96,0xf6,0x59,0x31,0x8d,0x25,0x00,0x29};
const char error_202_en[36] = {0x00,0x32,0x00,0x30,0x00,0x32,0x00,0x28,0x00,0x48,0x00,0x6f,0x00,0x6d,0x00,0x69,0x00,0x6e,0x00,0x67,0x00,0x20,0x00,0x46,0x00,0x61,0x00,0x69,0x00,0x6c,0x00,0x65,0x00,0x64,0x00,0x29};
const char error_203[24] = {0x95,0x19,0x8b,0xef,0x00,0x3a,0x00,0x32,0x00,0x30,0x00,0x33,0x00,0x28,0x63,0xa2,0x6d,0x4b,0x59,0x31,0x8d,0x25,0x00,0x29};
const char error_203_en[38] = {0x00,0x32,0x00,0x30,0x00,0x33,0x00,0x28,0x00,0x50,0x00,0x72,0x00,0x6f,0x00,0x62,0x00,0x69,0x00,0x6e,0x00,0x67,0x00,0x20,0x00,0x46,0x00,0x61,0x00,0x69,0x00,0x6c,0x00,0x65,0x00,0x64,0x00,0x29};
const char error_204[26] = {0x95,0x19,0x8b,0xef,0x00,0x3a,0x00,0x32,0x00,0x30,0x00,0x34,0x00,0x28,0x8b,0xf7,0x70,0xb9,0x51,0xfb,0x91,0xcd,0x54,0x2f,0x00,0x29};
const char error_204_en[34] = {0x00,0x32,0x00,0x30,0x00,0x34,0x00,0x28,0x00,0x43,0x00,0x6c,0x00,0x69,0x00,0x63,0x00,0x6b,0x00,0x20,0x00,0x52,0x00,0x65,0x00,0x62,0x00,0x6f,0x00,0x6f,0x00,0x74,0x00,0x29};

#define TEST_MODE

//*****************Error*************************
/*Error value*/
// #define Error_201   "错误:201(命令超时)"   // The command too much inactive time
#define Error_201   error_201   // The command too much inactive time
// #define Error_202   "错误:202(回零失败)"     // Homing Failed
#define Error_202   error_202     // Homing Failed
// #define Error_203   "错误:203(探测失败)"    // Probing Failed
#define Error_203   error_203    // Probing Failed
// #define Error_204   "错误:204(请点击重启)"      // SD Read Error
#define Error_204    error_204     // SD Read Error
// #define Error_Sure   "   确认   "
#define Error_Sure   error_sure

// #define Error_201_En   "201(Command Timeout)"   // The command too much inactive time
#define Error_201_En   error_201_en   // The command too much inactive time
// #define Error_202_En   "202(Homing Failed)"     // Homing Failed
#define Error_202_En   error_202_en     // Homing Failed
// #define Error_203_En   "203(Probing Failed)"    // Probing Failed
#define Error_203_En   error_203_en    // Probing Failed
// #define Error_204_En   "204(Click Reboot)"      // SD Read Error
#define Error_204_En   error_204_en      // SD Read Error
// #define Error_Sure_En   "Confirm"
#define Error_Sure_En   error_sure_en

enum ERROR_MODE{
Error_Mode_None = 0,
Error_Mode_201 = 1,
Error_Mode_202,
Error_Mode_203,
Error_Mode_204,
};

//**********************************************
#define	FileNameCharLen	   40
#define	PAGEMAXNUM  10

#define	TEXT_VIEW_LEN		(FileNameCharLen/2)
#define	SUM_FILES	28
typedef struct CardRecord
{
    int recordcount;
    int Filesum;
    unsigned long addr[SUM_FILES];
    char Cardshowfilename[SUM_FILES][FileNameCharLen];
    char Cardfilename[SUM_FILES][FileNameCharLen];
    char Cardlongfilename[SUM_FILES][FileNameCharLen];
}CRec;

typedef struct PageInf
{
    unsigned short pagename;
    unsigned short pagenum[PAGEMAXNUM];
} PGINF;

enum PAGE_NAME{
PMain = 0,
PPrinting,
PFile,
PTempControl,
PAuxlevel,
PAutobedlevel,
PMoveControl,
PSetting,
PKeyboard,
PNoFilement,
PPopUp,
PThermalRunaway,
PHeatingFailed,
PThermistorError,
PPowerloss,
PEmergencyStop,
PError,
PTest,
PServoAlarm,
};

enum KEY_VALUE{
key_zero = 0,
key_one,
key_two,
key_three,
key_four,
key_five,
key_six,
key_seven,
key_eight,
key_night,
key_ten,
key_eleven,
key_twelve,
key_thirteen,
key_fourteen,
key_fifteen,
key_sixteen,
key_seventeen,
key_eighteen,
key_nineteen,
};

enum MODE_AFTER_HOME{
None = 0,
MoveHome = 1,
AuxLevelHome,
AutoLevelHome,
NoFilamentHome,
PauseHome,
StopHome,
};

enum OTHER_MODE{
Stop_Mode = 1,
Pause_Mode,
Language_Mode,
RestoreFactory_Mode,
};

char Get_Light_Mode(void);
void Light_init(void);
void Servo_alarm_init(void);
void Set_Light(char mode, bool status);
void Emergency_Stop(void);
void SDCard_Init(void);
bool SDCard_Detected(void);
void SDCard_Upate(void);
void SDCard_File_Refresh(void);

//*****************************************************
void Filament_pin_init(void);
bool No_Filament_Check(void);
void Servo_alarm_detect(void);
void Filament_Check_InPrinting(void);
void Printing_Pause_Process(void);
void Start_up_Procedure(void);
void main_page_data_init(void);
void printing_page_data_init(void);
void file_page_data_init(void);
void temp_page_data_init(void);
void autolevel_page_data_init(void);
void movecontrol_page_data_init(void);
void setting_page_data_init(void);
void error_page_data_init(void);
void All_Page_init(void);


//********************************************************************//
void printing_stop(void);
void printing_pause(void);
void no_filament_printing_pause(void);
void printing_resume(void);
void fileshow_handle(void);
void temperature_all_show(void);
void PLA_last_data_set(int16_t nozzle0, int16_t nozzle1, int16_t bed);
void PLA_data_show(void);
void PLA_execute(void);
void ABS_last_data_set(int16_t nozzle0, int16_t nozzle1, int16_t bed);
void ABS_data_show(void);
void ABS_execute(void);
long pow(int num, unsigned int time);
void Key_to_print(void);
void Main_page(void);
void Printing_page(void);
void File_page(void);
void Temp_page(int pagenum);
void Assitantlevel_page(void);
void Autobedlevel_page(void);
void MoveControl_page(void);
void Setting_page(void);
void LCD_Handle(unsigned long addr, unsigned short data);

void Print_Completed(void);
void After_Home(void);
void Show_Leveling_progress(xy_int8_t meshCount);
void Leveling_completed(void);
void Thermal_Runaway_fail(void);
void Heating_fail(void);
void Error_Mode_Handle(uint8_t mode);

bool time_refresh(unsigned long *var, unsigned long time_units);
void Language_Update(void);
void Refresh_data(void);
void LCD_Init(void);
void LCD_Update();
 
#endif // DWIN_LCD_H
