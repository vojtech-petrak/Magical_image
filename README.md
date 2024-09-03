# Magical image

Firmware for the ESP8266 MCU that downloads images through WiFi and displays them on an E-ink display.
It connects to WiFi only within a three hour range by day and the images change at night.
To do that it keeps track of time and syncronizes it everyday with an NTP server.

It is designed to detect every error possible, report it and recover from it.
If the error can't be recovered from (such as disconnected SD card), it cleares the display so it doesn't get damaged (it gets damaged if there is an image that doesn't change for more than a day).
All possible errors are specified in the storage.h file.

## Hardware

- ESP8266
- Waveshare e-ink display - the display dimensions must by updated in the epd.h file, the connection pins are also there  
  (documentation: https://www.waveshare.com/wiki)
- SD card - the connection pins are specified in the storage.h file
- battery - unregulated voltage must be supplied to the A0 pin

## Server endpoints and file encoding

The URLs must be specified in the server_access.h file and as you are there, also specify the WiFi credentials.  
Some other constants regarding the operation of the device can be changed in the storage.h file.

- recent image (GET) - use if particular image is to be displayed that night
  - days until next check of this endpoint
  - one or zero images
- random images (GET) - use if the date when the image is displayed doesn't matter
  - number of images in the file
  - the images
- error and battery report (POST)
  - first three bits specify the origin of an error, the rest specifies the error itself
  - if byte full of ones is sent, the next two bytes specify battery charge (in little indian)

### Image encoding

- image heigth (two little endian bytes)
- image width (two little endian bytes)
- image data (pairs of three bit color codes prepended with a zero bit)

## Build and upload

`pio run -t upload`

If that doens't work, try running `pio run -t compiledb` beforehand.
