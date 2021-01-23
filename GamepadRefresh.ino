/*
   --------- GamepadRefresh task

   Performs BLE Gamepad HID initialization and sends gamepad refresh notifications
   when required, up to a user-defined refresh rate.
*/


// Convert channelValue (timer ticks) to gamepad axis value

int16_t _channelValueToAxisValue (uint32_t channelValue) {
    
  // use floats during conversion to preserve precision
  float axisValue = ((float) channelValue / RMT_TICK_US - PPM_PULSE_CENTER)
                  * ((float) AXIS_RESOLUTION / (2 * PPM_PULSE_DELTA));
  
  // perform boundary checks, as we don't want axis values to rollover/overflow
  if (axisValue < AXIS_MIN)
    axisValue = AXIS_MIN;
  else if (axisValue > AXIS_MAX)
    axisValue = AXIS_MAX;

  return (int16_t) axisValue;
}


// Compute the gamepad refresh rate, which can be set dynamically via the refresh rate channel

uint32_t _getRefreshRate() {
  return axisCount >= REFRESH_RATE_CHANNEL
    ? REFRESH_RATE_MIN + abs(_channelValueToAxisValue (channelValues[REFRESH_RATE_CHANNEL -1])) * REFRESH_RATE_MAX / AXIS_MAX
    : REFRESH_RATE_DEFAULT;
}


void gamepadRefreshTask (void *pvParameter) {

  // Begin advertising as a single 8-bit "compatibility" mode gamepad, or a single
  // 16-bit gamepad, or a dual 16-bit gamepad, depending on the availability and
  // the value of the refresh rate channel:
  
  DEBUG_PRINTLN ();
  DEBUG_PRINT ("GamepadRefresh: axisCount = ");
  DEBUG_PRINTLN (axisCount);
   
  if (axisCount < REFRESH_RATE_CHANNEL) {
    
    DEBUG_PRINT ("No configuration channel --> 16-bit gamepad @ ");
    DEBUG_PRINT (REFRESH_RATE_DEFAULT);
    DEBUG_PRINTLN (" Hz default refresh rate");
    gamepad.begin (axisCount);
  }
  else {
    int16_t val = _channelValueToAxisValue (channelValues[REFRESH_RATE_CHANNEL - 1]);
    DEBUG_PRINT ( val < 0 ? "Negative refresh rate --> 8-bit gamepad (compatibility mode) @ "
                          : "Positive refresh rate --> 16-bit gamepad @ " );
    DEBUG_PRINT (_getRefreshRate());
    DEBUG_PRINTLN (" Hz");
       
    gamepad.begin (val < 0 ? 0 : axisCount);
  }
  gamepadInitialized = true;
  DEBUG_PRINTLN ("Waiting for Bluetooth connection...");

  // endless loop running at the user-defined Gamepad refresh rate
  uint32_t lastRefresh = 0;
  while (gamepadInitialized) {
    
    // Compute refresh rate, as the user may change it dynamically via the refresh rate channel
    uint32_t refreshRate = _getRefreshRate();
    uint32_t now = xTaskGetTickCount() / portTICK_PERIOD_MS;

    if ( gamepad.connected                                                // BLE connection established ?
         && channelCount == axisCount                           // and channelCount is unchanged ?
         && (changeDetected()                                             // and user activity detected ?
             || (now - lastRefresh) > REFRESH_INACTIVITY_MILLIS) ) {     // or last refresh older than REFRESH_INACTIVITY_MILLIS ?

      // convert timer ticks to gamepad axis values

      for (int i = 0; i < axisCount; i++) {
        axisValues[i] = _channelValueToAxisValue (channelValues[i]);
        DEBUG_PRINT (axisValues[i]);
        DEBUG_PRINT (" ");
      }
      DEBUG_PRINT ("/ ");
      DEBUG_PRINT (refreshRate);
      DEBUG_PRINTLN (" Hz");
                  
      // send BLE HID notification and reset the lastRefresh timestamp
      gamepad.setAxes (axisValues);
      lastRefresh = now;
    }
    
    vTaskDelay ((1000 / refreshRate) / portTICK_PERIOD_MS);
  }
  
  Serial.println ("gamepadRefreshTask exiting : gamepadInitialized is false");
  vTaskDelete (NULL);
}
