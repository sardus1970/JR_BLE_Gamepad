/*
   --------- JR BLE Gamepad                                       Fabrizio Sitzia 2021
   
   This sketch makes your RC transmitter (Jumper T8SG, Frsky Taranis, etc.) behave like
   a generic Bluetooth LE Gamepad, so you won't need cables, hubs, receivers or 
   whatever to hook it up to the computer that runs your favorite RC simulator.
   
   It does so by continously extracting channel information from the JR module bay's
   PPM signal, transforming that information into gamepad axis values, and transmitting
   them to the computer via Bluetooth Low Energy.
   
   The sketch is designed for ESP32 boards. It was developped on an "AZ-Delivery ESP32
   D1 Mini", as that particular board fits nicely in a JR module bay enclosure.
   
   To compile the sketch and to upload it to your board, you will need the Arduino IDE
   with the "ESP32 by Espressif Systems" add-on.
    
   The configurable parameters are listed below. They are set to reasonable defaults
   that should work with a wide range of PPM signals and gamepad drivers.
   
   Refer to this project's README.md file for a more in-depth description.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/rmt.h"
#include "JRGamepad.h"
#include "Arduino.h"


// ----- Configurable parameters

// Debug mode: if defined, a lot of stuff gets printed to the Serial Monitor
#undef DEBUG

// The JR module's PPM input is attached to this pin
#define PPM_PIN GPIO_NUM_22

// ESP32 onboard LED pin:
//  - a fast flash (5Hz) indicates PPM signal absence
//  - a slow flash (1Hz) indicates that Bluetooth is not connected
//  - a steady LED indicates that you're ready to go
#define LED_PIN 2

// List of unused GPIO pins to set as outputs in order to reduce high-frequency noise.
//
// If you change PPM_PIN or LED_PIN, you may have to adjust these as well!
const int unusedOutput[] = { 0,1,4,5,12,13,14,15,16,17,18,19,20,21,23,24,25,26,27,28,29,30,31,32,33 };

// Gamepad drivers on different operating systems and brands exhibit varying levels of
// compatibility: Some will only support 8-bit axis values, others will only work
// reliably with refresh rates below 25Hz, etc.
// 
// To accomodate the limitations of different gamepad drivers without having to edit
// the parameters in this sketch and re-flash the board, a "refresh rate channel"
// can be used to set both he desired refresh rate and to switch between 8-bit
// and 16-bit axis resolution modes:
// 
//    - Negative channel values indicate 8-bit "compatibility" mode
//    - Positive values indicate 16-bit "high-resolution" mode
//    - The absolute channel value sets the desired refresh rate
// 
// Example:
// 
// On a transmitter where channel values (conveniently) range from -100 to 100:
// 
//    - A channel value of -30 selects 8-bit "compatibility" mode and a 30 Hz refresh rate
//    - A channel value of 30 selects 16-bit "high-resolution" mode and a 30 Hz refresh rate
//    - A channel value of 70 selects 16-bit "high-resolution" mode and a 70 Hz refresh rate
//
// Note that the 8-bit or 16-bit resolution is set once on startup, and cannot be changed
// without restarting the board. The refresh rate however can be changed on the fly.
// 
// If you do not intend to use a refresh rate channel then set it to zero, and use
// REFRESH_RATE_DEFAULT to set a fixed gamepad refresh rate.
#define REFRESH_RATE_CHANNEL 0
#define REFRESH_RATE_MIN 1
#define REFRESH_RATE_MAX 100
#define REFRESH_RATE_DEFAULT -25

// On transmitters that will always output an 8-channel PPM signal, regardless of the
// number of channels that you actually want, you can specify a lower number of channels
// using FORCE_CHANNEL_COUNT.
// 8 channels will result in a dual gamepad configuration, which some systems (most
// notably Android) have trouble dealing with!
// 
// If you want to use all the channels that are present in the PPM signal, then set
// FORCE_CHANNEL_COUNT to zero (the default)
#define FORCE_CHANNEL_COUNT 6

// NoiseEstimator: scale factor when using max noise as the threshold.
// Good values for NOISE_SCALE are in the range 1.1f to 1.5f
#define NOISE_SCALE 1.2f


// ----- Constants & Macros

// Minimum sync pulse duration, channel pulse center, delta from center and maximum
// framesize (in microseconds).
//
// Note that changing these parameters may require adjusting the RMT parameters
// (ESP32 "Remote Control" module) as well!
// The PPM_SYNC_MINIMUM value is especially critical, as the product
// "PPM_SYNC_MINIMUM * RMT_TICK_US" must fit in a 16-bit hardware register!
#define PPM_SYNC_MINIMUM  2500
#define PPM_PULSE_CENTER  1500
#define PPM_PULSE_DELTA   500
#define PPM_MAX_FRAMESIZE 50000

// Force a gamepad refresh after REFRESH_INACTIVITY_MILLIS milliseconds of inactivity,
// as some gamepad drivers will drop the Bluetooth connection if there is no activity
// for an extended period of time!
#define REFRESH_INACTIVITY_MILLIS 1000

// ESP32 RMT ("Remote control") module settings.
//
// The ESP32 RMT module is used to sample and decode the PPM signal. Only change these
// values if you know the implications (ie. read the datasheets first!)
#define RMT_RX_CHANNEL   RMT_CHANNEL_0          // RMT has 8 channels and we have to pick one
#define RMT_CLK_DIV      4                      // RMT clock divider (80 MHz gets divided by RMT_CLK_DIV)
#define RMT_TICK_US      (80 / RMT_CLK_DIV)     // RMT clock ticks per microsecond

// Gamepad axis resolution (16 bit, only the high byte is used in 8 bit mode)
#define AXIS_RESOLUTION 65536
#define AXIS_MIN        -32767
#define AXIS_MAX        32767

// The minimum & maximum number of PPM channels supported by this implementation
#define PPM_MIN_CHANNELS 2
#define PPM_MAX_CHANNELS 12

// Macros for printing to the Serial Monitor, depending on whether DEBUG is defined
#ifdef DEBUG
 #define DEBUG_PRINT(x)  Serial.print (x)
 #define DEBUG_PRINTLN(x)  Serial.println (x)
#else
 #define DEBUG_PRINT(x)
 #define DEBUG_PRINTLN(x)
#endif


// ----- Globals

// raw RMT tick values of the channels in the most recent PPM frame (set by ChannelExtractor)
static uint32_t channelValues[PPM_MAX_CHANNELS];

// number of channels detected in the most recent PPM frame (set by ChannelExtractor)
static uint32_t channelCount = 0;

// normalized gamepad axis values ranging from AXIS_MIN to AXIS_MAX (set by GamepadRefresh)
static int16_t axisValues[PPM_MAX_CHANNELS]; 
// fixed number of channels set after initial detection of a PPM signal (set by ChannelExtractor)
static uint32_t axisCount;

// total number of received frames, number of missing frames since PPM signal loss (set by GamepadRefresh)
static uint32_t receivedFrames = 0;
static uint32_t missingFrames = 1;

// flags indicating completed initialization of the ChannelExtractor, NoiseEstimator and GamepadRefresh tasks
static bool channelsAvailable  = false;         // ChannelExtractor : true when the PPM signal is detected for the first time
static bool noiseEstimated     = false;         // NoiseEstimator   : true if channel noise estimation is completed
static bool gamepadInitialized = false;         // GamepadRefresh   : true after Bluetooth advertising has started

// the Gamepad BLE implementation for this particular sketch
static JRGamepad gamepad;


// ----- Arduino setup

void setup() {
  // use the lowest CPU frequency we can get away with, to reduce power consumption
  setCpuFrequencyMhz (80);  // ...80 MHz is the minimum for Bluetooth
  Serial.begin (115200);
  Serial.println (" ");
  Serial.println ("========================================================");
  Serial.println ("   JR BLE Gamepad - 2021 Fabrizio Sitzia, Sven Busser   ");
  Serial.println ("========================================================");
 
  // initialize the PPM and LED pins
  pinMode (PPM_PIN, INPUT_PULLUP);
  pinMode (LED_PIN, OUTPUT);
  digitalWrite (LED_PIN, HIGH);

  // set the other IO pins to a defined state, as floating pins can be a potential HF noise source
  pinMode (3, INPUT_PULLUP);
  for (int i = 0; i < (sizeof(unusedOutput) / sizeof(int)); i++) {
    pinMode (unusedOutput[i], OUTPUT);
    digitalWrite (unusedOutput[i], LOW);
  }

  // start the ChannelExtractor task
  xTaskCreate (channelExtractorTask, "channelExtractorTask", 1024, NULL, 1, NULL);
}


// ----- Arduino loop

void loop() {
  
  // blink the ESP32 onboard LED on error conditions
  
  if (missingFrames > 0) {                    // no PPM signal ?
    // blink fast (5Hz)
    digitalWrite (LED_PIN, LOW); delay (100);
    digitalWrite (LED_PIN, HIGH);    
  }
  else if (! gamepad.connected) {            // no BLE Gamepad connection ?
    // blink slowly (1Hz)
    digitalWrite (LED_PIN, LOW); delay (500);
    digitalWrite (LED_PIN, HIGH); delay (400);    
  }
  
  delay (100);
}
