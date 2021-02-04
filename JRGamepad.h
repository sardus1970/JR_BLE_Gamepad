// The JRGamepad class implements the Gamepad HID & Bluetooth LE communications part.
//
// It borrows heavily from the Arduino "ESP32-BLE-Gamepad" library by lemmingdev,
// and from ESP32 code examples from a famous Marxist revolutionary (chegewara ;-)

#ifndef JRGAMEPAD_H
#define JRGAMEPAD_H
#include "sdkconfig.h"
#if defined(CONFIG_BT_ENABLED)

#include "BLEHIDDevice.h"
#include "BLECharacteristic.h"

// Gamepad modes
#define SINGLE_8BIT   0
#define SINGLE_16BIT  1
#define DUAL_8BIT     2
#define DUAL_16BIT    3
#define SINGLE_7BIT   4
#define SINGLE_15BIT  5
#define DUAL_7BIT     6
#define DUAL_15BIT    7


class JRGamepad {

  private:
    BLEHIDDevice* hid;
    static void taskServer (void* pvParameter);
    
  public:
    uint8_t batteryLevel;
    std::string deviceManufacturer;
    std::string deviceName;
    
    uint32_t gamepads;        // number of gamepads: 1 or 2
    uint32_t gamepadMode;     // 0-7 (see defines above)
    bool compatibilityMode;   // true if axes should have 8-bit instead of 16-bit resolution
    bool unityBugWorkaround;  // true if only positive axis values should be used (1 bit of resolution loss),
                              // ...needed for Unity-based RC simulators under Windows
    bool connected;           // true if paired and connected to host
   
    BLECharacteristic* inputGamepad[2];
    JRGamepad ( std::string deviceName          = "JR Gamepad",
                std::string deviceManufacturer  = "sardus1970",
    			      uint8_t batteryLevel            = 100 );
  
    void begin (uint8_t gamepadMode);
    void end (void);
    void setAxes(int16_t axes[]);
      
  protected:
    virtual void onStarted (BLEServer *pServer) { };
};

#endif // CONFIG_BT_ENABLED
#endif // JRGAMEPAD_H
