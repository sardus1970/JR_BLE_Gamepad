#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include "BLE2902.h"
#include "BLEHIDDevice.h"
#include "HIDTypes.h"
#include "HIDKeyboardTypes.h"
#include <driver/adc.h>
#include "sdkconfig.h"

#include "Arduino.h"
#include "JRGamepad.h"

static const char _gamepadName [][16] =
{
  "JR Gamepad 8",
  "JR Gamepad 16",
  "JR Gamepad 2x8",
  "JR Gamepad 2x16"
};


// HID reports:
//
// For maximum compatibility with generic gamepad drivers, a maximum of 6 PPM channels per gamepad
// are mapped as follows:
//
//  - The first 4 channels are mapped to the analog stick axes X, Y, Z and rZ
//  - The last 2 channels are mapped to the rX and rY axes, which are typically used for the left
//    and right analog triggers
//
// The "dual" modes with more than 6 channels are mapped as two gamepads in a composite HID report.

static const uint8_t _single8bitHIDreport[] = {
  
  USAGE_PAGE(1),                0x01, // USAGE_PAGE (Generic Desktop)
  USAGE(1),                     0x05, // USAGE (Gamepad)
  COLLECTION(1),                0x01, // COLLECTION (Application)
  
      REPORT_ID(1),        0x01, //     Gamepad 1
      
      // ------------------------------------------------- Buttons 1 to 8 - unused, but necessary
      USAGE_PAGE(1),       0x09, //     USAGE_PAGE (Button)
      USAGE_MINIMUM(1),    0x01, //     USAGE_MINIMUM (Button 1)
      USAGE_MAXIMUM(1),    0x08, //     USAGE_MAXIMUM (Button 8)
      LOGICAL_MINIMUM(1),  0x00, //     LOGICAL_MINIMUM (0)
      LOGICAL_MAXIMUM(1),  0x01, //     LOGICAL_MAXIMUM (1)
      REPORT_SIZE(1),      0x01, //     REPORT_SIZE (1)
      REPORT_COUNT(1),     0x08, //     REPORT_COUNT (8)
      HIDINPUT(1),         0x02, //     INPUT (Data, Variable, Absolute)
      
      // ------------------------------------------------- 6x 8-bit resolution gamepad axes
      USAGE_PAGE(1),       0x01, //     USAGE_PAGE (Generic Desktop)
      USAGE(1),            0x30, //     USAGE (X)
      USAGE(1),            0x31, //     USAGE (Y)
      USAGE(1),            0x32, //     USAGE (Z)
      USAGE(1),            0x33, //     USAGE (rX)
      USAGE(1),            0x34, //     USAGE (rY)
      USAGE(1),            0x35, //     USAGE (rZ)
      LOGICAL_MINIMUM(1),  0x81, //     LOGICAL_MINIMUM (-127)
      LOGICAL_MAXIMUM(1),  0x7F, //     LOGICAL_MAXIMUM (127)
      REPORT_SIZE(1),      0x08, //     REPORT_SIZE (8)
      REPORT_COUNT(1),     0x06, //     REPORT_COUNT (6)
      HIDINPUT(1),         0x02, //     INPUT (Data,Var,Abs) 
  END_COLLECTION(0)              // END_COLLECTION (Application)
};

