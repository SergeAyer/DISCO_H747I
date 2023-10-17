# DISCO_H747I
DISCO_H747I library to be used for integration into mbed-os

(Tested with mbed os 6.17.0) After importing this library, one needs to: 
- Modifiy the "mbed_app.json" file of your application as follows:
   - In the "config" section:
      "usb_speed": {
        "help": "USE_USB_OTG_FS or USE_USB_OTG_HS or USE_USB_HS_IN_FS",
        "value": "USE_USB_OTG_FS"
      }

   - In the "target_overrides" section (for device "DISCO_H747I"):   
      "target.device_has_add": ["USBDEVICE"]





