#include "Arduino.h"

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include "BLE2902.h"
#include "BLEHIDDevice.h"
#include "HIDTypes.h"
#include "HIDKeyboardTypes.h"
#include <driver/adc.h>
#include "sdkconfig.h"

#include "JRGamepad.h"


// HID reports:
//
// For maximum compatibility with generic gamepad drivers, up to 6 PPM channels are mapped as follows:
//
//  - The first 4 channels are mapped to the X, Y, Z and rZ axes that are typically used for the
//    axes of two analog joysticks
//  - The last 2 channels are mapped to the rX and rY axes typically used for the left and right analog
//    triggers.
//
// If more than 6 PPM channels are required, they will be mapped as two gamepads in a composite HID
// report. This may work or not depending on the particular gamepad driver implementation.
// For instance, this works fine on Mac OS and Windows, but not on Android!
// (Android's gamepad driver apparently does not support composite gamepad HID reports. That's odd
// because composite HID reports are used on that system for mapping keyboards with an integrated
// trackpad)
//
// The following HID report structures are used for the various gamepad scenarios:
//
//  _compatibleHidReport is used for a single gamepad with legacy 8-bit axis resolution
//  _singleHidReport is used for single gamepad with 16-bit axis resolution
//  _compositeHidReport is used for a dual gamepad with 16-bit axis resolution

static const uint8_t _compatibleHidReport[] = {
  USAGE_PAGE(1),                0x01, // USAGE_PAGE (Generic Desktop)
  USAGE(1),                     0x05, // USAGE (Gamepad)
  COLLECTION(1),                0x01, // COLLECTION (Application)
    USAGE(1),                   0x01, //   USAGE (Pointer)
    COLLECTION(1),              0x00, //   COLLECTION (Physical)     
      REPORT_ID(1),             0x01, //     Report ID 1
      
      // ------------------------------------------------- Buttons 1 to 8 - unused, but necessary
      USAGE_PAGE(1),            0x09, //     USAGE_PAGE (Button)
      USAGE_MINIMUM(1),         0x01, //     USAGE_MINIMUM (Button 1)
      USAGE_MAXIMUM(1),         0x08, //     USAGE_MAXIMUM (Button 8)
      LOGICAL_MINIMUM(1),       0x00, //     LOGICAL_MINIMUM (0)
      LOGICAL_MAXIMUM(1),       0x01, //     LOGICAL_MAXIMUM (1)
      REPORT_SIZE(1),           0x01, //     REPORT_SIZE (1)
      REPORT_COUNT(1),          0x08, //     REPORT_COUNT (8)
      HIDINPUT(1),              0x02, //     INPUT (Data, Variable, Absolute)
        
      // ------------------------------------------------- X/Y position, Z/rZ position (16 bit resolution)
      USAGE_PAGE(1),            0x01, //     USAGE_PAGE (Generic Desktop)
      COLLECTION(1),            0x00, //     COLLECTION (Physical)
        USAGE(1),               0x30, //       USAGE (X)
        USAGE(1),               0x31, //       USAGE (Y)
        USAGE(1),               0x32, //       USAGE (Z)
        USAGE(1),               0x35, //       USAGE (rZ)
        LOGICAL_MINIMUM(1),     0x81, //       LOGICAL_MINIMUM (-127)
        LOGICAL_MAXIMUM(1),     0x7f, //       LOGICAL_MAXIMUM (127)
        REPORT_SIZE(1),         0x08, //       REPORT_SIZE (8)
        REPORT_COUNT(1),        0x04, //       REPORT_COUNT (4)
        HIDINPUT(1),            0x02, //       INPUT (Data,Var,Abs)          

        // ------------------------------------------------- Triggers
        USAGE(1),               0x33, //       USAGE (rX) Left Trigger
        USAGE(1),               0x34, //       USAGE (rY) Right Trigger
        LOGICAL_MINIMUM(1),     0x81, //       LOGICAL_MINIMUM (-127)
        LOGICAL_MAXIMUM(1),     0x7f, //       LOGICAL_MAXIMUM (127)
        REPORT_SIZE(1),         0x08, //       REPORT_SIZE (8)
        REPORT_COUNT(1),        0x02, //       REPORT_COUNT (2)
        HIDINPUT(1),            0x02, //       INPUT (Data, Variable, Absolute) ;4 bytes (X,Y,Z,rZ)
      END_COLLECTION(0),              //     END_COLLECTION (Physical)

    END_COLLECTION(0),                //   END_COLLECTION (Physical)
  END_COLLECTION(0)                   // END_COLLECTION (Application)
};