static const uint8_t _single16bitHIDreport[] = {

  USAGE_PAGE(1),       0x01, // USAGE_PAGE (Generic Desktop)
  USAGE(1),            0x05, // USAGE (Gamepad)
  COLLECTION(1),       0x01, // COLLECTION (Application)
  
      REPORT_ID(1),        0x01, //     Gamepad 1
      
      // ------------------------------------------------- Buttons 1 to 8 - unused, but necessary
      USAGE_PAGE(1),       0x09, //     USAGE_PAGE (Button)
      USAGE_MINIMUM(1),    0x01, //     USAGE_MINIMUM (Button 1)
      USAGE_MAXIMUM(1),    0x08, //     USAGE_MAXIMUM (Button 8)
      LOGICAL_MINIMUM(1),  0x00, //     LOGICAL_MINIMUM (0)
      LOGICAL_MAXIMUM(1),  0x01, //     LOGICAL_MAXIMUM (1)
      REPORT_SIZE(1),      0x01, //     REPORT_SIZE (1)
      REPORT_COUNT(1),     0x08, //     REPORT_COUNT (8)
      HIDINPUT(1),         0x02, //     INPUT (Data, Variable, Absolute)
      
      // ------------------------------------------------- 6x16-bit resolution gamepad axes
      USAGE_PAGE(1),       0x01, //     USAGE_PAGE (Generic Desktop)
      USAGE(1),            0x30, //     USAGE (X)
      USAGE(1),            0x31, //     USAGE (Y)
      USAGE(1),            0x32, //     USAGE (Z)
      USAGE(1),            0x33, //     USAGE (rX)
      USAGE(1),            0x34, //     USAGE (rY)
      USAGE(1),            0x35, //     USAGE (rZ)
      0x16,                0x01, 0x80,  // LOGICAL_MINIMUM (-32767)
      0x26,                0xFF, 0x7F,  // LOGICAL_MAXIMUM (32767)
      REPORT_SIZE(1),      0x10, //     REPORT_SIZE (16)
      REPORT_COUNT(1),     0x06, //     REPORT_COUNT (6)
      HIDINPUT(1),         0x02, //     INPUT (Data,Var,Abs)
 
  END_COLLECTION(0)          //     END_COLLECTION
};

static const uint8_t _dual8bitHIDreport[] = {
  USAGE_PAGE(1),       0x01, // USAGE_PAGE (Generic Desktop)
  USAGE(1),            0x05, // USAGE (Gamepad)
  COLLECTION(1),       0x01, // COLLECTION (Application)
      
      REPORT_ID(1),        0x01, //     Gamepad 1 / 2
      
      // ------------------------------------------------- Buttons 1 to 8 - unused, but necessary
      USAGE_PAGE(1),       0x09, //     USAGE_PAGE (Button)
      USAGE_MINIMUM(1),    0x01, //     USAGE_MINIMUM (Button 1)
      USAGE_MAXIMUM(1),    0x08, //     USAGE_MAXIMUM (Button 8)
      LOGICAL_MINIMUM(1),  0x00, //     LOGICAL_MINIMUM (0)
      LOGICAL_MAXIMUM(1),  0x01, //     LOGICAL_MAXIMUM (1)
      REPORT_SIZE(1),      0x01, //     REPORT_SIZE (1)
      REPORT_COUNT(1),     0x08, //     REPORT_COUNT (8)
      HIDINPUT(1),         0x02, //     INPUT (Data, Variable, Absolute)
      
      // ------------------------------------------------- 6x 8-bit resolution gamepad axes
      USAGE_PAGE(1),       0x01, //     USAGE_PAGE (Generic Desktop)
      USAGE(1),            0x30, //     USAGE (X)
      USAGE(1),            0x31, //     USAGE (Y)
      USAGE(1),            0x32, //     USAGE (Z)
      USAGE(1),            0x33, //     USAGE (rX)
      USAGE(1),            0x34, //     USAGE (rY)
      USAGE(1),            0x35, //     USAGE (rZ)
      LOGICAL_MINIMUM(1),  0x81, //     LOGICAL_MINIMUM (-127)
      LOGICAL_MAXIMUM(1),  0x7F, //     LOGICAL_MAXIMUM (127)
      REPORT_SIZE(1),      0x08, //     REPORT_SIZE (8)
      REPORT_COUNT(1),     0x06, //     REPORT_COUNT (6)
      HIDINPUT(1),         0x02, //     INPUT (Data,Var,Abs) 
  END_COLLECTION(0),         //     END_COLLECTION
   
  USAGE(1),            0x05, // USAGE (Gamepad)
  COLLECTION(1),       0x01, // COLLECTION (Application)
      
      REPORT_ID(1),        0x02, //     Gamepad 2 / 2
      
      // ------------------------------------------------- Buttons 1 to 8 - unused, but necessary
      USAGE_PAGE(1),       0x09, //     USAGE_PAGE (Button)
      USAGE_MINIMUM(1),    0x01, //     USAGE_MINIMUM (Button 1)
      USAGE_MAXIMUM(1),    0x08, //     USAGE_MAXIMUM (Button 8)
      LOGICAL_MINIMUM(1),  0x00, //     LOGICAL_MINIMUM (0)
      LOGICAL_MAXIMUM(1),  0x01, //     LOGICAL_MAXIMUM (1)
      REPORT_SIZE(1),      0x01, //     REPORT_SIZE (1)
      REPORT_COUNT(1),     0x08, //     REPORT_COUNT (8)
      HIDINPUT(1),         0x02, //     INPUT (Data, Variable, Absolute)
      
      // ------------------------------------------------- 6x 8-bit resolution gamepad axes
      USAGE_PAGE(1),       0x01, //     USAGE_PAGE (Generic Desktop)
      USAGE(1),            0x30, //     USAGE (X)
      USAGE(1),            0x31, //     USAGE (Y)
      USAGE(1),            0x32, //     USAGE (Z)
      USAGE(1),            0x33, //     USAGE (rX)
      USAGE(1),            0x34, //     USAGE (rY)
      USAGE(1),            0x35, //     USAGE (rZ)
      LOGICAL_MINIMUM(1),  0x81, //     LOGICAL_MINIMUM (-127)
      LOGICAL_MAXIMUM(1),  0x7F, //     LOGICAL_MAXIMUM (127)
      REPORT_SIZE(1),      0x08, //     REPORT_SIZE (8)
      REPORT_COUNT(1),     0x06, //     REPORT_COUNT (6)
      HIDINPUT(1),         0x02, //     INPUT (Data,Var,Abs)
  END_COLLECTION(0)          //     END_COLLECTION
};

