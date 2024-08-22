#include <wstring.h>
#include <stdio.h>
#include <string.h>
#include <Arduino.h>
#include "dwin_uart.h"
#include "dwin_lcd.h"

#include "../../MarlinCore.h"
#include "../../inc/MarlinConfig.h"
#include "../../module/settings.h"
#include "../../core/serial.h"
#include "../../core/macros.h"
#include "../../pins/pins.h"

#include "../fontutils.h"
#include "../../sd/cardreader.h"
#include "../../feature/powerloss.h"
#include "../../feature/babystep.h"
#include "../../module/temperature.h"
#include "../../module/printcounter.h"
#include "../../module/motion.h"
#include "../../module/planner.h"
#include "../../module/stepper/indirection.h"
#include "../../gcode/queue.h"
#include "../../gcode/gcode.h"
#include "../../module/probe.h"

#include "../../../Version.h"
#include "../../../Configuration.h"
#include "../../feature/bedlevel/bedlevel.h"
#include "../../feature/bedlevel/abl/abl.h"
#include "../../module/planner.h"

#ifdef USB_DISK_SUPPORT
  #include "../../sd/usb_disk/Sd2Card_UDisk.h"
#endif

#if ENABLED(EEPROM_SETTINGS)
  #include "../../HAL/shared/eeprom_api.h"
  #include "../../module/settings.h"
#endif

extern UARTDATA dwinlcd;
bool pause_action_flag = 0;
bool Auto_Close_Flag = 0;
bool emergency_stop_status = false;
CRec CardRecbuf;
  
const PGINF DwinPageInf[] ={
{ PMain, {1}},
{ PPrinting, {2,3,4,5,6}},
{ PFile, {7,8,9,10}},
{ PTempControl, {11,12,13,14}},
{ PAuxlevel, {15,16}},
{ PAutobedlevel, {17,18}},
{ PMoveControl, {19,20}},
{ PSetting, {21}},
{ PKeyboard, {22, 23}},
{ PNoFilement, {24}},
{ PPopUp, {25, 26, 27}},
{ PThermalRunaway, {28}},
{ PHeatingFailed, {29}},
{ PThermistorError, {30}},
{ PPowerloss, {31}},
{ PEmergencyStop, {32}},
{ PError, {33}},
{ PTest, {34,35,36,37}},
{ PServoAlarm, {38}},
{ 255,{255}},
};

float FilamentUnitbuf[2] = {200, 200};
unsigned char Language = 1;
float last_zoffset = 0.0;
// motor disable status
bool motor_disable_status = true;
bool Motor_Disable_Status(void){return motor_disable_status;}
void Motor_Disable_Status_Set(bool status){motor_disable_status = status;}

// represents SD-card status, true means SD is available, false means opposite.
bool sd_refreshed_status;
bool SDCard_Inserted(void){return sd_refreshed_status;}
void SDCard_Inserted_Set(bool status){sd_refreshed_status = status;}

// CardCheckStatus represents if to check card status in printing or not, with value as true for on , but false for off
bool CardCheckStatus = false;
bool SDCard_Out_ToMainPage_OnOff(void){return CardCheckStatus;}
void SDCard_Out_ToMainPage_Set(bool status){ CardCheckStatus = status;}

//the status of abosrbing filaments
bool	add_filaments = false;
bool Add_Filaments(void){return add_filaments;}
void Add_Filaments_Set(bool status){add_filaments = status;}

//the emergency key open or not
bool	emergency_key_status = false;
bool Emergency_Status(void){return emergency_key_status;}
void Emergency_Status_Set(bool status){emergency_key_status = status;}

//the mode about the page changing after home
char mode_after_home = 0;
void Mode_After_Home_set(char mode){mode_after_home = mode;}
char Mode_After_Home_get(void){return mode_after_home;}

//the mode about choosing the stop key , the pause key , language , the reset
char other_mode = 0;
char record_page = 0;
void Other_Mode_set(char mode){other_mode = mode;}
char Other_Mode_get(void){return other_mode;}
uint32_t Filament_time;
#if CHECKFILEMENT
//if begin to check filament status or not
bool FilamentCheckOrNot = false;
bool Checkfilament_InPrinting(void){return FilamentCheckOrNot;}
void Checkfilament_InPrinting_Set(bool status){FilamentCheckOrNot = status;}
#endif

// X, Y, Z axis move unit
uint8_t Axis_Move_Unit = 100;
void Axis_MoveUnit_set(float val)
{
	Axis_Move_Unit = val*10;
	if(Axis_Move_Unit == 100) dwin_icon_set(0, ControlMoveUnitIcon);
	else if(Axis_Move_Unit == 10) dwin_icon_set(1, ControlMoveUnitIcon);
	else if(Axis_Move_Unit == 1) dwin_icon_set(2, ControlMoveUnitIcon);
}
float Axis_MoveUnit_get(void){return (float)Axis_Move_Unit/10;}
// record error mode
uint8_t error_mode = 0;
void Error_Mode_set(uint8_t mode){error_mode = mode;}
uint8_t Error_Mode_get(void){return error_mode;}

// EEPROM存储处理 ======================================================
#define EEPROM_OFFSET 100 // 需与Settings.cpp中的EEPROM_OFFSET一致
void BL24CXX_Write(uint16_t WriteAddr,uint8_t *pBuffer,uint16_t NumToWrite)
{
	persistentStore.access_start();
	persistentStore.write_data(EEPROM_OFFSET + settings.datasize() + WriteAddr, pBuffer, NumToWrite);
	persistentStore.access_finish();
}

void BL24CXX_Read(uint16_t ReadAddr,uint8_t *pBuffer,uint16_t NumToRead)
{
	persistentStore.access_start();
	persistentStore.read_data(EEPROM_OFFSET + settings.datasize() + ReadAddr, pBuffer, NumToRead);
	persistentStore.access_finish();
}

  const float manual_feedrate_mm_m[] = { 60*60, 60*60, 8*60, 80*60 }; 
// 轴的线性移动
inline void RTS_line_to_current(AxisEnum axis) 
{
	if (!planner.is_full())
	{
		planner.buffer_line(current_position, MMM_TO_MMS(manual_feedrate_mm_m[(int8_t)axis]), active_extruder);
	}
}

// LED灯的相关设置
char light_set = 0;
char Get_Light_Mode(void){return light_set;}

// LED引脚初始化
void Light_init(void)
{
	// 灯的引脚初始化
	SET_OUTPUT(RED_LIGHT_PIN);
	//SET_OUTPUT(YELLOW_LIGHT_PIN);
	SET_OUTPUT(GREEN_LIGHT_PIN);
	SET_INPUT(E_STOP_PIN);

	//Set_Light(yellow, ON);
	// Set_Light(green, ON);
}

// 伺服电机报警引脚初始化
void Servo_alarm_init(void)
{
	SET_INPUT(SERVO_ALARM_PIN);
}

// LED灯的模式设置，可以设置亮红灯，亮黄灯或亮绿灯
void Set_Light(char mode, bool status)
{
	if(mode == red)
	{
		WRITE(RED_LIGHT_PIN, status);
		
		WRITE(GREEN_LIGHT_PIN, !status);
		//WRITE(YELLOW_LIGHT_PIN, !status);
	}
	else if(mode == yellow)
	{
		//WRITE(YELLOW_LIGHT_PIN, status);
		
		WRITE(RED_LIGHT_PIN, !status);
		WRITE(GREEN_LIGHT_PIN, !status);
	}
	else if(mode == green)
	{
		WRITE(GREEN_LIGHT_PIN, status);
		
		WRITE(RED_LIGHT_PIN, !status);
		//WRITE(YELLOW_LIGHT_PIN, !status);
	}
	else if(mode == red_green)
	{
		WRITE(GREEN_LIGHT_PIN, status);
		WRITE(RED_LIGHT_PIN, status);
	}
	light_set = mode;
}

// 紧急停止函数
void Emergency_Stop(void)
{
	if(READ(E_STOP_PIN) == 0 && !emergency_stop_status) //emergency stop
	{
		emergency_stop_status = true;
		Set_Light(red, ON);
		dwin_page_set(DwinPageInf[PEmergencyStop].pagenum[0]);
		kill(M112_KILL_STR, nullptr, true);
		// if(card.isPrinting())
		// {
		// 	//SERIAL_ECHOLNPAIR("\n 11light_set =  ",(int)Get_Light_Mode());
		// 	printing_stop();
		// }
		// else
		// {
		// 	quickstop_stepper();
		// 	thermalManager.disable_all_heaters();
		// }

	}
	else if(READ(E_STOP_PIN) == 1 && emergency_stop_status)
	{
		//Set_Light(yellow, ON);
		Set_Light(red_green, OFF);
		emergency_stop_status = false;
		dwin_page_set(DwinPageInf[PMain].pagenum[0]);
	}
	else if(card.isPrinting() && Get_Light_Mode() != green)
	{
		Set_Light(green, ON);
	}
	else if(!card.isPrinting() && Get_Light_Mode() != red && Get_Light_Mode() != yellow)
	{
		//Set_Light(yellow, ON);
		Set_Light(red_green, OFF);
	}
}

//文件的初始化
void SDCard_Init(void)
{
	if(SDCard_Detected())
	{
		#ifndef USB_DISK_SUPPORT
		card.mount();
		#endif

		if(CardReader::flag.mounted)
		{
			uint16_t fileCnt = card.get_num_Files();
			card.getWorkDirName();
			if(card.filename[0] != '/')
			{
				card.cdup();
			}

			int addrnum = 0;
			int num = 0;
			for(uint16_t i = 0;(i < fileCnt) && (i < (FileNameCharLen + addrnum));i ++)
			{
				card.selectFileByIndex(fileCnt - 1 - i);
				char longfilenameunicode[LONG_FILENAME_LENGTH];
				char *pointFilename = card.longFilename;
				int filenamelen = strlen(card.longFilename);
				int j = 1;
				while((strncmp(&pointFilename[j], ".gcode", 6) && strncmp(&pointFilename[j], ".GCODE", 6)) && ((j ++) < filenamelen));
				if(j >= filenamelen)
				{
					addrnum++;
					continue;
				}

				if (j >= (FileNameCharLen-2))
				{
					strncpy(&card.longFilename[FileNameCharLen - 3], "..", 2);
					card.longFilename[FileNameCharLen - 1] = '\0';
					j = FileNameCharLen - 1;
				}

				memset(longfilenameunicode, 0, sizeof(longfilenameunicode));
				for(int k = 0; k < j; k++)
				{
					longfilenameunicode[2*k] = ( card.longFilenameUnicode[k] >> 8 ) & 0x00FF;
					longfilenameunicode[2*k+1] = card.longFilenameUnicode[k] & 0x00FF;
				}

				if(j > 20)
				{
					j = 20; //目前只分配了40个字节的空间
				}

				memcpy(CardRecbuf.Cardlongfilename[num], longfilenameunicode, 2*j);
				strncpy(CardRecbuf.Cardshowfilename[num], card.longFilename, j);
				strcpy(CardRecbuf.Cardfilename[num], card.filename);
				CardRecbuf.addr[num] = FilenamesText +num*TEXT_VIEW_LEN;
				dwin_unicode_set(CardRecbuf.Cardlongfilename[num],CardRecbuf.addr[num],2*j);
				dwin_icon_set(0, ShowChosenFileIcon + num);
				CardRecbuf.Filesum = (++num);
			}

			SDCard_Inserted_Set(true);
			return;
		}
	}
	//未检测到SD卡，或者挂载失败
	SERIAL_ECHOLN("***sd not detected, or init failed***");
}

