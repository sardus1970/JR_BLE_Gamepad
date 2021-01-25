# JR BLE Gamepad
2021- Fabrizio Sitzia 

## Introduction

This ESP32 sketch turns your RC transmitter into a generic Bluetooth LE Gamepad so you can run your favorite RC simulator wirelessly.

![Introduction image](data/images/intro.jpg)

The goal was to get rid of any USB-C hubs, dongles, cables, etc. when running a simulator on the above laptop, while preserving the low latency and high resolution of a wired USB connection.

A gamepad emulation was chosen because most operating systems and RC sims support them out of the box, without having to install any additional drivers.

The transmitter you see on the above photo (a *Jumper T8SG v2 plus*) features a "JR Module" bay on the back. In the RC world this is some kind of de-facto "standard" for extending a transmitter's functionality. It was a natural choice to try and make this project fit into that module.

By the time of this writing, the module has been tested successfully under Mac OS (Catalina and Big Sur), varios Android flavours and Windows 10.


## Building the JR module

### What you will need

- an ESP32 board that fits into a JR module enclosure (such as the *AZ-Delivery ESP32 D1 Mini* used by the author)
- linear 3.3V regulator or 3.3V step-down buck regulator (the latter is preferred)
- 10µF electrolytic capacitor
- 3 resistors (14kΩ, 44kΩ and 330Ω) and a NPN transistor (BC547 or similar)
- a small piece of stripboard, a few wires
- 5-pin female connector
- on/off switch
- skills with a soldering iron, dremel and a glue gun

### Testing the ESP32 board

It is a good idea to first test your ESP32 board before building anything.

Launch the Arduino IDE and edit the main `JR_BLE_Gamepad` sketch: Check if the `LED_PIN` and `PPM_PIN` parameters fit your board, and `#define DEBUG` to have  verbose information appear in the Serial Monitor. Connect the ESP board to the computer's USB port, then compile & upload the sketch.

If all goes well, the following output should appear in the Serial Monitor:

	JR BLE Gamepad - 2021 Fabrizio Sitzia
	
	initializing ChannelExtractor: waiting for PPM signal...

You will also notice that the blue onboard LED is now blinking fast, indicating that there is no PPM signal.

The next step assumes that your transmitter outputs a 3.3 volt PPM signal on pin 5 in the JR module bay.
It is **IMPORTANT** to check that the PPM signal voltage does not exceed 3.3V, as higher voltages might damage the ESP32 or even your transmitter. Skip to the next section if that's not the case or if you're unsure!

After ensuring that your transmitter outputs a 3.3V PPM signal, you can use two patch wires to connect the GND (2) and PPM signal (5) pins from your transmitter's module bay to the ESP32 board:

![PPM signal test](data/images/ppm_test.jpg)

Configure your transmitter to output a PPM signal. As soon as a PPM signal is detected you should see something like this appearing in the monitor log:

	initializing GamepadRefresh & NoiseEstimator tasks
	
	GamepadRefresh: axisCount = 6
	Positive refresh rate --> 16-bit gamepad @ 100 Hz
	NoiseEstimator: sampling noise
	*****************************************************************************************************
	diff : 1 1 1 20 20 21 
	Noise threshold (max) = 21
	Waiting for Bluetooth connection...

You will notice that the blue LED is now blinking slowly, indicating that there is no Bluetooth connection.

Open the Bluetooth settings on your computer. You should see a device called either *JR Gamepad 8*,
*JR Gamepad 16* or *JR Gamepad 2x16* - depending on the number of detected channels and the value of the refresh rate channel (refer to the Usage section)

Pair the device, and if all goes well the onboard LED will turn a steady blue, and you should see a stream of axis values appearing in the serial monitor:

	-65 -196 131 199 -32767 32767 / 100 Hz
	-65 -196 131 196 -32767 32767 / 100 Hz
	-131 -196 134 196 -32767 32767 / 100 Hz
	-65 -196 131 196 -32767 32767 / 100 Hz
	-131 -196 131 199 -32767 32767 / 100 Hz
	-65 -196 131 196 -32764 32767 / 100 Hz
	-131 -196 134 196 -32767 32767 / 100 Hz
	-65 -196 131 196 -32767 32767 / 100 Hz
	-131 -196 134 196 -32767 32767 / 100 Hz

Those values appear at a slow rate when you are not touching the transmitter's sticks, but as soon as you wiggle the sticks the rate will accelerate up to the specified refresh rate (100 Hz in the above output)

Go ahead and run your RC simulator now ;-)


### Testing the circuit on a breadboard

Now build up the entire circuit on a breadboard:

![schematic](data/images/schematic.png)

