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
#pragma once

/**
 * \file
 * \brief Sd2Card class for UDisk
 */

#include "../SdFatConfig.h"

class Sd2Card
{
  private:

  public:

    // Sd2Card() { config(); } // 构造函数中进行USB Host及OTG_FS硬件初始化

    static bool config(void);

    bool init(const uint8_t sckRateID = 0, const pin_t chipSelectPin = 0); // 保留该接口，实际未使用，仅为了兼容现有代码

    static void idle();

    bool readBlock(uint32_t block, uint8_t* dst);
    bool writeBlock(uint32_t blockNumber, const uint8_t* src);

    static bool isInserted();
};