// U盘检测，是否有U盘
bool SDCard_Detected(void)
{
  #ifdef USB_DISK_SUPPORT
    return Sd2Card::isInserted();
  #else
    uint32_t count=0;
    while(IS_SD_INSERTED()) // 20毫秒滤波
    {
		if(++count <= 4) delay(5);
		else break;
    }
	
    return (count > 4?true:false);
  #endif
}

// 文件页面更新
void SDCard_Upate(void)
{	
	const bool current_sd_status = SDCard_Detected();
	if (current_sd_status != SDCard_Inserted())
	{
		if (current_sd_status)
		{
			SERIAL_ECHOLN("sd_status changed, sd_status = yes");
			//safe_delay(100);
			SDCard_Init();
		}
		else
		{
			SERIAL_ECHOLN("sd_status changed, sd_status = no");
			card.release();
			// heating in printing or printing
			if(SDCard_Out_ToMainPage_OnOff())
			{
				card.flag.abort_sd_printing = true;
				wait_for_heatup = false;
				// cancel to check card during printing the gcode file 
				SDCard_Out_ToMainPage_Set(false);
				
				// back main page
				dwin_page_set(1); // exchange to 1 page
			}

			for(int i = 0;i < CardRecbuf.Filesum;i++)
			{
				for(int j = 0;j < TEXT_VIEW_LEN;j++)
					dwin_var_set(0,CardRecbuf.addr[i]+j);
			}

			LOOP_L_N(i, TEXT_VIEW_LEN){dwin_var_set(0,PrintingFileText+i);}
			LOOP_L_N(i, SUM_FILES){dwin_var_set(0,ShowChosenFileIcon+i);}
			memset(&CardRecbuf,0,sizeof(CardRecbuf));
		}
		SDCard_Inserted_Set(current_sd_status);
	}

}

// 文件页面中的文件更新
void SDCard_File_Refresh(void)
{
	// represents to update file list
	if (SDCard_Inserted())
	{
		SERIAL_ECHOLN("update card files");
		for (uint16_t i = 0; i < CardRecbuf.Filesum ; i++) 
		{
			delay(1);
			dwin_unicode_set(CardRecbuf.Cardlongfilename[i],CardRecbuf.addr[i],2*strlen(CardRecbuf.Cardshowfilename[i]));
			dwin_icon_set(0,ShowChosenFileIcon+i);
		}
	}
}

//*****************************************************
// 检测是否有物流的引脚初始化
void Filament_pin_init(void)
{
    #if CHECKFILEMENT
    //设置耗材检测引脚为输入
      SET_INPUT(FIL_RUNOUT_PIN);
    #endif
	// absorb filament pin
	SET_OUTPUT(ABSORB_FILAMENT_PIN);
}

//return value:true means no filament; false means having filament.
bool is_no_filament(void)
{
	if(VALID_LEVEL == READ(FIL_RUNOUT_PIN))
	{
		delay(10);
		if(VALID_LEVEL == READ(FIL_RUNOUT_PIN))
		{
			return true;
		}
	}

	return false;
}

//return value:true means no filament; false means having filament.
bool No_Filament_Check(void)
{
	static int count = 1;
	//for(; VALID_LEVEL == READ(FIL_RUNOUT_PIN)  && (count < 25); count++){ delay(10);}
	if(VALID_LEVEL == READ(FIL_RUNOUT_PIN)  && (count < 50)) count++;
	else count = 1;
	return (50 <= count ? true:false);
}
//15s后自动关闭进料
void Auto_Close_Filament()
{

    if(millis()-Filament_time > 1000*15 && Auto_Close_Flag)
	{
		Auto_Close_Flag = 0;
	    dwin_icon_set(0, AddFilamentIcon);
		WRITE(Add_FILAMENT_PIN, LOW);
		Add_Filaments_Set(false);
	}
 
} 
// 打印过程中，物料的情况检测
void Filament_Check_InPrinting(void)
{
	static bool addFilamentStatus = false;
	static uint32_t update_time;
	static char addFilament_cnt = 0;
	 Auto_Close_Filament();
	if(card.isPrinting())
	{
		if(No_Filament_Check() && !addFilamentStatus)
		{
			if(addFilament_cnt < 3) // start to absorb filament
			{
				addFilament_cnt++;
				dwin_icon_set(1, AddFilamentIcon);
				WRITE(Add_FILAMENT_PIN, HIGH);
				addFilamentStatus = true;
				update_time = millis();
			
			}
			else // three times, absorb/get no filament
			{
				no_filament_printing_pause();			
			}
		}
		else if(addFilamentStatus && (millis()-update_time > 1000*15))// after 15 seconds, close absorbing filament
		{
			addFilamentStatus = false;
			dwin_icon_set(0, AddFilamentIcon);
			WRITE(Add_FILAMENT_PIN, LOW);
			Auto_Close_Flag = 0;
		}
		else if(millis()-update_time > 1000*60 && addFilament_cnt)
		{
			addFilament_cnt = 0;
		}
	}
}
 
//打印过程中，暂停后，喷头移动的位置
void Printing_Pause_Process(void)
{
	float position_x,position_y;

	if(pause_action_flag && printingIsPaused() && !planner.has_blocks_queued())
	{
		pause_action_flag = false;
		report_current_position();
		position_x = current_position.x;
		position_y = current_position.y;
		if(position_x <= 600 && position_y <= 500)
		{
			queue.inject_P(PSTR("G0 X50 Y50"));
			TERN_(RTS_AVAILABLE, After_Home());
		}
		else if(position_x <= 600 && position_y <= 1000)
		{
			queue.inject_P(PSTR("G0 X50 Y950"));
			TERN_(RTS_AVAILABLE, After_Home());
		}
		else if(position_x <= 1200 && position_y <= 500)
		{
			queue.inject_P(PSTR("G0 X1150 Y50"));
			TERN_(RTS_AVAILABLE, After_Home());
		}
		else
		{
			queue.inject_P(PSTR("G0 X1150 Y950"));
			TERN_(RTS_AVAILABLE, After_Home());
		}
	}
}

// 启动过程的进度条执行
void Start_up_Procedure(void)
{
	LOOP_L_N(i, 2*100)
	{
		if(i < 100) dwin_icon_set(i,StartIcon);
		else dwin_icon_set((i-100),StartIcon+1);
		delay(10);
	}
	dwin_page_set(DwinPageInf[PMain].pagenum[0]);
}

// 主页面的参数，图标等等初始化
void main_page_data_init(void)
{
	// temperature , 温度
	dwin_icon_set(0, Nozzle0PreheatVar);
	dwin_icon_set(0, Nozzle1PreheatVar);  
	dwin_icon_set(0, BedPreheatVar);
	dwin_icon_set(thermalManager.temp_hotend[0].celsius, Nozzle0TempVar);
	dwin_icon_set(thermalManager.temp_hotend[1].celsius, Nozzle1TempVar);
	dwin_icon_set(thermalManager.temp_bed.celsius, BedTempVar);
	//motor icon ，电机按钮
	dwin_icon_set(0, MotorIcon);
	Motor_Disable_Status_Set(false);
	// 紧急停止按钮
	dwin_icon_set(0, EmergencyIcon); 
	Emergency_Status_Set(false);
}

// 打印页面的参数，图标等等初始化
void printing_page_data_init(void)
{
	// 打印文件名
	LOOP_L_N(i, TEXT_VIEW_LEN){dwin_var_set(0,PrintingFileText+i);}
	// 时间
	dwin_var_set(0,TimehourVar);
	dwin_var_set(0,TimeminVar);
	// 打印进度
	dwin_icon_set(0,PrintscheduleIcon);
	dwin_icon_set(0,PercentageVar);
	// 打印速率
	dwin_icon_set(feedrate_percentage = 100, FeedrateVar);
	// 进料速率
	dwin_icon_set(planner.flow_percentage[0], FlowPercentageVar);
	#ifdef HAS_FAN
	// turn off fans，风扇
	thermalManager.set_fan_speed(0, FanOff);
	#endif
	dwin_icon_set(0, FanKeyIcon);
	//absorb filament，吸料按钮
	dwin_icon_set(0, AddFilamentIcon);
	Auto_Close_Flag = 0;
	Add_Filaments_Set(false);
	// Z轴补偿
	last_zoffset = probe.offset.z;
	dwin_var_set(probe.offset.z * 100, ZOffsetVar);
}

// 文件页面的参数，图标等等初始化
void file_page_data_init(void)
{
	// 文件背景图标初始化
	LOOP_L_N(i, SUM_FILES){dwin_var_set(0,ShowChosenFileIcon+i);}
	// 所有文件显示初始化
	LOOP_L_N(j, SUM_FILES)
	{
		LOOP_L_N(i, TEXT_VIEW_LEN)
		{
			dwin_var_set(0, FilenamesText + j*TEXT_VIEW_LEN + i);
		}
	}
}

// 温度页面的参数初始化
void temp_page_data_init(void)
{
	// temperature , 温度
	dwin_icon_set(thermalManager.temp_hotend[0].target , Nozzle0PreheatVar);
	dwin_icon_set(thermalManager.temp_hotend[1].target, Nozzle1PreheatVar);  
	dwin_icon_set(thermalManager.temp_bed.target, BedPreheatVar);
	
	dwin_icon_set(thermalManager.temp_hotend[0].celsius, Nozzle0TempVar);
	dwin_icon_set(thermalManager.temp_hotend[1].celsius, Nozzle1TempVar);
	dwin_icon_set(thermalManager.temp_bed.celsius, BedTempVar);

	// turn off fans，风扇
	//thermalManager.set_fan_speed(0, FanOff);
	//dwin_icon_set(0, FanKeyIcon);
}

// 自动化页面的参数，图标等等初始化
void autolevel_page_data_init(void)
{
	bool zig = true;
	int8_t inStart, inStop, inInc, showcount;
	showcount = 0;
	
	for(int y = 0;y < GRID_MAX_POINTS_Y;y++)
	{
		// away from origin
		if (zig) 
		{
			inStart = 0;
			inStop = GRID_MAX_POINTS_X;
			inInc = 1;
		}
		else // towards origin
		{
			inStart = GRID_MAX_POINTS_X - 1;
			inStop = -1;
			inInc = -1;
		}
		zig ^= true;
		for(int x = inStart;x != inStop; x += inInc)
		{
			dwin_var_set(z_values[x][y]*100, AutolevelVaR + showcount*2);
			showcount++;
		}
	}        
	queue.enqueue_now_P(PSTR("M420 S1"));

	last_zoffset = probe.offset.z;
	dwin_var_set(probe.offset.z * 100, ZOffsetVar);

	dwin_icon_set(thermalManager.temp_bed.celsius, BedTempVar);
	dwin_icon_set(thermalManager.temp_bed.target, BedPreheatVar);
}

