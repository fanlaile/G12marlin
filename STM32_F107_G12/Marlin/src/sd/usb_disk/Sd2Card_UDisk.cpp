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

#include "../../inc/MarlinConfigPre.h"


#if ENABLED(USB_DISK_SUPPORT)
  #include "../../MarlinCore.h"
  #include "../../core/serial.h"

  #include "../cardreader.h"
  #include "Sd2Card_UDisk.h"

  #include "../../HAL/STM32/usbh_disk.h"

  // 挂载设备
  static void mount_cb(void)
  {
    SERIAL_ECHO_MSG("USB Disk connection");
    if(card.isMounted() == false) card.mount();
  }

  // 卸载设备
  static void unmount_cb(void)
  {
    SERIAL_ECHO_MSG("USB Disk disconnect");
    if(card.isMounted() == true)
    {
      card.release();
    }
  }


  
  // USB后台任务需要周期性调用
  void Sd2Card::idle() 
  {
    // USB主机后台任务
    usbh_disk_process();
  }

  // U盘插入检测
  bool Sd2Card::isInserted()
  {
    return usbh_disk_is_inserted();
  }

  // USB Host协议栈及OTG_FS硬件初始化
  bool Sd2Card::config(void)
  {
    if(usbh_disk_init(mount_cb, unmount_cb) == false)
    {
      SERIAL_ECHO_MSG("usbh_disk_init Fail");
      return false;
    }

    // 检测U盘
    constexpr millis_t TIMEOUT_MS = 2000UL;
    millis_t init_timeout_ms = millis() + TIMEOUT_MS;
    while(!ELAPSED(millis(), init_timeout_ms))
    {
      idle();

      TERN_(USE_WATCHDOG, HAL_watchdog_refresh());
      if(isInserted())
      {
        SERIAL_ECHO_MSG("Sd2Card::config OK");
        return true;
      }
    }

    SERIAL_ECHO_MSG("Sd2Card::config Timeout");

    return false;
  }

  // U盘初始化(保留该接口，兼容现有代码，使代码改动最小)
  bool Sd2Card::init(const uint8_t, const pin_t)
  {
    if (!isInserted())
    {
      return false;
    }
    return true;
  }

  // 读块. block:块号, dst:读缓冲区地址
  bool Sd2Card::readBlock(uint32_t block, uint8_t* dst)
  {
    // if(!isInserted())
    // {
    //   return false;
    // }
    if(usbh_disk_read_block(block, dst) == false)
    {
      SERIAL_ECHO_MSG("usbh_disk_read_block Fail");
      return false;
    }
    return true;
  }

  // 写块. block:块号, src:写缓冲区地址
  bool Sd2Card::writeBlock(uint32_t block, const uint8_t* src)
  {
    // if(!isInserted())
    // {
    //   return false;
    // }
    if(usbh_disk_write_block(block, src) == false)
    {
      SERIAL_ECHO_MSG("USBH_MSC_Write Fail");
      return false;
    }
    return true;
  }

#endif // USB_DISK_SUPPORT
