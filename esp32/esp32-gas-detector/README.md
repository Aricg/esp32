# MQ-135 Gas Detector

# Example output

Apparantly it will reach a lower baseline after running for 24 hours and then your can set threasholds to do
something.

```
================================================== [SUCCESS] Took 8.97 seconds ==================================================
--- Terminal on /dev/cu.usbserial-10 | 115200 8-N-1
--- Available filters and text transformations: colorize, debug, default, direct, esp32_exception_decoder, hexlify, log2file, nocontrol, printable, send_on_enter, time
--- More details at https://bit.ly/pio-monitor-filters
--- Quit: Ctrl+C | Menu: Ctrl+T | Help: Ctrl+T followed by Ctrl+H
.................................................
Calibration complete!
Calibrated R0 value: 7.32
MQ135 sensor initialized!
Waiting 5 seconds for sensor warm-up...
Sensor warm-up complete. Starting readings...
-----------------------------
Raw Analog Value: 1130
R0: 7.32
Gas Concentration: 2.84 ppm
-----------------------------
Raw Analog Value: 1107
R0: 7.32
Gas Concentration: 2.76 ppm
-----------------------------
Raw Analog Value: 1127
R0: 7.32
Gas Concentration: 2.65 ppm
-----------------------------
```