// 控制页面的参数，图标等等初始化
void movecontrol_page_data_init(void)
{
	if(X_ENABLE_READ() == X_ENABLE_ON || Y_ENABLE_READ() == Y_ENABLE_ON || \
		Z_ENABLE_READ() == Z_ENABLE_ON || E0_ENABLE_READ() == E_ENABLE_ON)
	{
		queue.enqueue_now_P(PSTR("M84"));
		dwin_icon_set(0, MotorIcon); 
		Motor_Disable_Status_Set(false);
	}
	else
	{
		queue.enqueue_now_P(PSTR("M17"));
		dwin_icon_set(1, MotorIcon); 
		Motor_Disable_Status_Set(true);
	}
			
	dwin_var_set(10*current_position[X_AXIS], DisplayXaxisVar);
	dwin_var_set(10*current_position[Y_AXIS], DisplayYaxisVar);
	dwin_var_set(10*current_position[Z_AXIS], DisplayZaxisVar);
	Axis_MoveUnit_set(10);
	
	dwin_var_set(10*FilamentUnitbuf[0], FilementUnit0);

	temp_page_data_init();
}

// 设置页面的参数，图标等等初始化
void setting_page_data_init(void)
{
	// Printer information
	LOOP_L_N(i, 10){dwin_var_set(0,MacVersionText+i);}
	LOOP_L_N(i, 20){dwin_var_set(0,SoftVersionText+i);}
	LOOP_L_N(i, 20){dwin_var_set(0,ScreenVerText+i);}
	LOOP_L_N(i, 20){dwin_var_set(0,PrinterSizeText+i);}
	LOOP_L_N(i, 40){dwin_var_set(0,CorpWebsiteText+i);}
	
	char sizebuf[20]={0};
	sprintf(sizebuf,"%d X %d X %d", X_BED_SIZE, Y_BED_SIZE, Z_MAX_POS);
	dwin_str_set(STRING_CONFIG_H_AUTHOR, MacVersionText);
	//delay(2);
	dwin_str_set(SHORT_BUILD_VERSION, SoftVersionText);
	//delay(2);
	dwin_str_set(SCREEN_VERSION, ScreenVerText);
	dwin_str_set(sizebuf, PrinterSizeText);
	dwin_str_set(WEBSITE_URL, CorpWebsiteText);

	//SERIAL_ECHOLNPAIR(" Language =  ", Language);
	BL24CXX_Read(FONT_EEPROM, (uint8_t*)&Language, sizeof(Language));
	if ((Language != 1)&&(Language != 2))
	{
		Language = 1;
	}
	
	dwin_var_set(Language, LanguageIcon);
}

void error_page_data_init(void)
{
	LOOP_L_N(i, 30/2){dwin_var_set(0,ErrorText+i);}
}

#if ENABLED(TEST_MODE)
void test_page_data_init(void)
{
	dwin_var_set(planner.settings.axis_steps_per_mm[X_AXIS] * 100, XGearRatioVar);
	dwin_var_set(planner.settings.axis_steps_per_mm[Y_AXIS] * 100, YGearRatioVar);
	dwin_var_set(planner.settings.axis_steps_per_mm[Z_AXIS] * 10, ZGearRatioVar);
	dwin_var_set(planner.settings.axis_steps_per_mm[E_AXIS] * 10, EGearRatioVar);

	dwin_var_set(planner.settings.max_feedrate_mm_s[X_AXIS], XMaxSpeedVar);
	dwin_var_set(planner.settings.max_feedrate_mm_s[Y_AXIS], YMaxSpeedVar);
	dwin_var_set(planner.settings.max_feedrate_mm_s[Z_AXIS], ZMaxSpeedVar);
	dwin_var_set(planner.settings.max_feedrate_mm_s[E_AXIS], EMaxSpeedVar);

	dwin_var_set(planner.settings.max_acceleration_mm_per_s2[X_AXIS], XMaxAccelerationVar);
	dwin_var_set(planner.settings.max_acceleration_mm_per_s2[Y_AXIS], YMaxAccelerationVar);
	dwin_var_set(planner.settings.max_acceleration_mm_per_s2[Z_AXIS], ZMaxAccelerationVar);
	dwin_var_set(planner.settings.max_acceleration_mm_per_s2[E_AXIS], EMaxAccelerationVar);

	dwin_var_set(planner.max_jerk.x * 10, XMaxJerkAccVar);
	dwin_var_set(planner.max_jerk.y * 10, YMaxJerkAccVar);
	dwin_var_set(planner.max_jerk.z * 10, ZMaxJerkAccVar);
	dwin_var_set(planner.max_jerk.e * 10, EMaxJerkAccVar);
}
#endif

// 页面的参数，图标等等初始化
void All_Page_init(void)
{
	//****** main page init***************
	main_page_data_init();

	//****** printing page init***************
	printing_page_data_init();
	
	//****** file page init******************
	file_page_data_init();
	
	//****** temp page init******************
	//temp_page_data_init();

	//****** autolevel page init**************
	autolevel_page_data_init();
	
	//****** movecontrol page init*************
	movecontrol_page_data_init();
	
	//****** setting page init******************
	setting_page_data_init();

	//****** error page init**************
	error_page_data_init();
	
#if ENABLED(TEST_MODE)
	//****** test page init**************
	test_page_data_init();
#endif

	//****** Language title init******************
	Language_Update();

}

// 1. 判断是否有断电续打的情况； 
// 2.跳转到“断电续打”的界面，进行相关显示； 
void Power_Loss_Recovery(void)
{
	SDCard_Upate();
	if(recovery.valid() && SDCard_Inserted())
	{
		for (uint16_t i = 0; i < CardRecbuf.Filesum ; i++)
		{
			if(!strcmp(CardRecbuf.Cardfilename[i], &recovery.info.sd_filename[1])) // Resume print before power failure while have the same file
			{
				int filelen = strlen(CardRecbuf.Cardshowfilename[i]);
				char buf[FileNameCharLen];
				memset(buf,0,sizeof(buf));
				memcpy(buf, CardRecbuf.Cardlongfilename[i], 2*filelen);
				SERIAL_ECHOLNPAIR(" CardRecbuf.Cardlongfilename[i] =  ", CardRecbuf.Cardlongfilename[i]);
				SERIAL_ECHOLNPAIR(" filelen =  ", filelen);
				// strcpy(buf, CardRecbuf.Cardshowfilename[i]);
				
				// filelen = (TEXT_VIEW_LEN - filelen)/2;	
				// if(filelen > 0) // 让文件名在中间显示
				// {
				// 	memset(buf,0,sizeof(buf));
				// 	strncpy(buf,"                    ", filelen);
				// 	strcpy(&buf[filelen], CardRecbuf.Cardlongfilename[i]);
					
				// }
				dwin_unicode_set(buf,PrintingFileText,2*filelen);
				// dwin_str_set(buf, PrintingFileText);
				dwin_page_set(DwinPageInf[PPowerloss].pagenum[0]);
				break;
			}
		}
	}

}

//********************************************************************//
// 打印停止
void printing_stop(void)
{
	planner.synchronize();
	// card.endFilePrint();
	card.flag.abort_sd_printing = true;
	queue.clear();
	quickstop_stepper();
	print_job_timer.stop();
	#if DISABLED(SD_ABORT_NO_COOLDOWN)
	thermalManager.disable_all_heaters();
	#endif
	thermalManager.zero_fan_speeds();

	wait_for_heatup = false;
	#if ENABLED(SDSUPPORT) && ENABLED(POWER_LOSS_RECOVERY)
	  card.removeJobRecoveryFile();
	#endif
	#ifdef EVENT_GCODE_SD_ABORT
	  queue.inject_P(PSTR(EVENT_GCODE_SD_ABORT));        
	#endif

	dwin_icon_set(0,TimehourVar);
	dwin_icon_set(0,TimeminVar);
	dwin_icon_set(0,PrintscheduleIcon);
	dwin_icon_set(0,PercentageVar);
	dwin_icon_set(0, ShowChosenFileIcon); 
	LOOP_L_N(i, TEXT_VIEW_LEN){dwin_var_set(0,PrintingFileText+i);}
	SDCard_Out_ToMainPage_Set(false);
}

// 打印暂停
float pause_z = 0, pause_e = 0;
int temphot = 0;
int temphot2 = 0;
void printing_pause(void)
{
	dwin_page_set(DwinPageInf[PPopUp].pagenum[2]);
	pause_z = current_position[Z_AXIS];
	pause_e = current_position[E_AXIS] - 2;
	
	#if ENABLED(POWER_LOSS_RECOVERY)
	if (recovery.enabled) recovery.save(true, false);
	#endif
	card.pauseSDPrint();
	print_job_timer.pause();
	if(!temphot)
		temphot = thermalManager.degTargetHotend(0);
	if(!temphot2)
		temphot2 = thermalManager.degTargetHotend(1);
	thermalManager.setTargetHotend(0, 0);
	thermalManager.setTargetHotend(0, 1);

	//dwin_page_set(dwin_get_currentpage()+2);
	Mode_After_Home_set(PauseHome);
	//dwin_page_set(DwinPageInf[PPopUp].pagenum[2]);
	pause_action_flag = true;
}

// 缺料暂停
void no_filament_printing_pause(void)
{
	dwin_page_set(DwinPageInf[PPopUp].pagenum[2]);
	pause_z = current_position[Z_AXIS];
	pause_e = current_position[E_AXIS] - 2;
	
	#if ENABLED(POWER_LOSS_RECOVERY)
	if (recovery.enabled) recovery.save(true, false);
	#endif
	card.pauseSDPrint();
	print_job_timer.pause();
	if(!temphot)
		temphot = thermalManager.degTargetHotend(0);
	if(!temphot2)
		temphot2 = thermalManager.degTargetHotend(1);
	thermalManager.setTargetHotend(0, 0);
	thermalManager.setTargetHotend(0, 1);

	//dwin_page_set(dwin_get_currentpage()+2);
	Mode_After_Home_set(NoFilamentHome);
	//dwin_page_set(DwinPageInf[PPopUp].pagenum[2]);
	pause_action_flag = true;
}

// 继续打印
void printing_resume(void)
{
	char pause_str_Z[16];
	char pause_str_E[16];
	char commandbuf[30];
	memset(pause_str_Z, 0, sizeof(pause_str_Z));
	dtostrf(pause_z, 3, 2, pause_str_Z);
	memset(pause_str_E, 0, sizeof(pause_str_E));
	dtostrf(pause_e, 3, 2, pause_str_E);

	memset(commandbuf,0,sizeof(commandbuf));

	//queue.enqueue_one_now(commandbuf);
	//memset(commandbuf,0,sizeof(commandbuf));
	sprintf_P(commandbuf, PSTR("M109 T0 S%i"), temphot);
	queue.enqueue_one_now(commandbuf);
	memset(commandbuf,0,sizeof(commandbuf));
	sprintf_P(commandbuf, PSTR("M109 T1 S%i"), temphot2);
	queue.enqueue_one_now(commandbuf);
	memset(commandbuf,0,sizeof(commandbuf));
	sprintf_P(commandbuf, PSTR("G0 Z%s"), pause_str_Z);
	queue.enqueue_one_now(commandbuf);
	sprintf_P(commandbuf, PSTR("G92 E%s"), pause_str_E);
	queue.enqueue_one_now(commandbuf);
	temphot = 0;
	temphot2 = 0;

	card.startFileprint();
	print_job_timer.start();

	// Set_Light(green, ON);
	SDCard_Out_ToMainPage_Set(true);
}

