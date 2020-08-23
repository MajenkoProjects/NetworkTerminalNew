This is a new implementation of the FabGL Network Terminal example.
It uses an ESP32 with a VGA interface to provide a network terminal
over Telnet.

This enhanced and re-written version includes:

* DEC LAT Terminal server inspired feel
* Privileged configuration mode
* NVS storage of settings
* Local dmoain searching
* Multiple concurrent sessions
* Break-back-to-local with CTRL-BREAK key
* OTA Update pushing

Requires:

* FabGL - https://github.com/fdivitto/FabGL
* CommandParser - https://github.com/MajenkoLibraries/CommandParser

Tested on V1.4 of the TTGO ESP32 VGA board.
