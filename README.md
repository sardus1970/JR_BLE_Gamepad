# JR BLE Gamepad - 2021-01-03 Fabrizio Sitzia 

- Introduction

This ESP32 sketch implements a JR module that makes an RC transmitter behave like a 
Bluetooth HID Gamepad.
Most operating systems and RC simulators support such gamepads out of the box, making
it unnecessary to install additional drivers.

The JR module has been specifically targeted towards a Jumper T8SG v2 Plus transmitter, 
used for controlling an RC helicopter simulator ("CGM Next") running an a Macbook Pro.
The goal was to get rid of any USB-C hubs, dongles, cables, etc. while preserving the
low latency and high resolution of a wired USB connection.

By the time of this writing, the module has been tested successfully on:

* Macs running the current Mac OS versions (Catalina and Big Sur), up to 12 axes / 50 Hz
refresh rate and 6 axes / 100 Hz, both with 16 bit resolution for the gamepad axes.
* Android devices running an RC glider simulator ("Picasim"), but see note below!
* TODO: Windows 10 tests

Note:

Some Android devices have issues with 16 bit axis resolution ("Bqeel Y4 Max" TV box),
while others have trouble with refresh rates above 20Hz ("Alldocube M5X S" tablet).
This has nothing to do with CPU performance, as the slowest of the lot (my venerable
"Blackview BV7000 Pro" smartphone) has no issues with 16 bit resolution / 70 Hz!

This prompted the implementation of an 8 bit "compatibility mode" and configurable
refresh rate from the transmitter (by sacrificing one channel), and an algorithm for
sending HID notifications only when a channel value has changed ...but as channel
values fluctuate all the time due to noise, this issue had to be dealt with as well!


- Implementation notes

This sketch extracts the RC channels from the PPM signal and maps them to gamepad axes.
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


It has been developed on an "AZ-Delivery ESP32 D1 Mini" board, but it should run on 
 * any similar ESP32-based board. The D1 Mini was chosen because it fits neatly in a
 * JR module bay enclosure.
 * 
 * 

- Usage

Channel number 6 is used by this JR module to switch into an 8 bit "compatibility" mode
and for dynamically setting the desired gamepad update frequency as follows:

* Negative channel values indicate 8 bit compatibility mode, and positive channel
  values indicate 16 bit mode.
* The absolute (unsigned) channel value specifies the refresh frequency.
  The frequency can be modified on the fly, without restarting the module.
* The compatibility mode is set automatically as soon as a PPM frame is detected.
  Changing the mode later on requires restarting the JR module (Power on/off) and
  un-pairing / re-pairing the module with the host!
  The same applies when switching from less than 6 to more than 6 channels or back,
  as this impacts the structure of the HID report that is sent to the host.
* If the channel number 6 is not available in the PPM signal it is assumed that you
  want to achieve the lowest possible latency / highest possible refresh rate.
  Therefore 16 bit mode with 100 Hz is set as the default.


Immediately after successfull connection with the host, the noise level of each of the
PPM signal's channels is sampled for 1 second.
This is needed for only sending host notifications when required, ie. when a channel's
value changes due to user input, but not due to noise!

The transmitter's sticks should therefore not be moved during the first 3 seconds or so
after connecting to the host.