static const uint8_t _singleHidReport[] = {
  USAGE_PAGE(1),       0x01, // USAGE_PAGE (Generic Desktop)
  
  USAGE(1),            0x05, // USAGE (Gamepad)
  COLLECTION(1),       0x01, // COLLECTION (Application)
    USAGE(1),            0x01, // USAGE (Pointer)
    COLLECTION(1),       0x00, // COLLECTION (Physical)   
      REPORT_ID(1),        0x01, //     Report ID 1
      
      // ------------------------------------------------- Buttons 1 to 8 - unused, but necessary
      USAGE_PAGE(1),       0x09, //     USAGE_PAGE (Button)
      USAGE_MINIMUM(1),    0x01, //     USAGE_MINIMUM (Button 1)
      USAGE_MAXIMUM(1),    0x08, //     USAGE_MAXIMUM (Button 8)
      LOGICAL_MINIMUM(1),  0x00, //     LOGICAL_MINIMUM (0)
      LOGICAL_MAXIMUM(1),  0x01, //     LOGICAL_MAXIMUM (1)
      REPORT_SIZE(1),      0x01, //     REPORT_SIZE (1)
      REPORT_COUNT(1),     0x08, //     REPORT_COUNT (8)
      HIDINPUT(1),         0x02, //     INPUT (Data, Variable, Absolute)
        
      // ------------------------------------------------- X/Y position, Z/rZ position (16 bit resolution)
      USAGE_PAGE(1),       0x01, //     USAGE_PAGE (Generic Desktop)
      COLLECTION(1),       0x00, //     COLLECTION (Physical)
        USAGE(1),          0x30, //     USAGE (X)
        USAGE(1),          0x31, //     USAGE (Y)
        USAGE(1),          0x32, //     USAGE (Z)
        USAGE(1),          0x35, //     USAGE (rZ)
        0x16,              0x01, 0x80,  // LOGICAL_MINIMUM (-32767)
        0x26,              0xFF, 0x7F,  // LOGICAL_MAXIMUM (32767)
        REPORT_SIZE(1),    0x10, //     REPORT_SIZE (16)
        REPORT_COUNT(1),   0x04, //     REPORT_COUNT (4)
        HIDINPUT(1),       0x02, //     INPUT (Data,Var,Abs)          

        // ------------------------------------------------- Triggers
        USAGE(1),          0x33, //     USAGE (rX) Left Trigger
        USAGE(1),          0x34, //     USAGE (rY) Right Trigger
        0x16,              0x01, 0x80,  // LOGICAL_MINIMUM (-32767)
        0x26,              0xFF, 0x7F,  // LOGICAL_MAXIMUM (32767)
        REPORT_SIZE(1),    0x10, //     REPORT_SIZE (16)
        REPORT_COUNT(1),   0x02, //     REPORT_COUNT (2)
        HIDINPUT(1),       0x02, //     INPUT (Data, Variable, Absolute) ;4 bytes (X,Y,Z,rZ)
      END_COLLECTION(0),         //     END_COLLECTION

    END_COLLECTION(0),         //     END_COLLECTION
  END_COLLECTION(0)          //     END_COLLECTION
};