static const uint8_t _dual16bitHIDreport[] = {

  USAGE_PAGE(1),       0x01, // USAGE_PAGE (Generic Desktop)
  USAGE(1),            0x05, // USAGE (Gamepad)
  COLLECTION(1),       0x01, // COLLECTION (Application)
      
      REPORT_ID(1),        0x01, //     Gamepad 1/2
      
      // ------------------------------------------------- Buttons 1 to 8 - unused, but necessary
      USAGE_PAGE(1),       0x09, //     USAGE_PAGE (Button)
      USAGE_MINIMUM(1),    0x01, //     USAGE_MINIMUM (Button 1)
      USAGE_MAXIMUM(1),    0x08, //     USAGE_MAXIMUM (Button 8)
      LOGICAL_MINIMUM(1),  0x00, //     LOGICAL_MINIMUM (0)
      LOGICAL_MAXIMUM(1),  0x01, //     LOGICAL_MAXIMUM (1)
      REPORT_SIZE(1),      0x01, //     REPORT_SIZE (1)
      REPORT_COUNT(1),     0x08, //     REPORT_COUNT (8)
      HIDINPUT(1),         0x02, //     INPUT (Data, Variable, Absolute)
      
      // ------------------------------------------------- 6x16-bit resolution gamepad axes
      USAGE_PAGE(1),       0x01, //     USAGE_PAGE (Generic Desktop)
      USAGE(1),            0x30, //     USAGE (X)
      USAGE(1),            0x31, //     USAGE (Y)
      USAGE(1),            0x32, //     USAGE (Z)
      USAGE(1),            0x33, //     USAGE (rX)
      USAGE(1),            0x34, //     USAGE (rY)
      USAGE(1),            0x35, //     USAGE (rZ)
      0x16,                0x01, 0x80,  // LOGICAL_MINIMUM (-32767)
      0x26,                0xFF, 0x7F,  // LOGICAL_MAXIMUM (32767)
      REPORT_SIZE(1),      0x10, //     REPORT_SIZE (16)
      REPORT_COUNT(1),     0x06, //     REPORT_COUNT (6)
      HIDINPUT(1),         0x02, //     INPUT (Data,Var,Abs)
      
  END_COLLECTION(0),         //     END_COLLECTION
   
  USAGE(1),            0x05, // USAGE (Gamepad)
  COLLECTION(1),       0x01, // COLLECTION (Application)
      
      REPORT_ID(1),        0x02, //     Gamepad 2/2
      
      // ------------------------------------------------- Buttons 1 to 8 - unused, but necessary
      USAGE_PAGE(1),       0x09, //     USAGE_PAGE (Button)
      USAGE_MINIMUM(1),    0x01, //     USAGE_MINIMUM (Button 1)
      USAGE_MAXIMUM(1),    0x08, //     USAGE_MAXIMUM (Button 8)
      LOGICAL_MINIMUM(1),  0x00, //     LOGICAL_MINIMUM (0)
      LOGICAL_MAXIMUM(1),  0x01, //     LOGICAL_MAXIMUM (1)
      REPORT_SIZE(1),      0x01, //     REPORT_SIZE (1)
      REPORT_COUNT(1),     0x08, //     REPORT_COUNT (8)
      HIDINPUT(1),         0x02, //     INPUT (Data, Variable, Absolute)
      
      // ------------------------------------------------- 6x16-bit resolution gamepad axes
      USAGE_PAGE(1),       0x01, //     USAGE_PAGE (Generic Desktop)
      USAGE(1),            0x30, //     USAGE (X)
      USAGE(1),            0x31, //     USAGE (Y)
      USAGE(1),            0x32, //     USAGE (Z)
      USAGE(1),            0x33, //     USAGE (rX)
      USAGE(1),            0x34, //     USAGE (rY)
      USAGE(1),            0x35, //     USAGE (rZ)
      0x16,                0x01, 0x80,  // LOGICAL_MINIMUM (-32767)
      0x26,                0xFF, 0x7F,  // LOGICAL_MAXIMUM (32767)
      REPORT_SIZE(1),      0x10, //     REPORT_SIZE (16)
      REPORT_COUNT(1),     0x06, //     REPORT_COUNT (6)
      HIDINPUT(1),         0x02, //     INPUT (Data,Var,Abs)

  END_COLLECTION(0)          //     END_COLLECTION
};

