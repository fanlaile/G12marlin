
#include "../../inc/MarlinConfig.h"

#if ENABLED(SPI_FLASH_EEPROM_EMULATION)
  #include "../../libs/W25Qxx.h"
  #include "../shared/eeprom_api.h"
  
  #define DEBUG_OUT ENABLED(EEPROM_CHITCHAT)
  #include "src/core/debug_out.h"

  #ifndef MARLIN_EEPROM_SIZE
    #define MARLIN_EEPROM_SIZE    0x1000 // 4KB
  #endif

  #ifndef SPI_FLASH_SIZE
    #define SPI_FLASH_SIZE    (2 * 1024 * 1024) // 2M (W25Q32)
  #endif

  #ifndef SPI_FLASH_SECTOR
    #define SPI_FLASH_SECTOR          32
  #endif
  #ifndef SPI_FLASH_SECTOR_SIZE
    #define SPI_FLASH_SECTOR_SIZE       (0x1000) // 4KB
  #endif

  #define FLASH_USE_MAX_SIZE      (SPI_FLASH_SECTOR * SPI_FLASH_SECTOR_SIZE)

  #define FLASH_ADDRESS_START     (SPI_FLASH_SIZE - FLASH_USE_MAX_SIZE)
  #define FLASH_ADDRESS_END       (SPI_FLASH_SIZE)

  #define EEPROM_SLOTS            (FLASH_USE_MAX_SIZE / (MARLIN_EEPROM_SIZE))
  #define SLOT_ADDRESS(slot)      (FLASH_ADDRESS_START + (slot * (MARLIN_EEPROM_SIZE)))

  #define EMPTY_UINT32            ((uint32_t)-1)
  #define EMPTY_UINT8             ((uint8_t)-1)

  static uint8_t ram_eeprom[MARLIN_EEPROM_SIZE] __attribute__((aligned(4))) = {0};
  int current_slot = -1;

  static_assert(0 == MARLIN_EEPROM_SIZE % 4, "MARLIN_EEPROM_SIZE must be a multiple of 4"); // Ensure copying as uint32_t is safe
  static_assert(0 == FLASH_USE_MAX_SIZE % MARLIN_EEPROM_SIZE, "MARLIN_EEPROM_SIZE must divide evenly into your FLASH_USE_MAX_SIZE");
  static_assert(FLASH_USE_MAX_SIZE >= MARLIN_EEPROM_SIZE, "FLASH_USE_MAX_SIZE must be greater than or equal to your MARLIN_EEPROM_SIZE");
  static_assert(IS_POWER_OF_2(FLASH_USE_MAX_SIZE), "FLASH_USE_MAX_SIZE should be a power of 2, please check your chip's spec sheet");

  static bool eeprom_data_written = false;

  // 检查buffer中的所有数据是否都为指定值
  static uint8_t mem_is_constval(const uint8_t *buf, uint8_t val, uint32_t size)
  {
    for(uint32_t i = 0; i < size; i++)
    {
        if(buf[i] != val)
        {
            return 0;
        }
    }
    return 1;
  }

  static void dichotomy_get_current_slot(void)
  {
    //
    // 二分法查找当前插槽
    //
    uint32_t start_slot = 0;
    uint32_t end_slot = EEPROM_SLOTS;
    
    while (1) 
    {
        uint32_t slot = (start_slot + end_slot) >> 1;
        uint32_t address = SLOT_ADDRESS(slot);

        W25QXX.SPI_FLASH_BufferRead(ram_eeprom, address, MARLIN_EEPROM_SIZE);
        if (mem_is_constval(ram_eeprom, EMPTY_UINT8, MARLIN_EEPROM_SIZE) == 0) 
        { // 已使用插槽
            end_slot = slot;
        }
        else
        { // 空闲插槽
            start_slot = slot;
        }

        if((end_slot == EEPROM_SLOTS) && (start_slot + 1 == EEPROM_SLOTS)) // 处理满空闲插槽的情况
        {
            break;
        }
        else if(start_slot == 0)
        {
            if((start_slot + 1 == end_slot && slot == start_slot) // 处理只有一个空闲插槽的情况
                || (end_slot == 0))                               // 处理无空闲插槽的情况
            {
                current_slot = end_slot;
                break;
            }
        }
        else if((start_slot + 1) == end_slot) // 处理既有空闲插槽、又有已使用插槽的情况
        {
            current_slot = end_slot;
            break;
        }
    }
  }

  bool PersistentStore::access_start()
  {
      if (current_slot == -1 || eeprom_data_written) 
      {
          W25QXX.init(SPI_QUARTER_SPEED);
          if (eeprom_data_written) DEBUG_ECHOLN("Dangling EEPROM write_data");

              // 二分法查找当前插槽，减少耗时
              dichotomy_get_current_slot();

          if (current_slot == -1) 
          {
              // 没有存储任何数据，清除缓冲数据
              for (uint32_t i = 0; i < MARLIN_EEPROM_SIZE; i++) ram_eeprom[i] = EMPTY_UINT8;
              current_slot = EEPROM_SLOTS;
          }
          else 
          {
              // 载入当前数据
              uint32_t eeprom_addr = SLOT_ADDRESS(current_slot);
              W25QXX.SPI_FLASH_BufferRead(ram_eeprom, eeprom_addr, MARLIN_EEPROM_SIZE);

          }
          eeprom_data_written = false;
      }

      return true;
  }

  bool PersistentStore::access_finish()
  {
      if (eeprom_data_written) 
      {
          if (--current_slot < 0) 
          {
              // 已用完所有插槽，擦掉所有区域
              current_slot = EEPROM_SLOTS - 1;
              for(int sector = 0; sector < SPI_FLASH_SECTOR; sector++) 
              {
                  W25QXX.SPI_FLASH_SectorErase(FLASH_ADDRESS_START + sector * SPI_FLASH_SECTOR_SIZE);
              }
          }
          
          W25QXX.SPI_FLASH_BufferWrite(ram_eeprom, SLOT_ADDRESS(current_slot), MARLIN_EEPROM_SIZE);
          eeprom_data_written = false;
      }
    return true;
  }

  bool PersistentStore::write_data(int &pos, const uint8_t *value, size_t size, uint16_t *crc)
  {
      while (size--) 
      {
          uint8_t v = *value;
          if (v != ram_eeprom[pos]) 
          {
              ram_eeprom[pos] = v;
              eeprom_data_written = true;
          }
          crc16(crc, &v, 1);
          pos++;
          value++;
      }

      return false;
  }

  bool PersistentStore::read_data(int &pos, uint8_t* value, size_t size, uint16_t *crc, const bool writing/*=true*/)
  {
      do 
      {
          const uint8_t c = ram_eeprom[pos];
          if (writing) *value = c;
          crc16(crc, &c, 1);
          pos++;
          value++;
      } while (--size);

      return false;
  }

  size_t PersistentStore::capacity()
  {
      return MARLIN_EEPROM_SIZE;
  }

#endif // SPI_FLASH_EEPROM_EMULATION
