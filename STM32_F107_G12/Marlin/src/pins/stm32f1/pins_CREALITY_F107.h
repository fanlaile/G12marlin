/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2020 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (c) 2011 Camiel Gubbels / Erik van der Zalm
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

// Source: 

/**
 * HOW TO COMPILE
 *
 * PlatformIO - Use the STM32F107VC environment (or the "Auto Build Marlin" extension).
 */

#pragma once

#if NOT_TARGET(STM32F1)
  #error "Oops! Select an STM32F1 board in 'Tools > Board.'"
#endif

#ifndef BOARD_INFO_NAME
  #define BOARD_INFO_NAME      "Creality 3D V6.0.2"
#endif

#ifndef DEFAULT_MACHINE_NAME
  #define DEFAULT_MACHINE_NAME "G12"
#endif

//
// Limit Switches
//
#define X_MIN_PIN                             PC4
#define Y_MIN_PIN                             PC5

#define X_MAX_PIN                             PB1
#define Y_MAX_PIN                             PA5
#define Z_MAX_PIN                             PB2

//
// Servos
//
#ifdef BLTOUCH
  #define SERVO0_PIN                          PB2   // BLTouch OUT
  #define Z_MIN_PIN                           PE7   // BLTouch IN
#else
  #define Z_MIN_PIN                           PB0
#endif

//
// Z Probe (when not Z_MIN_PIN)
//
//#ifndef Z_MIN_PROBE_PIN
//  #define Z_MIN_PROBE_PIN  Z_MIN_PIN
//#endif

//
// Filament runout
//
#define FIL_RUNOUT_PIN                      PB0  // BED_THE
#define FIL_RUNOUT2_PIN                    PE7 // Y_MAX_PIN
//
// Steppers
//
#define X_ENABLE_PIN                          PB6
#define X_STEP_PIN                            PB5
#define X_DIR_PIN                             PB4

#define Y_ENABLE_PIN                          PB3
#define Y_STEP_PIN                            PD7
#define Y_DIR_PIN                             PD6

#define Z_ENABLE_PIN                          PD5
#define Z_STEP_PIN                            PD4
#define Z_DIR_PIN                             PD3

#define E0_ENABLE_PIN                         PD2
#define E0_STEP_PIN                           PD1
#define E0_DIR_PIN                            PD0

#define E1_ENABLE_PIN                         PC12
#define E1_STEP_PIN                           PC11
#define E1_DIR_PIN                            PC10

//
// Servo motor alarm
//
#define SERVO_ALARM_PIN                       PA5

//
// Temperature Sensors
//
#define TEMP_0_PIN                             PA0  // E1_THERMISTOR PA0 -> PA6,使用BOX0_TEMP替换TH0，TH0是PT100的电路
#define TEMP_1_PIN                             PA1  // E2_THERMISTOR
#define TEMP_BED_PIN                           PA7  // BED_THERMISTOR_1

//
// Heaters / Fans
//
#define HEATER_0_PIN                          PB8  // E1_HEAT_PWM
#define HEATER_1_PIN                          PB9  // E2_HEAT_PWM
#define HEATER_BED_PIN                        PB7  // BED_HEAT_1 FET

#define FAN_PIN                               PE11 //PE9   // Part cooling fan FET
//#define FAN1_PIN                              PE11  // Extruder fan FET
//#define FAN2_PIN                              PE1   // Controller fan FET

#ifndef E0_AUTO_FAN_PIN
  #define E0_AUTO_FAN_PIN                     PE11  // FAN1_PIN
#endif

// SPI Flash
#define HAS_SPI_FLASH                          1
#if HAS_SPI_FLASH
  #define SPI_FLASH_SIZE                0x1000000  // 16MB
#endif

// SPI 2
#define W25QXX_CS_PIN                       PB12
// 当前HAL/STM32的MOSI, MISO, SCK不由下面引脚定义，
// 需在buildroot\share\PlatformIO\variants\CREALITY_F107\variant.h中修改
#define W25QXX_MOSI_PIN                     PB15
#define W25QXX_MISO_PIN                     PB14
#define W25QXX_SCK_PIN                      PB13

#define SPI_FLASH_EEPROM_EMULATION
//
#define ABSORB_FILAMENT_PIN	PE0 // LIGHT_PIN 
//
// USB Disk
//
// PA11  OTG_DM
// PA12  OTG_DP

//
// Misc functions
//
#define SDSS                                  -1  // SPI_CS
#define LED_PIN                               -1  // green LED   Heart beat
#define PS_ON_PIN                             -1
#define KILL_PIN                              -1
#define POWER_LOSS_PIN                        -1  // PWR_LOSS / nAC_FAULT

