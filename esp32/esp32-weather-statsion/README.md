# ESP-32 Weather station

# TODO upgrade to this board:

https://www.waveshare.com/product/arduino/boards-kits/esp32/esp32-c6-lcd-1.47.htm
and a U/I
Waveshare ESP32-C6 : LVGL UI Tutorial with Squareline Studio

and of course we eventually want to broadcast this over 900mhz LORA and not wifi bc that would be cooler
https://www.ibiblio.org/kuphaldt/electricCircuits/AC/AC_14.html
https://www.youtube.com/watch?v=2AXv49dDQJw

Example output of current code:

```
Leaving...
Hard resetting via RTS pin...
================================================== [SUCCESS] Took 12.51 seconds ==================================================
--- Terminal on /dev/cu.usbserial-10 | 115200 8-N-1
--- Available filters and text transformations: colorize, debug, default, direct, esp32_exception_decoder, hexlify, log2file, nocontrol, printable, send_on_enter, time
--- More details at https://bit.ly/pio-monitor-filters
--- Quit: Ctrl+C | Menu: Ctrl+T | Help: Ctrl+T followed by Ctrl+H
Starting setup...
Serial initialized
Initializing I2C...
I2C initialized
Scanning I2C bus...
I2C device found at address 0x77 !
Initializing BME680...
BME680 initialized successfully
OLED display initialized
Temperature = 33.76 *C
Pressure = 691.93 hPa
Humidity = 100.00 %
Reading took 826 ms
Successful readings: 1 | Errors: 0

Temperature = 33.76 *C
Pressure = 691.93 hPa
Humidity = 100.00 %
Reading took 465 ms
Successful readings: 2 | Errors: 0
```