// 文件显示
void fileshow_handle(void)
{
	char cmd[50];
	//char* c;
	sprintf_P(cmd, PSTR("M23 %s"), CardRecbuf.Cardfilename[CardRecbuf.recordcount]);
	//for (c = &cmd[4]; *c; c++) *c = tolower(*c);

	queue.enqueue_one_now(cmd);
	queue.enqueue_now_P(PSTR("M24"));
	
	// clean show the printing filename buf.
	for(int j = 0;j < TEXT_VIEW_LEN;j++)
	{
	  dwin_var_set(0,PrintingFileText+j);
	}
	dwin_unicode_set(CardRecbuf.Cardlongfilename[CardRecbuf.recordcount],PrintingFileText,2*strlen(CardRecbuf.Cardshowfilename[CardRecbuf.recordcount]));
	// dwin_str_set(CardRecbuf.Cardshowfilename[CardRecbuf.recordcount], PrintingFileText);
}

// 温度显示
void temperature_all_show(void)
{	
	thermalManager.setTargetHotend(thermalManager.temp_hotend[0].target , 0);
	thermalManager.setTargetHotend(thermalManager.temp_hotend[1].target, 1);
	thermalManager.setTargetBed(thermalManager.temp_bed.target);
	
	dwin_var_set(thermalManager.temp_hotend[0].target ,Nozzle0PreheatVar);
	dwin_var_set(thermalManager.temp_hotend[1].target ,Nozzle1PreheatVar);
	dwin_var_set(thermalManager.temp_bed.target ,BedPreheatVar);
}

// PLA参数设置
int16_t PLA_last_temp[3] = {PREHEAT_1_TEMP_HOTEND, PREHEAT_1_TEMP_HOTEND, PREHEAT_1_TEMP_BED};
void PLA_last_data_set(int16_t nozzle0, int16_t nozzle1, int16_t bed)
{
	PLA_last_temp[0] = nozzle0;
	PLA_last_temp[1] = nozzle1;
	PLA_last_temp[2] = bed;
}

// PLA参数显示
void PLA_data_show(void)
{
	dwin_var_set(PLA_last_temp[0] ,PLA_ABS_Nzll0TVar);
	dwin_var_set(PLA_last_temp[1] ,PLA_ABS_Nzll1TVar);
	dwin_var_set(PLA_last_temp[2] ,PLA_ABS_BedTVar);
}

// PLA模式执行
void PLA_execute(void)
{
	thermalManager.temp_hotend[0].target = PLA_last_temp[0];
	thermalManager.temp_hotend[1].target = PLA_last_temp[1];
	thermalManager.temp_bed.target = PLA_last_temp[2];

	temperature_all_show();
	dwin_page_set(DwinPageInf[PTempControl].pagenum[0]);
}

// ABS参数设置
int16_t ABS_last_temp[3] = {PREHEAT_2_TEMP_HOTEND, PREHEAT_2_TEMP_HOTEND, PREHEAT_2_TEMP_BED};
void ABS_last_data_set(int16_t nozzle0, int16_t nozzle1, int16_t bed)
{
	ABS_last_temp[0] = nozzle0;
	ABS_last_temp[1] = nozzle1;
	ABS_last_temp[2] = bed;
}

// ABS参数显示
void ABS_data_show(void)
{
	dwin_var_set(ABS_last_temp[0] ,PLA_ABS_Nzll0TVar);
	dwin_var_set(ABS_last_temp[1] ,PLA_ABS_Nzll1TVar);
	dwin_var_set(ABS_last_temp[2] ,PLA_ABS_BedTVar);
}

// ABS模式执行
void ABS_execute(void)
{
	thermalManager.temp_hotend[0].target = ABS_last_temp[0];
	thermalManager.temp_hotend[1].target = ABS_last_temp[1];
	thermalManager.temp_bed.target = ABS_last_temp[2];

	temperature_all_show();
	dwin_page_set(DwinPageInf[PTempControl].pagenum[0]);
}

// 幂函数运算
long pow(int num, unsigned int time)
{
	if(!num) return -1;
	if(!time) return 1;
	
	long result_val = 1;
	LOOP_L_N(i, time){ result_val = result_val*num;}
	
	return result_val;
}

// 一键功能打印
void Key_to_print(void)
{
	if(SDCard_Detected())
	{
		CardRecbuf.recordcount = 0;
		fileshow_handle();

		Checkfilament_InPrinting_Set(true);
		SDCard_Out_ToMainPage_Set(true);
		
		#ifdef HAS_FAN
		thermalManager.set_fan_speed(0, FanOn);
		#endif
		dwin_icon_set(1, FanKeyIcon);
		dwin_var_set(feedrate_percentage=100, FeedrateVar);
		
		dwin_page_set(DwinPageInf[PPrinting].pagenum[0]);
		
		// Set_Light(green, ON);
	}
}

// 主页面处理
void Main_page(void)
{
	unsigned long addr = dwinlcd.recdat.addr;
	unsigned short key_value = dwinlcd.recdat.data[0];
	SERIAL_ECHOLNPAIR(" addr =  ", addr);
	SERIAL_ECHOLNPAIR(" key_value =  ", key_value);
	if(addr == Nozzle0PreheatVar)
	{
		thermalManager.setTargetHotend(thermalManager.temp_hotend[0].target = key_value , 0);
		return;
	}
	else if(addr == Nozzle1PreheatVar)
	{
		thermalManager.setTargetHotend(thermalManager.temp_hotend[1].target = key_value , 1);
		return;
	}
	else if(addr == BedPreheatVar)
	{
		thermalManager.setTargetBed(thermalManager.temp_bed.target=key_value);
		return;
	}

	// 过滤非按键的命令
	if(addr !=MainPageKey) return;
	
	switch(key_value)
	{
		case key_zero:
			break;
			
		case key_one: // file
			CardRecbuf.recordcount = -1;
			SDCard_Upate();
			SDCard_File_Refresh();
			dwin_page_set(DwinPageInf[PFile].pagenum[0]);
			break;
			
		case key_two: // key to print
			Key_to_print();
			break;
			
		case key_three: // temperature
			dwin_page_set(DwinPageInf[PTempControl].pagenum[0]);
			break;
			
		case key_four:// Assitant Level
			queue.enqueue_now_P(PSTR("G28"));
			Mode_After_Home_set(AuxLevelHome);
			dwin_page_set(DwinPageInf[PPopUp].pagenum[2]);
			break;
			
		case key_five:// Auto Level
			autolevel_page_data_init();
			dwin_var_set(probe.offset.z*100, ZOffsetVar);
			dwin_page_set(DwinPageInf[PAutobedlevel].pagenum[0]);
			break;
			
		case key_six: // move control
			movecontrol_page_data_init();
			dwin_page_set(DwinPageInf[PMoveControl].pagenum[0]);
			break;
			
		case key_seven:// motor control, on or off
			if(Motor_Disable_Status())
			{
				queue.enqueue_now_P(PSTR("M84"));
				dwin_icon_set(0, MotorIcon); 
				Motor_Disable_Status_Set(false);
			}
			else
			{
				queue.enqueue_now_P(PSTR("M17"));
				dwin_icon_set(1, MotorIcon); 
				Motor_Disable_Status_Set(true);
			}
			break;
			
		case key_eight: //emergency stop
			if(Emergency_Status()) 
			{
				// delay(1500);
				dwin_icon_set(0, EmergencyIcon); 
				Emergency_Status_Set(false);
			}
			else// emergency status
			{
				dwin_icon_set(1, EmergencyIcon); 
				quickstop_stepper();
				queue.enqueue_now_P(PSTR("M84"));
				thermalManager.disable_all_heaters();
				Emergency_Status_Set(true);
			}
			break;
			
		case key_night://Information
			setting_page_data_init();
			dwin_page_set(DwinPageInf[PSetting].pagenum[0]);
			break;
			
	}
}

// 打印页面处理
void Printing_page(void)
{
	unsigned long addr = dwinlcd.recdat.addr;
	unsigned short key_value = dwinlcd.recdat.data[0];
	
	SERIAL_ECHOLNPAIR(" addr =  ", addr);
	
	if(addr == Nozzle0PreheatVar)
	{
          	thermalManager.setTargetHotend(thermalManager.temp_hotend[0].target = key_value , 0);
		return;
	}
	else if(addr == Nozzle1PreheatVar)
	{
		thermalManager.setTargetHotend(thermalManager.temp_hotend[1].target = key_value , 1);
		return;
	}
	else if(addr == BedPreheatVar)
	{
		thermalManager.setTargetBed(thermalManager.temp_bed.target=key_value);
		return;
	}
	else if(addr == ZOffsetVar)
	{
		last_zoffset = probe.offset.z;
		probe.offset.z = (key_value >= (0xFFFF-Z_PROBE_OFFSET_RANGE_MAX*100+1))?(key_value-0xFFFF-1):key_value;
		probe.offset.z /= 100;
		SERIAL_ECHOLNPAIR(" probe.offset.z =  ", probe.offset.z);
		if(WITHIN(probe.offset.z , Z_PROBE_OFFSET_RANGE_MIN, Z_PROBE_OFFSET_RANGE_MAX))
		{
			babystep.add_mm(Z_AXIS, probe.offset.z - last_zoffset);
			dwin_var_set(probe.offset.z * 100, ZOffsetVar);  
			// settings.save();
		}
		else
		{
			probe.offset.z = last_zoffset ;
		}
		return;
	}
	else if(addr == FeedrateVar)
	{
		feedrate_percentage = key_value;
		return;
	}
	else if(addr == FlowPercentageVar)
	{
	 	planner.flow_percentage[0] = key_value;
	 	planner.refresh_e_factor(0);
		return;
	}


	// 过滤非按键的命令
	if(addr !=PrintingPageKey) return;
	
	SERIAL_ECHOLNPAIR(" key_value =  ", key_value);
	switch(key_value)
	{
		case key_zero:  //  finish printing
		//	card.flag.abort_sd_printing = true;  //hmxtest
			quickstop_stepper();
			print_job_timer.stop();
			dwin_icon_set(1, MotorIcon); 
			dwin_icon_set(0,PrintscheduleIcon);
			//dwin_icon_set(0,PrintscheduleIcon+1);
			dwin_icon_set(0,PercentageVar);
			//delay(2);
			dwin_var_set(0,TimehourVar);
			dwin_var_set(0,TimeminVar);
			print_job_timer.reset();
			dwin_page_set(DwinPageInf[PMain].pagenum[0]);
			break;

		case key_one: // stop
			Other_Mode_set(Stop_Mode);
			dwin_icon_set(Language, PopUp1Title); 
			record_page = dwin_get_currentpage();
			dwin_page_set(DwinPageInf[PPopUp].pagenum[0]);
			break;
			
		case key_two: // pause
			Other_Mode_set(Pause_Mode);
			dwin_icon_set(Language+2, PopUp1Title); 
			record_page = dwin_get_currentpage();
			dwin_page_set(DwinPageInf[PPopUp].pagenum[0]);
			break;
			
		case key_three: // resume
			printing_resume();
			dwin_page_set(dwin_get_currentpage()-2);
			break;
			
		case key_four: // cancel
			dwin_page_set(dwin_get_currentpage());
			break;
			
		case key_five: // ajust to expand the list
		case key_seven:
			dwin_var_set(probe.offset.z*100, ZOffsetVar);
			dwin_var_set(planner.flow_percentage[0], FlowPercentageVar);
			dwin_var_set(feedrate_percentage, FeedrateVar);
			#ifdef HAS_FAN
			thermalManager.fan_speed[0] ? dwin_icon_set(1, FanKeyIcon):dwin_icon_set(0, FanKeyIcon);
			#endif
			Add_Filaments()?dwin_icon_set(1, AddFilamentIcon):dwin_icon_set(0, AddFilamentIcon);
			
			dwin_page_set(DwinPageInf[PPrinting].pagenum[key_value-4]);
			break;
			
		case key_six:// ajust to close the list
		case key_eight:
			dwin_page_set(DwinPageInf[PPrinting].pagenum[key_value-6]);
			break;
						
		case key_night: // add filaments
			if(Add_Filaments()) // close
			{
		    	dwin_icon_set(0, AddFilamentIcon);
				WRITE(Add_FILAMENT_PIN, LOW);
				Add_Filaments_Set(false);
				Auto_Close_Flag = 0;
				
			}
			else	// open
			{
		        dwin_icon_set(1, AddFilamentIcon);
				WRITE(Add_FILAMENT_PIN, HIGH);
				Add_Filaments_Set(true);
				Auto_Close_Flag = 1;
				Filament_time = millis();
			}
			break;
			
		case key_ten: // nozzle fans
		#ifdef HAS_FAN
			if(thermalManager.fan_speed[0])// turn off the fan
			{
				dwin_icon_set(0, FanKeyIcon); 
				thermalManager.set_fan_speed(0, FanOff);
			}
			else// turn on the fan
			{
				dwin_icon_set(1, FanKeyIcon);
				thermalManager.set_fan_speed(0, FanOn);
			}
		#endif
			break;
			
		case key_eleven: // Zaxis offset -
			last_zoffset = probe.offset.z;
			if (WITHIN((last_zoffset - 0.05), Z_PROBE_OFFSET_RANGE_MIN, Z_PROBE_OFFSET_RANGE_MAX)) 
			{
				probe.offset.z = (last_zoffset - 0.05);
				babystep.add_mm(Z_AXIS, probe.offset.z - last_zoffset);
				dwin_var_set(probe.offset.z*100, ZOffsetVar);
				// settings.save();//caixiaoliang add 20201102
			}
			break;
			
		case key_twelve:// Zaxis offset +
			last_zoffset = probe.offset.z;
			if (WITHIN((last_zoffset + 0.05), Z_PROBE_OFFSET_RANGE_MIN, Z_PROBE_OFFSET_RANGE_MAX)) 
			{
				probe.offset.z = (last_zoffset + 0.05);                
				babystep.add_mm(Z_AXIS, probe.offset.z - last_zoffset);  
				dwin_var_set(probe.offset.z*100, ZOffsetVar);
				// settings.save();
			}
			break;

 	}

}