It will look a bit like this:

![breadboard](data/images/breadboard.jpg)

If in the previous section you had to skip the PPM signal test you can perform that test safely now. The purpose of the transistor circuit in the above schematic is to shift a wide range of input PPM signal voltages down (or up) to 3.3V.


### Soldering the circuit on a stripboard

Those instructions assume that the stripboard circuit is intended to be fit into a Jumper-style JR module box that you can either order online, or 3D-print using the model included in this project.

Prepare a piece of stripboard with the following dimensions:

TODO

![pin headers](data/images/pin_header.jpg)

TODO

![JR bay connector](data/images/module_connector.jpg)


## Usage

While the module is intended to be plug-and-play, you should configure your transmitter for optimal performance (high refresh rates / decreased latency), or simply out of necessity:
Some gamepad drivers do not support 16-bit "high-resolution" axis values,  while others (some Android devices come to mind) may have trouble with refresh rates above 25 Hertz.

But first things first: How many channels do you need for your particular RC simulator ?

I found out for myself that the magic number of channels for my helicopter, glider and drone simulators is 6:

- You need 4 channels for your sticks of course
- 1 channel is sufficient for mapping multiple switches used to toggle functions such as flight modes, autorotation, flaps, etc.
- 1 additional channel - the *refresh rate channel*  - is used to configure this module (by default channel number 6)

This is convenient, because the maximum number of channels supported by a generic gamepad driver is 6 ;-)
An additional, second gamepad has to be emulated if need more than 6 channels, but be aware that some drivers (Android) do not support this.

Therefore, in your transmitter's configuration, set the number of PPM channels to the exact number of channels that you need, and adjust the PPM frame size accordingly with the following formula:

*`number of channels * 2 ms + 2.6 ms = frame size in milliseconds`*

For my 6 channel example, this results in

*`6 * 2 ms + 2.6 ms = 14.6 milliseconds`*

A 14.6 milliseconds PPM frame size results in a 68 Hertz channel refresh rate, which is not too bad.



The refresh rate channel is used to switch into an 8-bit "compatibility" mode and for dynamically setting the desired gamepad update frequency as follows:

* Negative channel values indicate 8 bit compatibility mode, and positive channel values indicate 16 bit mode.
* The absolute (unsigned) channel value specifies the refresh frequency. The frequency can be modified on the fly, without restarting the module.
* The compatibility mode is set automatically as soon as a PPM frame is detected. Changing the mode later on requires restarting the JR module (Power on/off) and un-pairing / re-pairing the module with the host!
  The same applies when switching from less than 6 to more than 6 channels or back, as this impacts the structure of the HID report that is sent to the host.
* If the channel number 6 is not available in the PPM signal then the defaults defined in the main sketch will be used




## How does it work ?

The sketch constantly extracts the RC channels from the JR module bay's PPM signal. Those channel values are mapped to gamepad axis values, which are packed into a gamepad HID report and sent out via Bluetooth LE.

### PPM signal

A "standard" PPM signal has the following characteristics:

* It contains a repeating sequence of 9 pulses: 8 channel pulses plus a sync pulse.
* A notch pulse of typically 400 us marks the beginning of a channel pulse.
  That notch pulse is considered to be part of the channel's pulse width.
* The channel's pulse center is expected to be at 1.5 milliseconds, with the pulse
  width varying +/- 0.5 ms, depending on the channel's value.
  A channel pulse thus has a duration of 1 to 2 milliseconds.     
* The 8 channel pulses are followed by a variable length synchronisation pulse.
  A standard sync pulse has a minimal duration of 4.6 ms, and the entire PPM frame
  has a constant width of 22.5 ms. The sync pulse is used to fill the remainder
  of the 22.5 ms PPM frame.
* The channel update frequency of a standard PPM signal is thus 1000 / 22.5 ms = 44.4 Hz

The PPM algorithm of this sketch is of course geared towards the PPM standard, but it
relaxes a number of its constraints, such as the number of channels encoded in a PPM frame
which may vary from 2 to 12 instead of being fixed to 8, the minimal length of a sync pulse
(2.5 ms instead of 4.6 ms) and the fixed PPM frame length of 22.5 ms.

Generic gamepad drivers support at most 6 axes!

Mapping more than 6 channels, while sticking to generic gamepad drivers, is achieved by
declaring a composite HID device with two gamepads.
But then again, how such composite devices are handled largely depends on the particular
host driver implementation: Mac OS for instance makes them appear as a single gamepad
with 12 axes, Android blissfully ignores a second gamepad, and Windows 10, very surprisingly,
actually behaves according to the USB HID standard by presenting 2 gamepads.