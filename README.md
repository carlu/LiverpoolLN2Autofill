# University of Liverpool - Nuclear Physics Laboratory - LN2 Autofill System

This repository contains the code for the University of Liverpool Nuclear Physics LN2 autofill system.

## Hardware Components

* Analogue
  * HLMP-1710 green LEDs - Mounted in LN2 exhaust valve, resistance increases when temperature drops so able to tell when LN2 reaches exhaust and the Dewar is full.
  * 6.8 kOhm resistors - In series with LEDs to control current.
  * 24V power supply
  * Voltage dividers - 500 Ohm and 4 kOhm to scale potential into 5V range of Arduino ADC.
  * Relay board and valves.
  * Transistor (2N222 NPN) and resistors (2.2 kOhm, 10 kOhm) to short relay input to ground when GPIO pin is on.
* Digital
  * Arduino Yun - Contains:
    * Atmel ATmega32U4 microcontroller
    * Atheros AR9331 running OpenWRT wireless stack
    * 6 x 10bit 5V ADCs

## Software Components

* System consists of two separate codes:
* C code to run on Arduino microcontroller.
  * Uses Arduino Bridge Library to communicate via HTTP requests.
	* Allows direct read/write of pins via web browser for debugging.
	* "Fill Lines" defined based on relay pin number, LED ADC number, and threshold for ADC/LED.
  * Record kept internally of success/failure and total time of last fill on each line.
	* Several built-in functions to read/write properties of each line or initiate a fill cycle and report the results.
  * readstatus() function able to return a full account of the current system status to any web browser.
* Python script to run on AR9331 or another computer to send control signals, log long term fill data.
  * Also plots LED volts vs time for latest and previous fill.
	* Contains schedule for when lines should be filled, sends fill command to microcontroller when time comes.
	* Plot total fill time for all historical fills.
	* Send email success/fail messages for all autofills, attach plots. (Requires local sendmail functionality)
	* Detect other fail conditions such as no response from Arduino and email warnings.
* Python/Flask based testserver to serve dummy data while debugging.  

## Authors

Carl Unsworth

Dan Judson

Design of LED based LN2 flow sensors copied from TRIUMF GRSI group.
