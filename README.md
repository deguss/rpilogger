[Open Source Data Logger](http://opendatalogger.com) [![build-status](https://api.travis-ci.org/deguss/rpilogger.svg)](https://travis-ci.org/deguss/rpilogger)
================

Open source data logger for a Raspberry Pi

## Description
* Analog-to-Digital converters are connected through the I2C interface to the GPIO header of the Raspberry Pi. 
* The real-time kernel Linux embedded software samples analogue voltages with up to 500Hz frequency (2 channels simultaneously, each in differential mode).
* The data is stored in high compression NETCDF4 format (binary) which can be easily processed with any Computer algebra program (Matlab, Octave, Scilab and of course with python numpy).
* Optionally the Raspberry runs a lightweight web server with diagnostic tools. The reports and log files are updated in real-time (websocket connection).


[visit project's website](http://opendatalogger.com)
