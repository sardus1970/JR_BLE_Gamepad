#ifndef JRGAMEPAD_H
#define JRGAMEPAD_H
#include "sdkconfig.h"
#if defined(CONFIG_BT_ENABLED)

#include "BLEHIDDevice.h"
#include "BLECharacteristic.h"

// Gamepad axes count, resolution (16-bit) and limits

#define AXIS_COUNT 12

class JRGamepad {

  private:
    BLEHIDDevice* hid;
    static void taskServer (void* pvParameter);
    
  public:
    uint8_t batteryLevel;
    std::string deviceManufacturer;
    std::string deviceName;
    
    bool connected;           // true if paired and connected to host
    bool dualGamepad;         // true if more than 6 channels are required
    bool compatibilityMode;   // true if axes should have 8 instead of 16 bit resolution
  
    BLECharacteristic* inputGamepad1;
    BLECharacteristic* inputGamepad2;
    JRGamepad ( std::string deviceName          = "JR Gamepad 16",
                std::string deviceManufacturer  = "Fabrizio Sitzia",
    			      uint8_t batteryLevel      = 100 );
  
    void begin (int channelCount);
    void end (void);
    void setAxes(int16_t axes[]);
      
  protected:
    virtual void onStarted (BLEServer *pServer) { };
};

#endif // CONFIG_BT_ENABLED
#endif // JRGAMEPAD_H