// 文件页面处理
void File_page(void)
{
	unsigned long addr = dwinlcd.recdat.addr;
	unsigned short key_value = dwinlcd.recdat.data[0];
	SERIAL_ECHOLNPAIR(" addr =  ", addr);
	SERIAL_ECHOLNPAIR(" key_value =  ", key_value);

	if(addr == ChooseFileKey)
	{
		if(SDCard_Detected())
		{
			if(key_value >= CardRecbuf.Filesum) return;
			CardRecbuf.recordcount = key_value;
			// clean file background icon
			LOOP_L_N(i, CardRecbuf.Filesum){dwin_icon_set(0, ShowChosenFileIcon+i);} 
			dwin_icon_set(1, ShowChosenFileIcon+key_value);
		}
		return;
	}

	// 过滤非按键的命令
	if(addr !=FilePageKey) return;
	
	switch(key_value)
	{
		case key_zero: // return to main page
			dwin_page_set(DwinPageInf[PMain].pagenum[0]);
			break;
			
		case key_one:// start to print the chosen file
			if(SDCard_Detected())
			{
				 if(CardRecbuf.recordcount < 0) break; // don't choose file

				#if CHECKFILEMENT
				  /**************checking filament status during printing beginning ************/
				 // if(CheckFilement(1)) break; Jie
				#endif
				
				fileshow_handle();

				Checkfilament_InPrinting_Set(true);
				SDCard_Out_ToMainPage_Set(true);
				#ifdef HAS_FAN
				thermalManager.set_fan_speed(0, FanOn);
				#endif
				dwin_icon_set(1, FanKeyIcon);
				dwin_var_set(feedrate_percentage=100, FeedrateVar);
				
				dwin_page_set(DwinPageInf[PPrinting].pagenum[0]);
				
				// Set_Light(green, ON);
			}
			break;
			
		case key_two:// next page
		case key_four:
		case key_six:
			dwin_page_set(DwinPageInf[PFile].pagenum[key_value/2]);
			break;
			
		case key_three:// last page
		case key_five:
		case key_seven:
			dwin_page_set(DwinPageInf[PFile].pagenum[(key_value-3)/2]);
			break;
	}
}

// 温度页面处理	
void Temp_page(int pagenum)
{
	unsigned long addr = dwinlcd.recdat.addr;
	unsigned short key_value = dwinlcd.recdat.data[0];
	if(pagenum == DwinPageInf[PTempControl].pagenum[0])
	{
		if(addr == Nozzle0PreheatVar)
		{
	          	thermalManager.setTargetHotend(thermalManager.temp_hotend[0].target = key_value , 0);
		}
		else if(addr == Nozzle1PreheatVar)
		{
			thermalManager.setTargetHotend(thermalManager.temp_hotend[1].target = key_value , 1);
		}
		else if(addr == BedPreheatVar)
		{
			thermalManager.setTargetBed(thermalManager.temp_bed.target=key_value);
		}
		// return;
	}
	else if(pagenum == DwinPageInf[PTempControl].pagenum[2]) // PLA page
	{
		if(addr == PLA_ABS_Nzll0TVar) PLA_last_temp[0] = key_value;
		else if(addr == PLA_ABS_Nzll1TVar) PLA_last_temp[1] = key_value;
		else if(addr == PLA_ABS_BedTVar)PLA_last_temp[2] = key_value;
		PLA_last_data_set(PLA_last_temp[0], PLA_last_temp[1], PLA_last_temp[2]);
		// return;
	}
	else if(pagenum == DwinPageInf[PTempControl].pagenum[3]) // ABS page
	{
		if(addr == PLA_ABS_Nzll0TVar) ABS_last_temp[0] = key_value;
		else if(addr == PLA_ABS_Nzll1TVar) ABS_last_temp[1] = key_value;
		else if(addr == PLA_ABS_BedTVar) ABS_last_temp[2] = key_value;
		ABS_last_data_set(ABS_last_temp[0], ABS_last_temp[1], ABS_last_temp[2]);
		// return;
	}

	// 过滤非按键的命令
	if(addr !=TempPageKey) return;
		
	switch(key_value)
	{
		case key_zero:// return to main page
			dwin_page_set(DwinPageInf[PMain].pagenum[0]);
			break;
			
		case key_one:// key to cool for nozzle 0
			thermalManager.setTargetHotend(thermalManager.temp_hotend[0].target = 0 , 0);
			dwin_var_set(0,Nozzle0PreheatVar);
			#ifdef HAS_FAN
			thermalManager.set_fan_speed(0, FanOn);
			dwin_icon_set(1, FanKeyIcon);
			#endif
			break;
			
		case key_two:// key to cool for nozzle 1
			thermalManager.setTargetHotend(thermalManager.temp_hotend[1].target = 0 , 1);
			dwin_var_set(0,Nozzle1PreheatVar);
			#ifdef HAS_FAN
			thermalManager.set_fan_speed(0, FanOn);
			dwin_icon_set(1, FanKeyIcon);
			#endif
			break;
			
		case key_three:// key to cool for bed
			thermalManager.setTargetBed(thermalManager.temp_bed.target=0);
			dwin_var_set(0,BedPreheatVar);
			break;
			
		case key_four:// key to cool for all
			thermalManager.setTargetHotend(thermalManager.temp_hotend[0].target = 0 , 0);
			thermalManager.setTargetHotend(thermalManager.temp_hotend[1].target = 0 , 1);
			thermalManager.setTargetBed(thermalManager.temp_bed.target=0);
			dwin_var_set(0,Nozzle0PreheatVar);
			dwin_var_set(0,Nozzle1PreheatVar);
			dwin_var_set(0,BedPreheatVar);
			#ifdef HAS_FAN
			thermalManager.set_fan_speed(0, FanOn);
			dwin_icon_set(1, FanKeyIcon);
			#endif
			break;
			
		case key_five:// fans switch
			#ifdef HAS_FAN
			if(thermalManager.fan_speed[0])// turn off the fan
			{
				dwin_icon_set(0, FanKeyIcon); 
				thermalManager.set_fan_speed(0, FanOff);
			}
			else// turn on the fan
			{
				dwin_icon_set(1, FanKeyIcon);
				thermalManager.set_fan_speed(0, FanOn);
			}
			#endif
			break;
			
		case key_six: // PLA
			PLA_data_show();
			dwin_page_set(DwinPageInf[PTempControl].pagenum[2]);
			break;
			
		case key_seven:// ABS
			ABS_data_show();
			dwin_page_set(DwinPageInf[PTempControl].pagenum[3]);
			break;
			
		case key_eight:// yes of PLA
			PLA_execute();
			break;
			
		case key_night:// no of PLA
			temperature_all_show();
			dwin_page_set(DwinPageInf[PTempControl].pagenum[0]);
			break;
			
		case key_ten:// yes of ABS
			ABS_execute();
			break;
			
		case key_eleven:// no of ABS
			temperature_all_show();
			dwin_page_set(DwinPageInf[PTempControl].pagenum[0]);
			break;
	}

}

// 辅助调平页面处理
void Assitantlevel_page(void)
{
	unsigned long addr = dwinlcd.recdat.addr;
	unsigned short key_value = dwinlcd.recdat.data[0];

	// 过滤非按键的命令
	if(addr !=AuxLevelPageKey) return;
		
	switch(key_value)
	{
		case key_zero:// return to main page
			dwin_page_set(DwinPageInf[PMain].pagenum[0]);
			break;
			
		case key_one:// Assitant Level ,  Centre 1
			if (!planner.has_blocks_queued())
			{
				uint16_t xpos, ypos;
				char command[50];
				xpos = X_BED_SIZE / 2;
				ypos = Y_BED_SIZE / 2;

				queue.enqueue_now_P(PSTR("G1 F600 Z3")); 
				memset(command, 0, 50);
				sprintf(command, "%s%d%s%d%s", "G1 X", xpos, " Y", ypos, " F8000");
				queue.enqueue_now_P(command);
				queue.enqueue_now_P(PSTR("G1 F600 Z0"));
			}
			break;
			
		case key_two:// Assitant Level , Front Left 2
			if (!planner.has_blocks_queued())
			{
				queue.enqueue_now_P(PSTR("G1 F600 Z3")); 
				queue.enqueue_now_P(PSTR("G1 X50 Y50 F8000"));
				queue.enqueue_now_P(PSTR("G1 F600 Z0"));
			}
			break;
			
		case key_three:// Assitant Level , Front Right 3
			if (!planner.has_blocks_queued())
			{
				uint16_t xpos, ypos;
				char command[50];
				xpos = X_BED_SIZE - 50;
				ypos = 50;

				queue.enqueue_now_P(PSTR("G1 F600 Z3")); 
				memset(command, 0, 50);
				sprintf(command, "%s%d%s%d%s", "G1 X", xpos, " Y", ypos, " F8000");
				queue.enqueue_now_P(command);
				queue.enqueue_now_P(PSTR("G1 F600 Z0"));
			}
			break;
			
		case key_four:// Assitant Level , Back Right 4
			if (!planner.has_blocks_queued())
			{
				uint16_t xpos, ypos;
				char command[50];
				xpos = X_BED_SIZE - 50;
				ypos = Y_BED_SIZE - 100;
				
				queue.enqueue_now_P(PSTR("G1 F600 Z3")); 
				memset(command, 0, 50);
				sprintf(command, "%s%d%s%d%s", "G1 X", xpos, " Y", ypos, " F8000");
				queue.enqueue_now_P(command);
				queue.enqueue_now_P(PSTR("G1 F600 Z0"));
			}
			break;
			
		case key_five:// Assitant Level , Back Left 5
			if (!planner.has_blocks_queued())
			{
				uint16_t xpos, ypos;
				char command[50];
				xpos = 50;
				ypos = Y_BED_SIZE - 100;

				queue.enqueue_now_P(PSTR("G1 F600 Z3")); 
				memset(command, 0, 50);
				sprintf(command, "%s%d%s%d%s", "G1 X", xpos, " Y", ypos, " F8000");
				queue.enqueue_now_P(command);
				queue.enqueue_now_P(PSTR("G1 F600 Z0"));
			}
			break;
	}

}