// BLEDevice server callbacks for handling Bluetooth connection / disconnection

class MyCallbacks : public BLEServerCallbacks {
  public :
  
  JRGamepad* JRGamepadInstance;

  MyCallbacks (JRGamepad* o_JRGamepad) {
    this->JRGamepadInstance = o_JRGamepad;
  };

  void onConnect (BLEServer* pServer){
    this->JRGamepadInstance->connected = true;
    
    for (uint32_t g = 0; g < this->JRGamepadInstance->gamepads; g++) {
      BLE2902* desc = (BLE2902*)this->JRGamepadInstance->inputGamepad[g]->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
      desc->setNotifications(true);
    }
  }

  void onDisconnect (BLEServer* pServer){
    this->JRGamepadInstance->connected = false;
       
    for (int g = 0; g < this->JRGamepadInstance->gamepads; g++) {
      BLE2902* desc = (BLE2902*)this->JRGamepadInstance->inputGamepad[g]->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
      desc->setNotifications(false);
    }
  }
};


// Constructor

JRGamepad::JRGamepad (std::string deviceName, std::string deviceManufacturer, uint8_t batteryLevel) : hid(0)
{
  this->connected = false;
  this->deviceName = deviceName;
  this->deviceManufacturer = deviceManufacturer;
  this->batteryLevel = batteryLevel;
}


