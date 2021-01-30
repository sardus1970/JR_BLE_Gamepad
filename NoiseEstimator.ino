 /*
   --------- NoiseEstimator task
  
   We only want to perform a gamepad refresh when the channel values change as the result of
   user input, not as a result of channel background noise!
  
   Therefore the channel noise is sampled during the first second after PPM signal detection
   and a noise threshold is computed as follows:
  
    - The lowest (_min) and highest (_max) value of every channel is sampled for 1 second
    - Then the difference (_diff) between the _min and _max channel values is computed
    - The noise threshold value is calculated by taking either the median difference or
      the maximum difference, depending on the measured noise amplitude:
      
      The maximum difference is suitable for PPM signals with little, low-amplitude noise
      that is undetectable in an 8-bit signal.
      The median difference is best for noisier PPM signals that feature irregular,
      high-amplitude noise peaks that would appear in an 8-bit signal.
  
   The changeDetected() function, which is invoked by the GamepadRefresh task, compares
   the current channel values with the previous reference channel values (_ref), and returns
   true if the difference exceeds the noise threshold.
*/

uint32_t _min[PPM_MAX_CHANNELS];   // lowest sampled channel values
uint32_t _max[PPM_MAX_CHANNELS];   // highest sampled channel values
uint32_t _diff[PPM_MAX_CHANNELS];  // their difference
uint32_t _ref[PPM_MAX_CHANNELS];   // reference channel values

uint32_t _noiseThreshold;          // the computed noise threshold


// Comparator for the C-language's qsort function (We must sort the _diff array
// in order to find the median value)

int _cmpfunc (const void * a, const void * b) {
   return ( *(uint32_t*)a - *(uint32_t*)b );
}


void noiseEstimatorTask (void *pvParameter) {

  // 1. Initialize data structures

  DEBUG_PRINTLN ("NoiseEstimator: sampling noise");
  
  for (int i = 0; i < axisCount; i++) {
    _min[i] = (PPM_PULSE_CENTER + PPM_PULSE_DELTA) * RMT_TICK_US;
    _max[i] = (PPM_PULSE_CENTER - PPM_PULSE_DELTA) * RMT_TICK_US;
    _ref[i] = _diff[i] = 0;
  }

    
  // 2. Sample noise during 1 second

  // iterate 100 times with a 10 millisecond pause between each iteration
  for (int l = 0; l < 100; l++) {
    DEBUG_PRINT ("*");

    // remember the minimum and maximum value of every channel
    for (int i = 0; i < axisCount; i++) {
      if (channelValues[i] < _min[i])
        _min[i] = channelValues[i];
      if (channelValues[i] > _max[i])
        _max[i] = channelValues[i];
    }
    vTaskDelay (10 / portTICK_PERIOD_MS);
  }


  // 3. Finished sampling: Compute the noise threshold

  // calculate the differences between the min and max channel values
  for (int i = 0; i < axisCount; i++)
    _diff[i] = _max[i] - _min[i];

  // ...sort them
  qsort (_diff, axisCount, sizeof(uint32_t), _cmpfunc);
  
  DEBUG_PRINTLN();
  DEBUG_PRINT ("diff : ");
  for (int i = 0; i < axisCount; i++) {
    DEBUG_PRINT (_diff[i]);
    DEBUG_PRINT (" ");
  }
  DEBUG_PRINTLN();

  // maximum measured noise amplitude
  uint32_t maxNoiseAmplitude = _diff[axisCount - 1];

  // if there is a lot of noise (maximum noise amplitude is detectable at 8-bit resolution)
  // then use the median difference as the noise threshold value, otherwise use the maxNoise value:
  
  if (maxNoiseAmplitude > (RMT_TICK_US * 1000 / 256)) {
    
    // use the median noise difference (very noisy/unstable PPM signal)
    DEBUG_PRINT ("Noise threshold (median) = ");

    _noiseThreshold = (axisCount & 0x00000001) == 1  // even or odd number of channels ?
      ? _diff[axisCount / 2]
      : (_diff[axisCount / 2 - 1] + _diff[axisCount / 2]) / 2;
  }
  else {
    
    // use the max noise measurement (stable PPM signal with little noise)
    DEBUG_PRINT ("Noise threshold (max) = ");
    _noiseThreshold = maxNoiseAmplitude;
  }
  DEBUG_PRINTLN (_noiseThreshold);

  // terminate the NoiseEstimator task
  noiseEstimated = true;
  vTaskDelete (NULL);
}


// Detect if channel value changes have resulted from user input by comparing the
// difference of the current channel values with the last reference values,
// against the noise threshold.

bool changeDetected() {

  // check each channel value against the noise threshold
  for (int i = 0; i < axisCount; i++)
    if (abs(channelValues[i] - _ref[i]) > _noiseThreshold) {
      
      // set the current channel values as the new reference values
      for (int j = 0; j < axisCount; j++)
        _ref[j] = channelValues[j];
               
      return true;
    }
  return false;
}