// 自动调平页面处理
void Autobedlevel_page(void)
{
	unsigned long addr = dwinlcd.recdat.addr;
	unsigned short key_value = dwinlcd.recdat.data[0];
	if(addr == BedPreheatVar)
	{
		thermalManager.setTargetBed(thermalManager.temp_bed.target=key_value);
		return;
	}

	// 过滤非按键的命令
	if(addr !=AutoLevelPageKey) return;
		
	switch(key_value)
	{
		case key_zero:// return to main page
			if(last_zoffset != probe.offset.z)
			{
				last_zoffset = probe.offset.z;
				settings.save();
			}
			dwin_page_set(DwinPageInf[PMain].pagenum[0]);
			break;
			
		case key_one: // +0.1
			last_zoffset = probe.offset.z;
			if (WITHIN((last_zoffset + 0.1), Z_PROBE_OFFSET_RANGE_MIN, Z_PROBE_OFFSET_RANGE_MAX)) 
			{
				probe.offset.z = (last_zoffset + 0.1);                
				babystep.add_mm(Z_AXIS, probe.offset.z - last_zoffset);  
				dwin_var_set(probe.offset.z*100, ZOffsetVar);
				//settings.save();
			}
			break;
			
		case key_two: // -0.1
			last_zoffset = probe.offset.z;
			if (WITHIN((last_zoffset - 0.1), Z_PROBE_OFFSET_RANGE_MIN, Z_PROBE_OFFSET_RANGE_MAX)) 
			{
				probe.offset.z = (last_zoffset - 0.1);
				babystep.add_mm(Z_AXIS, probe.offset.z - last_zoffset);
				dwin_var_set(probe.offset.z*100, ZOffsetVar);
				//settings.save();
			}
			break;
			
		case key_three: // Z axis home
			// Z-axis to home
			if (!planner.has_blocks_queued())
			{
				planner.synchronize();
				if (!TEST(axis_known_position, X_AXIS) || !TEST(axis_known_position, Y_AXIS))
				{
					queue.enqueue_now_P(PSTR("G28"));
				}
				else
				{
					queue.enqueue_now_P(PSTR("G28 Z0"));
				}
				queue.enqueue_now_P(PSTR("G1 F200 Z0.0"));
				last_zoffset = probe.offset.z;
				probe.offset.z = 0;
				babystep.add_mm(Z_AXIS, probe.offset.z - last_zoffset);
				//settings.save();//caixiaoliang add 20201102
				dwin_var_set(0, ZOffsetVar);
				
				Mode_After_Home_set(AutoLevelHome);
				dwin_page_set(DwinPageInf[PPopUp].pagenum[2]);
			}
			break;
			
		case key_four: // AutoLevel measure
			queue.enqueue_now_P(PSTR("G28\nG29")); 
			dwin_page_set(DwinPageInf[PAutobedlevel].pagenum[1]);
			break;
	}

}

// 移动控制页面处理
void MoveControl_page(void)
{
	unsigned long addr = dwinlcd.recdat.addr;
	unsigned short key_value = dwinlcd.recdat.data[0];
	 
	if(addr == DisplayXaxisVar)
	{
		current_position[X_AXIS] = ((float)key_value)/10;
		if (current_position[X_AXIS] < X_MIN_POS) current_position[X_AXIS] = X_MIN_POS;
		else if (current_position[X_AXIS] > X_MAX_POS) current_position[X_AXIS] = X_MAX_POS;
		dwin_var_set(10*current_position[X_AXIS], DisplayXaxisVar);
		RTS_line_to_current(X_AXIS);
		return;
	}
	else if(addr == DisplayYaxisVar)
	{
		current_position[Y_AXIS] = ((float)key_value)/10;
		if (current_position[Y_AXIS] < Y_MIN_POS) current_position[Y_AXIS] = Y_MIN_POS;
		else if (current_position[Y_AXIS] > Y_MAX_POS) current_position[Y_AXIS] = Y_MAX_POS;
		dwin_var_set(10*current_position[Y_AXIS], DisplayYaxisVar);
		RTS_line_to_current(Y_AXIS);
		return;
	}
	else if(addr == DisplayZaxisVar)
	{
		current_position[Z_AXIS] = ((float)key_value)/10;
		if (current_position[Z_AXIS] < Z_MIN_POS) current_position[Z_AXIS] = Z_MIN_POS;
		else if (current_position[Z_AXIS] > Z_MAX_POS) current_position[Z_AXIS] = Z_MAX_POS;
		dwin_var_set(10*current_position[Z_AXIS], DisplayZaxisVar);
		RTS_line_to_current(Z_AXIS);
		return;
	}
	else if(addr == Nozzle0PreheatVar)
	{
          	thermalManager.setTargetHotend(thermalManager.temp_hotend[0].target = key_value , 0);
		return;
	}
	else if(addr == Nozzle1PreheatVar)
	{
		thermalManager.setTargetHotend(thermalManager.temp_hotend[1].target = key_value , 1);
		return;
	}
	else if(addr == FilementUnit0)
	{
		FilamentUnitbuf[0] = ((float)key_value)/10;
	}

	// 过滤非按键的命令
	if(addr !=ControlOtherkey && addr !=ControlMovekey) return;
		
	if(addr == ControlOtherkey)
	{
		switch(key_value)
		{
			case key_zero:// return to main page
				dwin_page_set(DwinPageInf[PMain].pagenum[0]);
				break;
				
			case key_one: // motor disable
				if(Motor_Disable_Status())
				{
					queue.enqueue_now_P(PSTR("M84"));
					dwin_icon_set(0, MotorIcon); 
					Motor_Disable_Status_Set(false);
				}
				else
				{
					queue.enqueue_now_P(PSTR("M17"));
					dwin_icon_set(1, MotorIcon); 
					Motor_Disable_Status_Set(true);
				}
				break;
				
			case key_two: // add filaments
				if(Add_Filaments()) // close
				{
			        dwin_icon_set(0, AddFilamentIcon);
					WRITE(Add_FILAMENT_PIN, LOW);
					Add_Filaments_Set(false);
					Auto_Close_Flag = 0;
					
				}
				else	// open
				{
			        dwin_icon_set(1, AddFilamentIcon);
					WRITE(Add_FILAMENT_PIN, HIGH);
					Add_Filaments_Set(true);
					Auto_Close_Flag = 1;
					Filament_time = millis();
				}
				break;
				
			case key_three:// Unload filament
			case key_four:// Load filament
				if(thermalManager.temp_hotend[0].celsius < (PREHEAT_1_TEMP_HOTEND -20) || \
					thermalManager.temp_hotend[1].celsius < (PREHEAT_1_TEMP_HOTEND -40))
				{
					if(thermalManager.temp_hotend[0].target < (PREHEAT_1_TEMP_HOTEND -20))
					{
						thermalManager.temp_hotend[0].target = PREHEAT_1_TEMP_HOTEND -20;
						thermalManager.setTargetHotend(thermalManager.temp_hotend[0].target, 0);
						dwin_var_set(thermalManager.temp_hotend[0].target ,Nozzle0PreheatVar);
					}
					if(thermalManager.temp_hotend[1].target < (PREHEAT_1_TEMP_HOTEND -40))
					{
						thermalManager.temp_hotend[1].target = PREHEAT_1_TEMP_HOTEND -40;
						thermalManager.setTargetHotend(thermalManager.temp_hotend[1].target, 1);
						dwin_var_set(thermalManager.temp_hotend[1].target ,Nozzle1PreheatVar);
					}
					dwin_page_set(DwinPageInf[PMoveControl].pagenum[1]);
					delay(1500);
					dwin_page_set(DwinPageInf[PMoveControl].pagenum[0]);
				}
				else if(key_value == key_three)// Unload filament
				{
					current_position[E_AXIS] -= FilamentUnitbuf[0];
					RTS_line_to_current(E_AXIS);
				}
				else if(key_value == key_four)// Load filament
				{
					current_position[E_AXIS] += FilamentUnitbuf[0];
					RTS_line_to_current(E_AXIS);
				}
				break;
		}
	}
	else if(addr == ControlMovekey && !planner.has_blocks_queued())
	{
		AxisEnum axis =X_AXIS;
		float min = 0, max = 0;
		switch(key_value)
		{
			case key_zero:// home
				queue.enqueue_now_P(PSTR("G28"));
				Mode_After_Home_set(MoveHome);
				dwin_page_set(DwinPageInf[PPopUp].pagenum[2]);
				break;
				
			case key_one: // X_AXIS 
			case key_two:
				axis = X_AXIS;
				min = X_MIN_POS;
				max = X_MAX_POS;
				current_position[axis]=(float)current_position[axis]+(key_value == 1?1:(-1))*Axis_MoveUnit_get();
				break;
				
			case key_three:// Y_AXIS 
			case key_four:
				axis = Y_AXIS;
				min = Y_MIN_POS;
				max = Y_MAX_POS;
				current_position[axis]=(float)current_position[axis]+(key_value == 3?1:(-1))*Axis_MoveUnit_get();
				break;
								

			case key_five:// Z_AXIS 
			case key_six:
				axis = Z_AXIS;
				min = Z_MIN_POS;
				max = Z_MAX_POS;
				current_position[axis]=(float)current_position[axis]+(key_value == 5?1:(-1))*Axis_MoveUnit_get();
				break;
				
			case key_seven:
				Axis_MoveUnit_set(10);
				break;
				
			case key_eight:
				Axis_MoveUnit_set(1);
				break;
				
			case key_night:
				Axis_MoveUnit_set(0.1);
				break;

		}
		
		if(key_value != key_zero && max)
		{
			if (current_position[axis] < min) current_position[axis] = min;
			else if (current_position[axis] > max) current_position[axis] = max;
			RTS_line_to_current(axis);
			dwin_var_set(10*current_position[X_AXIS], DisplayXaxisVar);
			dwin_var_set(10*current_position[Y_AXIS], DisplayYaxisVar);
			dwin_var_set(10*current_position[Z_AXIS], DisplayZaxisVar);
		}
		dwin_icon_set(1, MotorIcon);
	}
}