static const uint8_t _compositeHidReport[] = {
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
      
      // ------------------------------------------------- 6 PPM channels - mapped to 16-bit resolution gamepad axes
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
      
      REPORT_ID(1),        0x02, //     Gamepad 2
      
      // ------------------------------------------------- Buttons 1 to 8 - unused, but necessary
      USAGE_PAGE(1),       0x09, //     USAGE_PAGE (Button)
      USAGE_MINIMUM(1),    0x01, //     USAGE_MINIMUM (Button 1)
      USAGE_MAXIMUM(1),    0x08, //     USAGE_MAXIMUM (Button 8)
      LOGICAL_MINIMUM(1),  0x00, //     LOGICAL_MINIMUM (0)
      LOGICAL_MAXIMUM(1),  0x01, //     LOGICAL_MAXIMUM (1)
      REPORT_SIZE(1),      0x01, //     REPORT_SIZE (1)
      REPORT_COUNT(1),     0x08, //     REPORT_COUNT (8)
      HIDINPUT(1),         0x02, //     INPUT (Data, Variable, Absolute)
      
      // ------------------------------------------------- 6 PPM channels - mapped to 16-bit resolution gamepad axes
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
    BLE2902* desc = (BLE2902*)this->JRGamepadInstance->inputGamepad1->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
    desc->setNotifications(true);
    if (this->JRGamepadInstance->dualGamepad) {
        desc = (BLE2902*)this->JRGamepadInstance->inputGamepad2->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
        desc->setNotifications(true);
    }
  }

  void onDisconnect (BLEServer* pServer){
    this->JRGamepadInstance->connected = false;
    BLE2902* desc = (BLE2902*)this->JRGamepadInstance->inputGamepad1->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
    desc->setNotifications(false);
    if (this->JRGamepadInstance->dualGamepad) {
        desc = (BLE2902*)this->JRGamepadInstance->inputGamepad2->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
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
//
// Different HID report descriptors will be chosen depending on channelCount:
//
//      channelCount == 0  : one "compatible" gamepad with 8 bit axis resolution
//      channelCount <= 6  : one gamepad with 16 bit axis resolution
//      channelCount > 6   : two gamepads with 16 bit axis resolution

void JRGamepad::begin (int channelCount)
{
  this->dualGamepad = false;
  this->compatibilityMode = false;

  if (channelCount == 0) {
  	this->deviceName = "JR Gamepad 8";
  	this->compatibilityMode = true;
  }
  else if (channelCount > 6) {
  	this->deviceName = "JR Gamepad 2x16";
  	this->dualGamepad = true;
  }
    
  xTaskCreate (this->taskServer, "server", 20000, (void *)this, 5, NULL);
}

// End Bluetooth advertising / comms (unimplemented ...never reached)
void JRGamepad::end(void)
{
}


// Set gamepad axes values
//
// The array passed as parameter must always have a size of AXIS_COUNT (12 axes) regardless
// of the number of effectively used channels.
// Unused channels / axes should be set to a constant value, such as zero.

void JRGamepad::setAxes (int16_t axes[])
{  
    if (! this->connected)
        return;

    uint8_t report0[7];     // compatibility mode HID report
    uint8_t report1[13];    // single gamepad HID report
    uint8_t report2[13];    // composite gamepad HID report
    int i = 0;

    // Gamepad 1 in 8 bit resolution compatibility mode
    if (this->compatibilityMode) {
        report0[i++] = 0;               // 8 buttons
        for (int a = 0; a < 6; a++)     // 6 axes
            report0[i++] = (axes[a] >> 8);
        this->inputGamepad1->setValue (report0, sizeof (report0));
        this->inputGamepad1->notify();
    }
    
    else {
   	    // Gamepad 1 with 16 bit resolution axes      
        report1[i++] = 0;               // 8 buttons
        for (int a = 0; a < 6; a++) {   // 6 axes
            report1[i++] = axes[a];
            report1[i++] = (axes[a] >> 8);
        }
        this->inputGamepad1->setValue (report1, sizeof (report1));
	    this->inputGamepad1->notify();

    	// Gamepad 2 with 16 bit resolution axes
    	if (this->dualGamepad) {
       		i = 0;
        	report2[i++] = 0;               // 8 buttons
        	for (int a = 6; a < 12; a++) {  // 6 axes
            	report2[i++] = axes[a];
            	report2[i++] = (axes[a] >> 8);
        	}  
        	this->inputGamepad2->setValue (report2, sizeof (report2));
        	this->inputGamepad2->notify();
        }
    }
}


// Initialize BLEDevice with a matching HID report, and start advertising

void JRGamepad::taskServer (void* pvParameter) {
  JRGamepad* JRGamepadInstance = (JRGamepad *) pvParameter;
  
  BLEDevice::init (JRGamepadInstance->deviceName);
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks (new MyCallbacks(JRGamepadInstance));

  JRGamepadInstance->hid = new BLEHIDDevice (pServer);
  JRGamepadInstance->inputGamepad1 = JRGamepadInstance->hid->inputReport(1); // <-- input REPORTID from report map
  if (JRGamepadInstance->dualGamepad)
    JRGamepadInstance->inputGamepad2 = JRGamepadInstance->hid->inputReport(2); // <-- input REPORTID from report map
 
  JRGamepadInstance->hid->manufacturer()->setValue (JRGamepadInstance->deviceManufacturer);

  JRGamepadInstance->hid->pnp(0x01,0x02e5,0xabcd,0x0110);
  JRGamepadInstance->hid->hidInfo(0x00,0x01);

  BLESecurity *pSecurity = new BLESecurity();
  pSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND);

  // select the HID report matching compatibilityMode and dualGamepad
  if (JRGamepadInstance->compatibilityMode)
    JRGamepadInstance->hid->reportMap ((uint8_t*) _compatibleHidReport, sizeof(_compatibleHidReport));      
  else if (JRGamepadInstance->dualGamepad)
    JRGamepadInstance->hid->reportMap ((uint8_t*) _compositeHidReport, sizeof(_compositeHidReport));
  else
    JRGamepadInstance->hid->reportMap ((uint8_t*) _singleHidReport, sizeof(_singleHidReport));
  
  JRGamepadInstance->hid->startServices();

  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->setAppearance(HID_GAMEPAD);
  pAdvertising->addServiceUUID (JRGamepadInstance->hid->hidService()->getUUID());
  pAdvertising->start();
  JRGamepadInstance->hid->setBatteryLevel (JRGamepadInstance->batteryLevel);

  vTaskDelay (portMAX_DELAY);   // ...run forever
}