// Initializer: Begin Bluetooth advertising

void JRGamepad::begin (uint32_t gamepadMode)
{
  this->gamepadMode       = gamepadMode;
  this->gamepads          = gamepadMode > 1 ? 2 : 1;
  this->compatibilityMode = (gamepadMode == 0 || gamepadMode == 2);
  this->deviceName        = _gamepadName[gamepadMode];
    
  xTaskCreate (this->taskServer, "server", 20000, (void *)this, 5, NULL);
}

// End Bluetooth advertising / unimplemented ...never reached
void JRGamepad::end(void)
{
}


// Set gamepad axis values
//
// Note: The axes array must be sized to hold 12 channels. Unused channels
// have to be set to zero before passing the array as parameter.

void JRGamepad::setAxes (int16_t axes[])
{  
  if (! this->connected)
      return;

  uint8_t report[13];

  for (uint32_t g = 0; g < this->gamepads; g++) {
    uint32_t i = 0;
    report[i++] = 0;                        // 8 buttons (1 byte)
    for (uint32_t a = 0; a < 6; a++) {      // 6 axes
      if (this->compatibilityMode) {
        report[i++] = (axes[g*6 + a] >> 8);     // 1 byte for each 8-bit axis
      }
      else {
        report[i++] = axes[g*6 + a];            // 2 bytes for each 16-bit axis
        report[i++] = (axes[g*6 + a] >> 8);
      }
    }
    this->inputGamepad[g]->setValue (report, this->compatibilityMode ? 7 : 13);
    this->inputGamepad[g]->notify();
  }
}


// Initialize BLEDevice with a matching HID report, and start advertising

void JRGamepad::taskServer (void* pvParameter) {
  JRGamepad* JRGamepadInstance = (JRGamepad *) pvParameter;

  BLEDevice::init (JRGamepadInstance->deviceName);
  
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks (new MyCallbacks(JRGamepadInstance));

  JRGamepadInstance->hid = new BLEHIDDevice (pServer);
  for (uint32_t g = 0; g < JRGamepadInstance->gamepads; g++)
    JRGamepadInstance->inputGamepad[g] = JRGamepadInstance->hid->inputReport(g + 1); // REPORT ID from report map
 
  JRGamepadInstance->hid->manufacturer()->setValue (JRGamepadInstance->deviceManufacturer);

  JRGamepadInstance->hid->pnp(0x01,0x02e5,0xabcd,0x0110);
  JRGamepadInstance->hid->hidInfo(0x00,0x01);

  BLESecurity *pSecurity = new BLESecurity();
  pSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND);

  // select the HID report matching the gamepad mode
  if (JRGamepadInstance->gamepadMode == SINGLE_8BIT)
    JRGamepadInstance->hid->reportMap ((uint8_t*) _single8bitHIDreport, sizeof (_single8bitHIDreport));
  else if (JRGamepadInstance->gamepadMode == SINGLE_16BIT)
    JRGamepadInstance->hid->reportMap ((uint8_t*) _single16bitHIDreport, sizeof (_single16bitHIDreport));
  else if (JRGamepadInstance->gamepadMode == DUAL_8BIT)
    JRGamepadInstance->hid->reportMap ((uint8_t*) _dual8bitHIDreport, sizeof (_dual8bitHIDreport));
  else if (JRGamepadInstance->gamepadMode == DUAL_16BIT)
    JRGamepadInstance->hid->reportMap ((uint8_t*) _dual16bitHIDreport, sizeof (_dual16bitHIDreport));

  JRGamepadInstance->hid->startServices();

  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->setAppearance (HID_GAMEPAD);
  pAdvertising->addServiceUUID (JRGamepadInstance->hid->hidService()->getUUID());
  pAdvertising->start();
  JRGamepadInstance->hid->setBatteryLevel (JRGamepadInstance->batteryLevel);

  vTaskDelay (portMAX_DELAY);   // ...run forever
}