// 设置页面处理
void Setting_page(void)
{
	unsigned long addr = dwinlcd.recdat.addr;
	unsigned short key_value = dwinlcd.recdat.data[0];

	// 过滤非按键的命令
	if(addr !=SettingPageKey) return;
		
	switch(key_value)
	{
		case key_zero:// return to main page
			dwin_page_set(DwinPageInf[PMain].pagenum[0]);
			break;
			
		case key_one: // Language
			Other_Mode_set(Language_Mode);
			dwin_icon_set(Language, PopUp2Title); 
			dwin_page_set(DwinPageInf[PPopUp].pagenum[1]);
			break;
		case key_two://Restore Factory  
			Other_Mode_set(RestoreFactory_Mode);
			dwin_icon_set(Language+2, PopUp2Title); 
			dwin_page_set(DwinPageInf[PPopUp].pagenum[1]);
			break;
#if ENABLED(TEST_MODE)
		case key_three://debug param 
			test_page_data_init();
			if(Language == 1)
			{
				dwin_page_set(DwinPageInf[PTest].pagenum[0]);
			}
			else
			{
				dwin_page_set(DwinPageInf[PTest].pagenum[2]);
			}
			break;
#endif
	}

}

// 断料页面处理
void NoFilement_page(void)
{
	unsigned long addr = dwinlcd.recdat.addr;
	unsigned short key_value = dwinlcd.recdat.data[0];

	// 过滤非按键的命令
	if(addr !=NoFilamentPageKey) return;
		
	switch(key_value)
	{
		case key_zero:// return to main page
			//dwin_page_set(DwinPageInf[PMain].pagenum[0]);
			break;
			
		case key_one: // add filament
			if(Add_Filaments()) // close
			{
		          	dwin_icon_set(0, AddFilamentIcon);
				WRITE(Add_FILAMENT_PIN, LOW);
				Add_Filaments_Set(false);
				Auto_Close_Flag = 0;
				
			}
			else	// open
			{
		          	dwin_icon_set(1, AddFilamentIcon);
				WRITE(Add_FILAMENT_PIN, HIGH);
				Add_Filaments_Set(true);
				Auto_Close_Flag = 1;
				Filament_time = millis();
			}
			break;
			
		case key_two://resume print
			if(is_no_filament() == false)
			{
				dwin_icon_set(0, AddFilamentIcon);
				WRITE(Add_FILAMENT_PIN, LOW);
				printing_resume();
				dwin_page_set(DwinPageInf[PPrinting].pagenum[0]);
				Auto_Close_Flag = 0;

			}			
			break;
			
		case key_three: // stop
			printing_stop();
			Mode_After_Home_set(StopHome);
			dwin_page_set(DwinPageInf[PPopUp].pagenum[2]);
			break;

	}
}

// 弹窗页面处理
void PopUp_page(void)
{
	unsigned long addr = dwinlcd.recdat.addr;
	unsigned short key_value = dwinlcd.recdat.data[0];

	// 过滤非按键的命令
	if(addr !=PopUpKey) return;
		
	switch(key_value)
	{
		case key_zero:// return to main page
			dwin_page_set(DwinPageInf[PMain].pagenum[0]);
			break;
			
 		case key_one:// yes of stop or pause
			if(Other_Mode_get() == Stop_Mode) //stop
			{
				dwin_page_set(DwinPageInf[PPopUp].pagenum[2]);
				printing_stop();
				Mode_After_Home_set(StopHome);
				//dwin_page_set(DwinPageInf[PPopUp].pagenum[2]);
			}
			else if(Other_Mode_get() == Pause_Mode) //pause
			{
				printing_pause();
			}
			break;
			
 		case key_two:// no of stop or pause
			dwin_page_set(record_page);
			break;
			
		case key_three: // yes of Language or Restore Factory
		case key_four: // no of Language or Restore Factory
			if(Other_Mode_get() == Language_Mode) //Language
			{
				Language = key_value-2;
				BL24CXX_Write(FONT_EEPROM, (uint8_t*)&Language, sizeof(Language));
				Language_Update();
				dwin_icon_set(Language, LanguageIcon);
			}
			else if(Other_Mode_get() == RestoreFactory_Mode && (key_value == key_three)) //Restore Factory
			{
				settings.reset();
				settings.save();
			}
			dwin_page_set(DwinPageInf[PSetting].pagenum[0]);
			break;			
	}
}

// 断电续打页面处理
void PowerLoss_page(void)
{
	unsigned long addr = dwinlcd.recdat.addr;
	unsigned short key_value = dwinlcd.recdat.data[0];

	// 过滤非按键的命令
	if(addr !=PowerlossPageKey) return;
		
	switch(key_value)
	{
		case key_zero:// return to main page
			dwin_page_set(DwinPageInf[PMain].pagenum[0]);
			
			card.endFilePrint();
			queue.clear();
			quickstop_stepper();
			print_job_timer.stop();
			thermalManager.disable_all_heaters();
			print_job_timer.reset();

			#if ENABLED(SDSUPPORT) && ENABLED(POWER_LOSS_RECOVERY)
			card.removeJobRecoveryFile();
			recovery.info.valid_head = 0;
			recovery.info.valid_foot = 0;
			recovery.close();
			#endif

			wait_for_heatup = false;
			break;
			
		case key_one: // Yes:continue to print the 3Dmode during power-off.
			if(recovery.valid()) 
			{
				dwin_page_set(DwinPageInf[PPrinting].pagenum[0]);
				dwin_var_set(feedrate_percentage, FeedrateVar);
				recovery.resume();				
			}
			break;
	}
}

void Error_page(void)
{
	unsigned long addr = dwinlcd.recdat.addr;
	unsigned short key_value = dwinlcd.recdat.data[0];

	// 过滤非按键的命令
	if(addr !=ErrorPageKey) return;
		
	switch(key_value)
	{
		case key_zero: 
			// printing
			if(printingIsActive()) 
			{
				dwin_page_set(DwinPageInf[PPopUp].pagenum[2]);
				printing_stop();
				Mode_After_Home_set(StopHome);
			}
			else if(printingIsPaused()) dwin_page_set(DwinPageInf[PPrinting].pagenum[2]);
			else dwin_page_set(DwinPageInf[PMain].pagenum[0]);

			if(Error_Mode_get() == Error_Mode_204) NVIC_SystemReset();   // reboot
			break;
	}
}

#if ENABLED(TEST_MODE)
void Test_page(void)
{
	unsigned long addr = dwinlcd.recdat.addr;
	unsigned short key_value = dwinlcd.recdat.data[0];
	float val_buf = 0.0;
	
	if(addr == XGearRatioVar)
	{	
		val_buf = (float)key_value/100;
		planner.settings.axis_steps_per_mm[X_AXIS] = val_buf;
		dwin_var_set(val_buf*100, XGearRatioVar);
		return;
	}
	else if(addr == YGearRatioVar)
	{
		val_buf = (float)key_value/100;
		planner.settings.axis_steps_per_mm[Y_AXIS] = val_buf;
		dwin_var_set(val_buf*100, YGearRatioVar);
		return;
	}
	else if(addr == ZGearRatioVar)
	{
		val_buf = (float)key_value/10;
		planner.settings.axis_steps_per_mm[Z_AXIS] = val_buf;
		dwin_var_set(val_buf*10, ZGearRatioVar);
		return;
	}
	else if(addr == EGearRatioVar)
	{
		val_buf = (float)key_value/10;
		planner.settings.axis_steps_per_mm[E_AXIS] = val_buf;
		dwin_var_set(val_buf*10, EGearRatioVar);
		return;
	}
	else if(addr == XMaxAccelerationVar)
	{
		val_buf = key_value;
		planner.set_max_acceleration(X_AXIS, val_buf);
		val_buf = planner.settings.max_acceleration_mm_per_s2[X_AXIS];
		dwin_var_set(val_buf, XMaxAccelerationVar);
		return;
	}
	else if(addr == YMaxAccelerationVar)
	{
		val_buf = key_value;
		planner.set_max_acceleration(Y_AXIS, val_buf);
		val_buf = planner.settings.max_acceleration_mm_per_s2[Y_AXIS];
		dwin_var_set(val_buf, YMaxAccelerationVar);
		return;
	}
	else if(addr == ZMaxAccelerationVar)
	{
		val_buf = key_value;
		planner.set_max_acceleration(Z_AXIS, val_buf);
		val_buf = planner.settings.max_acceleration_mm_per_s2[Z_AXIS];
		dwin_var_set(val_buf, ZMaxAccelerationVar);
		return;
	}
	else if(addr == EMaxAccelerationVar)
	{
		val_buf = key_value;
		planner.set_max_acceleration(E_AXIS, val_buf);
		val_buf = planner.settings.max_acceleration_mm_per_s2[E_AXIS];
		dwin_var_set(val_buf, EMaxAccelerationVar);
		return;
	}
	else if(addr == XMaxSpeedVar)
	{
		val_buf = key_value;
		planner.set_max_feedrate(X_AXIS, val_buf);
		val_buf = planner.settings.max_feedrate_mm_s[X_AXIS];
		dwin_var_set(val_buf, XMaxSpeedVar);
		return;
	}
	else if(addr == YMaxSpeedVar)
	{
		val_buf = key_value;
		planner.set_max_feedrate(Y_AXIS, val_buf);
		val_buf = planner.settings.max_feedrate_mm_s[Y_AXIS];
		dwin_var_set(val_buf, YMaxSpeedVar);
		return;
	}
	else if(addr == ZMaxSpeedVar)
	{
		val_buf = key_value;
		planner.set_max_feedrate(Z_AXIS, val_buf);
		val_buf = planner.settings.max_feedrate_mm_s[Z_AXIS];
		dwin_var_set(val_buf, ZMaxSpeedVar);
		return;
	}
	else if(addr == EMaxSpeedVar)
	{
		val_buf = key_value;
		planner.set_max_feedrate(E_AXIS, val_buf);
		val_buf = planner.settings.max_feedrate_mm_s[E_AXIS];
		dwin_var_set(val_buf, EMaxSpeedVar);
		return;
	}
	else if(addr == XMaxJerkAccVar)
	{
		val_buf = (float)key_value/10;
		planner.set_max_jerk(X_AXIS, val_buf);
		val_buf = planner.max_jerk.x;
		dwin_var_set(val_buf*10, XMaxJerkAccVar);
		return;
	}
	else if(addr == YMaxJerkAccVar)
	{
		val_buf = (float)key_value/10;
		planner.set_max_jerk(Y_AXIS, val_buf);
		val_buf = planner.max_jerk.y;
		dwin_var_set(val_buf*10, YMaxJerkAccVar);
		return;
	}
	else if(addr == ZMaxJerkAccVar)
	{
		val_buf = (float)key_value/10;
		planner.set_max_jerk(Z_AXIS, val_buf);
		val_buf = planner.max_jerk.z;
		dwin_var_set(val_buf*10, ZMaxJerkAccVar);
		return;
	}
	else if(addr == EMaxJerkAccVar)
	{
		val_buf = (float)key_value/10;
		planner.set_max_jerk(E_AXIS, val_buf);
		val_buf = planner.max_jerk.e;
		dwin_var_set(val_buf*10, EMaxJerkAccVar);
		return;
	}

	// 过滤非按键的命令
	if(addr !=TestPageKey) return;
		
	switch(key_value)
	{
		case key_zero: // 返回到设置页面
			settings.save();
			dwin_page_set(DwinPageInf[PSetting].pagenum[0]);
			break;
			
		case key_one: // 步进值（传动比）与最大速度界面
			if(Language == 1)
			{
				dwin_page_set(DwinPageInf[PTest].pagenum[0]);
			}
			else
			{
				dwin_page_set(DwinPageInf[PTest].pagenum[2]);
			}			
			break;
			
		case key_two:// 最大加速度与最大拐角加速度界面
			if(Language == 1)
			{
				dwin_page_set(DwinPageInf[PTest].pagenum[1]);
			}
			else
			{
				dwin_page_set(DwinPageInf[PTest].pagenum[3]);
			}
			break;
	}
}
#endif

