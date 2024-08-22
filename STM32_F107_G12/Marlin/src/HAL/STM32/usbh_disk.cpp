#if defined(ARDUINO_ARCH_STM32) && !defined(STM32GENERIC)

#include "../../inc/MarlinConfigPre.h"

#if ENABLED(USB_DISK_SUPPORT)

  // 当前OTG_FS(USB Host)仅适配了STM32F107xC
  #if NONE(STM32F107xC)
    #error "ERROR - Only STM32F107xC CPUs supported"
  #endif

  #include "../../inc/MarlinConfig.h"

  #include "./USB_Host_MSC/usbh_core.h"
  #include "./USB_Host_MSC/usbh_msc.h"

  typedef enum
  {
    APPLICATION_IDLE = 0,
    APPLICATION_READY,
    APPLICATION_DISCONNECT,
  }MSC_ApplicationTypeDef;

  static USBH_HandleTypeDef hUSBHost;
  static MSC_ApplicationTypeDef state = APPLICATION_IDLE;
  static void (*callback_mount)(void);
  static void (*callback_unmount)(void);


  // USB Host内核事件回调
  static void USBH_UserProcess(USBH_HandleTypeDef * phost, uint8_t id)
  {
    switch (id)
    {
    case HOST_USER_SELECT_CONFIGURATION:
      break;

    case HOST_USER_DISCONNECTION:
      state = APPLICATION_DISCONNECT;
      if(callback_unmount) callback_unmount();
      break;

    case HOST_USER_CLASS_ACTIVE:
      state = APPLICATION_READY;
      if(callback_mount) callback_mount();
      break;

    case HOST_USER_CONNECTION:
      break;

    default:
      break;
    }
  }

  bool usbh_disk_init(void (*mount_cb)(void), void (*unmount_cb)(void))
  {
      // 初始化USB主机库
    if(USBH_Init(&hUSBHost, USBH_UserProcess, 1) != USBH_OK)
    {
      return false;
    }

    // 添加支持类
    if(USBH_RegisterClass(&hUSBHost, USBH_MSC_CLASS) != USBH_OK)
    {
      return false;
    }

    // 开启主机进程
    if(USBH_Start(&hUSBHost) != USBH_OK)
    {
      return false;
    }

    if(mount_cb) callback_mount = mount_cb;
    if(unmount_cb) callback_unmount = unmount_cb;

    return true;
  }

  bool usbh_disk_is_inserted(void)
  {
      return (state == APPLICATION_READY);
  }

  bool usbh_disk_read_block(uint32_t block, uint8_t *dst)
  {
    if(USBH_MSC_Read(&hUSBHost, 0, block, (uint8_t *)dst, 1) != USBH_OK)
    {
      return false;
    }

    return true;
  }

  bool usbh_disk_write_block(uint32_t block, const uint8_t *src)
  {
    if(USBH_MSC_Write(&hUSBHost, 0, block, (uint8_t *)src, 1) != USBH_OK)
    {
      return false;
    }

    return true;
  }

  void usbh_disk_process(void)
  {
    // USB主机后台任务
    USBH_Process(&hUSBHost);
  }

#endif // USB_DISK_SUPPORT
#endif // ARDUINO_ARCH_STM32 && !STM32GENERIC


