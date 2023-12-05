# DISCO_H747I
DISCO_H747I library to be used for integration into mbed-os

The library has been tested with mbed-os 6.17.0. Changes documented
below also apply to mbed-os 6.17.0.

After importing this library, one needs to: 
- Modify the "mbed-os/targets/targets.json" file as follows:
   - For device "DISCO_H747I_CM7" (line 3456), add the following property:
      ```json
      "config": {
          "usb_speed": {
              "help": "USE_USB_OTG_FS or USE_USB_OTG_HS or USE_USB_HS_IN_FS",
              "value": "USE_USB_OTG_HS"
          }
      }
      ```
   - For device "DISCO_H747I_CM7" (line 3456), modify the "device_has_add" property
   as follows:
      ```json
      "device_has_add": [
          "QSPI",
          "USBDEVICE"
      ]
      ```
   - Comment lines 550 and 551 of file "mbed-os/targets/TARGET_STM/TARGET_STM32H7/TARGET_STM32H747xI/TARGET_DISCO_H747I\PeripheralPins.c"
     ```json
   // {PC_2, USB_HS, STM_PIN_DATA(STM_MODE_AF_PP, GPIO_PULLUP, GPIO_AF10_OTG2_HS)}, // USB_OTG_HS_ULPI_DIR // Connected to PMOD\#3
   // {PC_3, USB_HS, STM_PIN_DATA(STM_MODE_AF_PP, GPIO_PULLUP, GPIO_AF10_OTG2_HS)}, // USB_OTG_HS_ULPI_NXT // Connected to PMOD\#2
     ```
