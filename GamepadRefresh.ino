/*
   --------- GamepadRefresh task

   Performs BLE Gamepad HID initialization and sends gamepad refresh notifications
   when required, up to a user-defined refresh rate.
*/


// Convert channelValue (timer ticks) to gamepad axis value

int16_t _channelValueToAxisValue (uint32_t channelValue) {
    
  // use floats during conversion to preserve precision
  float axisValue = UNITY_BUG_WORKAROUND
                  ? ((float) channelValue / RMT_TICK_US - (PPM_PULSE_CENTER - PPM_PULSE_DELTA))
                    * ((float) AXIS_RESOLUTION / 2 / (2 * PPM_PULSE_DELTA))
                  : ((float) channelValue / RMT_TICK_US - PPM_PULSE_CENTER)
                    * ((float) AXIS_RESOLUTION / (2 * PPM_PULSE_DELTA));
  
  // perform boundary checks, as we don't want axis values to rollover/overflow
  float axisMin = UNITY_BUG_WORKAROUND ? 0 : AXIS_MIN;
   
  if (axisValue < axisMin)
    axisValue = axisMin;
  else if (axisValue > AXIS_MAX)
    axisValue = AXIS_MAX;

  return (int16_t) axisValue;
}


// Compute the gamepad refresh rate, which can be set dynamically via the refresh rate channel

uint32_t _getRefreshRate() {
  int16_t axisValue = abs(_channelValueToAxisValue (channelValues[REFRESH_RATE_CHANNEL -1]));
  uint32_t refreshRate = REFRESH_RATE_CHANNEL > 0
    ? ( UNITY_BUG_WORKAROUND
      ? abs (axisValue - AXIS_MAX / 2) * 2 * REFRESH_RATE_MAX / AXIS_MAX
      : axisValue * REFRESH_RATE_MAX / AXIS_MAX )
    : abs(REFRESH_RATE_DEFAULT);

  return refreshRate < REFRESH_RATE_MIN ? REFRESH_RATE_MIN : refreshRate;
}


void gamepadRefreshTask (void *pvParameter) {

  // Begin advertising as a single 7/8/15/16-bit single or dual gamepad, depending
  // on the compatibility modes
  DEBUG_PRINTLN ("");
  DEBUG_PRINT ("3. GamepadRefresh: axisCount = "); DEBUG_PRINTLN (axisCount);
  DEBUG_PRINT ("   Unity bug workaround under Windows is "); DEBUG_PRINTLN (UNITY_BUG_WORKAROUND ? "active" : "inactive");

  if (! REFRESH_RATE_CHANNEL) {    
    DEBUG_PRINT ("   No refresh rate channel: using REFRESH_RATE_DEFAULT = ");
    DEBUG_PRINTLN (REFRESH_RATE_DEFAULT);
  }
 
  int16_t val = (! REFRESH_RATE_CHANNEL || REFRESH_RATE_CHANNEL > axisCount)
              ? REFRESH_RATE_DEFAULT
              : (UNITY_BUG_WORKAROUND ? _channelValueToAxisValue (channelValues[REFRESH_RATE_CHANNEL - 1]) * 2 - AXIS_MAX
                                      : _channelValueToAxisValue (channelValues[REFRESH_RATE_CHANNEL - 1]));
     
  DEBUG_PRINT ( val < 0 ? "   Negative refresh rate --> 8-bit gamepad (compatibility mode) @ "
                        : "   Positive refresh rate --> 16-bit gamepad @ ");   
  DEBUG_PRINT (_getRefreshRate()); DEBUG_PRINTLN (" Hz");
  
  gamepadInitialized = true;
  DEBUG_PRINTLN ("   Waiting for Bluetooth connection...");
  DEBUG_PRINTLN (" ");

  gamepad.begin (
    (val < 0 ? 0 : 1)                  // 8bit or 16bit axis values ?
    + (axisCount > 6 ? 2 : 0)          // single or dual gamepad ?
    + (UNITY_BUG_WORKAROUND ? 4 : 0)   // positive axis values only ?
  );

  // endless loop running at the user-defined Gamepad refresh rate
  uint32_t lastRefresh = 0;
  while (gamepadInitialized) {
    
    // Compute refresh rate, as the user may change it dynamically via the refresh rate channel
    uint32_t refreshRate = _getRefreshRate();
    uint32_t now = xTaskGetTickCount() / portTICK_PERIOD_MS;

    if ( gamepad.connected                                            // BLE connection established ?
          && (changeDetected()                                           // and user activity detected ?
             || (now - lastRefresh) > REFRESH_INACTIVITY_MILLIS) ) {     // or last refresh older than REFRESH_INACTIVITY_MILLIS ?

      // convert timer ticks to gamepad axis values
      for (int i = 0; i < axisCount; i++) {
        axisValues[i] = _channelValueToAxisValue (channelValues[i]);
        // DEBUG_PRINT (channelValues[i]); DEBUG_PRINT ("/");
        DEBUG_PRINT (gamepad.compatibilityMode ? (axisValues[i] >> 8) : axisValues[i]); DEBUG_PRINT (" ");
      }
      DEBUG_PRINT ("/ "); DEBUG_PRINT (refreshRate); DEBUG_PRINTLN (" Hz");
                 
      // send BLE HID notification and reset the lastRefresh timestamp
      gamepad.setAxes (axisValues);
      lastRefresh = now;
    }
    
    vTaskDelay ((1000 / refreshRate) / portTICK_PERIOD_MS);
  }
  
  Serial.println ("gamepadRefreshTask exiting : gamepadInitialized is false");
  vTaskDelete (NULL);
}
