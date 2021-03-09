/* 
   --------- ChannelExtractor task
  
   Initializes the ESP32 RMT module, then enters an endless loop that continuously
   extracts the PPM signal frames.
  
   When a PPM signal is detected for the first time, the ChannelExtractor task will:

    - set axisCount to the number of channels
    - start the GamepadRefresh task & begin BLE Gamepad advertising
    - start the NoiseEstimator task for computing the channel noise threshold
*/

void channelExtractorTask (void *pvParameter) {
  DEBUG_PRINTLN ("");
  DEBUG_PRINTLN ("1. ChannelExtractor: waiting for PPM signal...");
  
  // Configure the ESP32 RMT module for performing continuous decoding of the PPM signal
  rmt_config_t rmt_rx;
  rmt_rx.channel                          = RMT_RX_CHANNEL;
  rmt_rx.gpio_num                         = PPM_PIN;
  rmt_rx.clk_div                          = RMT_CLK_DIV;
  rmt_rx.mem_block_num                    = 1;
  rmt_rx.rmt_mode                         = RMT_MODE_RX;
  rmt_rx.rx_config.filter_en              = true;                               // filter too short pulses / high frequency noise
  rmt_rx.rx_config.filter_ticks_thresh    = 100;                                
  rmt_rx.rx_config.idle_threshold         = PPM_SYNC_MINIMUM * RMT_TICK_US;     // use min sync pulse length as idle threshold
    
  rmt_config (&rmt_rx);
  rmt_driver_install (RMT_RX_CHANNEL, 128, 0);   // channel, ring buffer size, default flags

  RingbufHandle_t rb = NULL;
  rmt_channel_t channel = RMT_RX_CHANNEL;
  rmt_get_ringbuf_handle (channel, &rb);
  rmt_rx_start (channel, true);

  // endless loop
  while (rb) {
    size_t rx_size = 0;

    // Read the next item containing the current PPM frame from the Ringbuffer.
    // The xRingbufferReceive call blocks for a maximum of PPM_MAX_FRAMESIZE microseconds until the
    // next PPM frame is available.
    rmt_item32_t* item = (rmt_item32_t*) xRingbufferReceive (rb, &rx_size, PPM_MAX_FRAMESIZE / 1000);
    if (item) {
      
       channelCount = rx_size / 4 - 1;
      if (channelCount > PPM_MAX_CHANNELS)    // prevent overflows on glitchy PPM signals
        channelCount = PPM_MAX_CHANNELS;
       
      for (int i = 0; i < channelCount && i < PPM_MAX_CHANNELS; i++) {
        //Serial.print ((item+i)->duration1); Serial.print("+"); Serial.print((item+i)->duration0); Serial.print(" ");
        channelValues[i] = (item+i)->duration1 + (item+i)->duration0;
      }
      //Serial.println();
      
      vRingbufferReturnItem (rb, (void*) item);
      item = NULL;
      missingFrames = 0;
      receivedFrames++;
    }
    else
      missingFrames++;

    if (receivedFrames < 10)            // skip the first couple of PPM frames
      continue;                         // ...because things tend to be "glitchy" on startup!
      
    if (channelsAvailable == false) {   // is this the first iteration ? 
      channelsAvailable = true;
   
      // The initial axisCount plays a crucial role for the GamepadRefresh task, as it
      // impacts how it will advertise itself via Bluetooth!
      axisCount = FORCE_CHANNEL_COUNT ? FORCE_CHANNEL_COUNT : channelCount;
  
      // compute the channel noise threshold
      xTaskCreate (noiseEstimatorTask, "noiseEstimatorTask", 1024, NULL, 1, NULL);

      // initialize gamepad
      //xTaskCreate (gamepadRefreshTask, "gamepadRefreshTask", 65536, NULL, 1, NULL);
    }
  }

  Serial.println ("channelExtractorTask exiting : No Ringbuffer returned by RMT !");
  vTaskDelete (NULL);
}