// 屏命令处理
void LCD_Handle(unsigned long addr, unsigned short data)
{
	int pname, pnum; 
	pname = 0;
	pnum = dwin_get_currentpage();
	SERIAL_ECHOLNPAIR(" pnum =  ", pnum);
	
	for(int i =0; DwinPageInf[i].pagename != 255; i++)
	{
		for(int j=0;j < PAGEMAXNUM;j++)
		{
			if(pnum == DwinPageInf[i].pagenum[j])
			{
				pname = DwinPageInf[i].pagename;
				break;
			}
			else if(!DwinPageInf[i].pagenum[j])
				break;
		}
		
		if(pname)break;
	}
	SERIAL_ECHOLNPAIR(" pname =  ", pname);
	switch(pname)
	{
		case PMain:
			Main_page();
			break;
			
		case PPrinting:
			Printing_page();
			break;
			
		case PFile:
			File_page();
			break;
			
		case PTempControl:
			Temp_page(pnum);
			break;
			
		case PAuxlevel:
			Assitantlevel_page();
			break;
			
		case PAutobedlevel:
			Autobedlevel_page();
			break;
			
		case PMoveControl:
			MoveControl_page();
			break;
			
		case PSetting:
			Setting_page();
			break;

		case PNoFilement:
			NoFilement_page();
			break;

		case PPopUp:
			PopUp_page();
			break;
			
		case PPowerloss:
			PowerLoss_page();
			break;
			
		case PError:
			Error_page();
			break;

#if ENABLED(TEST_MODE)
		case PTest:
			Test_page();
			break;
#endif
		default:
			break;
	}
}

//***********************************************************
// 打印完成
void Print_Completed(void)
{
	dwin_var_set(100, PercentageVar);
	//delay(1);
	dwin_var_set(100 ,PrintscheduleIcon);
	dwin_page_set(DwinPageInf[PPrinting].pagenum[4]);
	SDCard_Out_ToMainPage_Set(false);
	
	thermalManager.disable_all_heaters();

	queue.clear();
	char sizebuf[30]={0};
	sprintf(sizebuf,"G91\nG0 Z+%d\nG90", 5);
	queue.enqueue_now_P(sizebuf);
	memset(sizebuf, 0, sizeof(sizebuf));
	sprintf(sizebuf,"G1 X%d Y%d F%d", X_BED_SIZE, Y_BED_SIZE, 1500);
	queue.enqueue_now_P(sizebuf);
	//Set_Light(yellow, ON);
	Set_Light(red_green, ON);
	delay(500);
	Set_Light(red_green, OFF);
	delay(500);
	Set_Light(red_green, ON);
	delay(500);
	Set_Light(red_green, OFF);
}

// 回零后各种情况处理
void After_Home(void)
{
	switch(Mode_After_Home_get())
	{
		case MoveHome:
			dwin_var_set(10*current_position[X_AXIS], DisplayXaxisVar);
			dwin_var_set(10*current_position[Y_AXIS], DisplayYaxisVar);
			dwin_var_set(10*current_position[Z_AXIS], DisplayZaxisVar);
			
			dwin_page_set(DwinPageInf[PMoveControl].pagenum[0]);
			break;

		case AuxLevelHome:
			dwin_page_set(DwinPageInf[PAuxlevel].pagenum[0]);
			break;
		
		case AutoLevelHome:
			dwin_page_set(DwinPageInf[PAutobedlevel].pagenum[0]);
			break;

		case NoFilamentHome:
			dwin_page_set(DwinPageInf[PNoFilement].pagenum[0]);
			break;

		case PauseHome:
			dwin_page_set(DwinPageInf[PPrinting].pagenum[2]);
			break;

		case StopHome:
			dwin_page_set(DwinPageInf[PMain].pagenum[0]);
			break;
	}
	Mode_After_Home_set(None);
}

// 自动测量的数据显示
void Show_Leveling_progress(xy_int8_t meshCount)
{
	static uint16_t showcount = 0;

	// if(meshCount.x == 0 && meshCount.y == 0) showcount = 0;
	if(((showcount++) < GRID_MAX_POINTS_X * GRID_MAX_POINTS_Y))
	{
		dwin_icon_set(showcount, AutolevelIcon);
	}
	dwin_var_set(z_values[meshCount.x][meshCount.y]*100, AutolevelVaR + (showcount-1)*2);

	if(showcount == GRID_MAX_POINTS_X * GRID_MAX_POINTS_Y) showcount = 0;
}

  // 自动调平完成处理
void Leveling_completed(void)
{
	dwin_page_set(DwinPageInf[PAutobedlevel].pagenum[0]);
	settings.save();
}
  
// 加热失败
void Thermal_Runaway_fail(void)
{
	dwin_page_set(DwinPageInf[PThermalRunaway].pagenum[0]);
	Set_Light(red, true);
}

// 加热失败
void Heating_fail(void)
{
	dwin_page_set(DwinPageInf[PHeatingFailed].pagenum[0]);
	Set_Light(red, true);
}

void Error_Mode_Handle(uint8_t mode)
{
	LOOP_L_N(i, 6){dwin_var_set(0,ErrorPageText+i);}
	LOOP_L_N(i, 20){dwin_var_set(0,ErrorText+i);}
	dwin_unicode_set(Language == 1?Error_Sure:Error_Sure_En, ErrorPageText,Language == 1?sizeof(Error_Sure):sizeof(Error_Sure_En));
	
	if(mode == Error_Mode_201)
	{
		dwin_unicode_set(Language == 1?Error_201:Error_201_En, ErrorText,Language == 1?sizeof(Error_201):sizeof(Error_201_En));
	}
	else if(mode == Error_Mode_202)
	{
		dwin_unicode_set(Language == 1?Error_202:Error_202_En, ErrorText,Language == 1?sizeof(Error_202):sizeof(Error_202_En));
		// No processing is done in printing
		if(!printingIsActive() && !printingIsPaused())
		{
			queue.enqueue_now_P(PSTR("G28"));
			dwin_icon_set(1, MotorIcon);
		}
	}
	else if(mode== Error_Mode_203)
	{
		//dwin_str_set(Error_203, ErrorText);
		dwin_unicode_set(Language == 1?Error_203:Error_203_En, ErrorText,Language == 1?sizeof(Error_203):sizeof(Error_203_En));
		reset_bed_level();
	}
	else if(mode == Error_Mode_204)
	{
		SERIAL_ERROR_MSG(STR_SD_ERR_READ);
		//dwin_str_set(Error_204, ErrorText);
		dwin_unicode_set(Language == 1?Error_204:Error_204_En, ErrorText,Language == 1?sizeof(Error_204):sizeof(Error_204_En));
	}
	Error_Mode_set(mode);
	dwin_page_set(DwinPageInf[PError].pagenum[0]);
}
//***********************************************************
#define	TIME_UNIT	2000	// unit:ms
bool time_refresh(unsigned long *var, unsigned long time_units)
{
	if((millis() - *var) > time_units)
	{
		*var = millis();
		return true;
	}
	else return false;
}

// 中英文的图标切换更新
void Language_Update(void)
{
	LOOP_L_N(i, ServoAlarmTitle -Main1Title + 1){dwin_icon_set(Language, Main1Title+i);}
}

// 更新数据
void Refresh_data(void)
{
	static unsigned long next_update_ms = millis();
	if(time_refresh(&next_update_ms, TIME_UNIT)) // one time each 2s
	{
		if(card.isPrinting()) // printing
		{
			millis_t time_sec = print_job_timer.duration();	// print timer
			dwin_var_set(time_sec/3600, TimehourVar);		
			dwin_var_set((time_sec%3600)/60, TimeminVar);	

			static unsigned int last_percentVal = 101; 
			if(last_percentVal != card.percentDone()) 
			{
				if((unsigned int) card.percentDone() > 0)
				{
					uint8_t percent_val = card.percentDone()+1;
					dwin_icon_set((unsigned int)percent_val ,PrintscheduleIcon);
				}	
				else
				{
					dwin_icon_set(0,PrintscheduleIcon);
				}
				dwin_var_set((unsigned int) card.percentDone(),PercentageVar);
				last_percentVal = card.percentDone();
			}

			long temp_percent[3];
			temp_percent[0] = thermalManager.temp_hotend[0].celsius * 100 / thermalManager.temp_hotend[0].target;
			temp_percent[1] = thermalManager.temp_hotend[1].celsius * 100 / thermalManager.temp_hotend[1].target;
			temp_percent[2] = thermalManager.temp_bed.celsius * 100 / thermalManager.temp_bed.target;
			LOOP_L_N(i, 3){temp_percent[i] = temp_percent[i] < 100? temp_percent[i]/2:50;}
			
			dwin_var_set(temp_percent[0], Nozzle0TempIcon);
			dwin_var_set(temp_percent[1], Nozzle1TempIcon);
			dwin_var_set(temp_percent[2], BedTempIcon);

			if(last_zoffset != probe.offset.z)
			{
				last_zoffset = probe.offset.z;
				settings.save();
			}
		}

		dwin_var_set(thermalManager.temp_hotend[0].celsius,Nozzle0TempVar);
		dwin_var_set(thermalManager.temp_hotend[1].celsius,Nozzle1TempVar);		
		dwin_var_set(thermalManager.temp_bed.celsius,BedTempVar);

		dwin_var_set(thermalManager.temp_hotend[0].target,Nozzle0PreheatVar);
		dwin_var_set(thermalManager.temp_hotend[1].target,Nozzle1PreheatVar);
		dwin_var_set(thermalManager.temp_bed.target,BedPreheatVar);
	}
}

// 伺服电机报警检测
void Servo_alarm_detect(void)
{
	if(READ(SERVO_ALARM_PIN) == 0)
	{
		delay(100);
		if(READ(SERVO_ALARM_PIN) == 0)
		{
			Set_Light(red, ON);
			dwin_page_set(DwinPageInf[PServoAlarm].pagenum[0]);
			kill(M112_KILL_STR, nullptr, true);
		}
	}
}

// LCD屏幕相关参数，图标初始化
void LCD_Init(void)
{
	Filament_pin_init();
	Servo_alarm_init();
	Light_init();

	settings.load();

	All_Page_init();
	
	// 启动进程的进度条
	Start_up_Procedure();

	// 断电续打
	Power_Loss_Recovery();
}

// LCD定时更新
void LCD_Update()
{
	Servo_alarm_detect();
	SDCard_Upate();
	Refresh_data();
	Filament_Check_InPrinting();

	Emergency_Stop();

	Printing_Pause_Process();
	
	if(dwinlcd.UART_RecData() > 0)
	{
		LCD_Handle(dwinlcd.recdat.addr, dwinlcd.recdat.data[0]);
		memset(&dwinlcd.recdat,0 , sizeof(dwinlcd.recdat));
		dwinlcd.recdat.head[0] = FHONE;
		dwinlcd.recdat.head[1] = FHTWO;
	}
}
 
	
