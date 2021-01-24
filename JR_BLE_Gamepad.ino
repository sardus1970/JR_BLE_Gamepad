/*
   --------- JR BLE Gamepad                                       Fabrizio Sitzia 2021
   
   This sketch makes your RC transmitter (Jumper T8SG, Frsky Taranis, etc.) behave like
   a generic Bluetooth LE Gamepad, so you won't need cables, hubs, receivers or 
   whatever to hook it up to the computer that runs your favorite RC simulator.
   
   It does so by continously extracting channel information from the JR module bay's
   PPM signal, transforming that information into gamepad axis values, and transmitting
   them via Bluetooth LE.
   
   The sketch is designed for ESP32 boards. It was developped for an "AZ Delivery ESP32
   D1 Mini", as that particular board fits nicely in a JR module bay enclosure.
   
   To compile the sketch and to upload it to your board, you will need the Arduino IDE
   with the "ESP32 by Espressif Systems" add-on.
    
   The configurable parameters are listed below. They are set to reasonable defaults
   that should work with a a wide range of PPM signals and gamepad drivers.
   
   Refer to the README.md file for a more in-depth description.
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

// Gamepad drivers on different operating systems and brands exhibit varying levels of
// compatibility: Some will only support 8-bit axis values, others will only work
// reliably with refresh rates below 25Hz, etc.
// 
// To accomodate the limitations of different gamepad drivers without having to edit
// the parameters in this sketch and re-flash the board, a "refresh rate channel"
// is used to both set the desired refresh rate, and to switch between 8-bit
// and 16-bit resolution modes:
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
// Note that the 8-bit or 16-bit mode is set once on startup, and cannot be changed
// without restarting the board. The refresh rate however can be changed on the fly.
// 
// If you do not intend to use a refresh rate channel, then set it to a number
// larger than PPM_MAX_CHANNELS, and use REFRESH_RATE_DEFAULT to set a fixed
// gamepad refresh rate (expressed in Hz)
#define REFRESH_RATE_CHANNEL 6
#define REFRESH_RATE_MIN 1
#define REFRESH_RATE_MAX 100
#define REFRESH_RATE_DEFAULT 25

// Force a gamepad refresh after REFRESH_INACTIVITY_MILLIS milliseconds of inactivity,
// as some gamepad drivers will drop the Bluetooth connection if there is no activity
// for an extended period of time!
#define REFRESH_INACTIVITY_MILLIS 1000

// Ideally, the gamepad positions should only change due to user input. But in
// the real world they change constantly due to all kinds of noise (interference,
// quantization noise, software rounding errors, your dog farting on the couch...)
//
// During the first second after PPM signal detection, this sketch therefore tries
// to estimate a noise threshold to differentiate user input from background noise.
//
// The NOISE_MEDIAN parameter controls the noise threshold selection:
// - If your PPM signal is very noisy/glitchy, then you should set it to 1
// - If your PPM signal has little noise, then you should set it to 0
//
// Optimally, when not moving any sticks, the gamepad positions should only be
// updated every REFRESH_INACTIVITY_MILLIS milliseconds - but they should react
// at the slightest touch of the sticks!
#define NOISE_MEDIAN 0

// The JR module's PPM input is attached to this pin
#define PPM_PIN GPIO_NUM_22

// ESP32 onboard LED pin:
//  - a fast flash (5Hz) indicates PPM signal absence
//  - a slow flash (1Hz) indicates that Bluetooth is not connected
//  - a steady LED indicates that you're ready to go
#define LED_PIN 2


// ----- Constants & Macros

// Minimum sync pulse duration, channel pulse center, delta from center and maximum
// framesize (in microseconds)
//
// Note that changing these parameters may require adjusting the RMT parameters
// (RMT = ESP32 "Remote Control" module) as well!
// The PPM_SYNC_MINIMUM value is especially critical, as the product
// "PPM_SYNC_MINIMUM * RMT_TICK_US" must fit in a 16-bit hardware register!
#define PPM_SYNC_MINIMUM  2500
#define PPM_PULSE_CENTER  1500
#define PPM_PULSE_DELTA   500
#define PPM_MAX_FRAMESIZE 50000

// ESP32 RMT ("Remote control") module settings:
//
// On an ESP32 this is definitely the way to go if you want to consistentyl sample
// PPM pulse widths with high resolution.
// (...I tried to use GPIO edge interrupts in combination with timers, but the
// interrupt latencies ranged from 2 to 8 microseconds)
#define RMT_RX_CHANNEL   RMT_CHANNEL_0
#define RMT_CLK_DIV      4                      // RMT clock divider (80 MHz gets divided by RMT_CLK_DIV)
#define RMT_TICK_US      (80 / RMT_CLK_DIV)     // RMT clock ticks per microsecond

// The minimum & maximum number of channels allowed in a PPM frame
#define PPM_MIN_CHANNELS 2
#define PPM_MAX_CHANNELS 12

// Gamepad axis resolution (16 bit, only the high byte is used in 8 bit mode)
#define AXIS_RESOLUTION 65536
#define AXIS_MIN        -32767
#define AXIS_MAX        32767

// List of unused GPIO pins to set as outputs (reduce noise)
const int unusedOutput[] = { 0,1,4,5,12,13,14,15,16,17,18,19,20,21,23,24,25,26,27,28,29,30,31,32,33 };

// Macros for printing to the Serial Monitor, depending on whether DEBUG is defined
#ifdef DEBUG
 #define DEBUG_PRINT(x)  Serial.print (x)
 #define DEBUG_PRINTLN(x)  Serial.println (x)
#else
 #define DEBUG_PRINT(x)
 #define DEBUG_PRINTLN(x)
#endif


// ----- Globals

// raw RMT tick channel values of the most recent PPM frame
static uint32_t channelValues[PPM_MAX_CHANNELS];
// number of channels detected in the most recent PPM frame
static uint32_t channelCount = 0;

// normalized gamepad axis values ranging from AXIS_MIN to AXIS_MAX
static int16_t axisValues[PPM_MAX_CHANNELS]; 
// fixed number of channels set after initial detection of a PPM signal
static uint32_t axisCount;

// total number of received frames, number of missing frames since PPM signal loss
static uint32_t receivedFrames = 0;
static uint32_t missingFrames = 1;

// flags indicating completed initialization of the ChannelExtractor, NoiseEstimator and GamepadRefresh tasks
static bool channelsAvailable  = false;         // true when the PPM signal is detected for the first time
static bool noiseEstimated     = false;         // true if channel noise estimation is completed
static bool gamepadInitialized = false;         // true after Bluetooth advertising has started

// the Gamepad BLE implementation for this particular sketch
static JRGamepad gamepad;


// ----- Arduino setup

void setup() {
  // use the lowest CPU frequency we can get away with
  setCpuFrequencyMhz (80);    // ...80MHz is the minimum for Bluetooth
  Serial.begin (115200);

  Serial.println ();
  Serial.println ("JR BLE Gamepad - 2021 Fabrizio Sitzia");
  Serial.println ();
 
  // initialize the PPM and LED pins
  pinMode (PPM_PIN, INPUT_PULLUP);
  pinMode (LED_PIN, OUTPUT);
  digitalWrite (LED_PIN, HIGH);

  // set the other IO pins to a defined state, as floating pins are a potential noise source
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
  
  if (missingFrames > 0) {                    // No PPM signal ?
    // blink fast (5Hz)
    digitalWrite (LED_PIN, LOW); delay (100);
    digitalWrite (LED_PIN, HIGH);    
  }
  else if (! gamepad.connected) {            // No BLE Gamepad connection ?
    // blink slowly (1Hz)
    digitalWrite (LED_PIN, LOW); delay (500);
    digitalWrite (LED_PIN, HIGH); delay (400);    
  }
  
  delay (100);
}
