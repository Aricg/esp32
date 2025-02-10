# esp32

![Timelapse Demo](./timelapse_example.gif)

esp32 projects

```python
python -m esptool --port /dev/cu.usbserial-2120 erase_flash
```

```bash
esptool.py --chip esp32 --port /dev/ttyUSB0 write_flash -z 0x1000 firmware.bin
```

## PlatformIO Commands

```bash
platformio init --board esp32dev
platformio init --board esp32cam
platformio lib install
platformio run
platformio run --target upload
platformio device monitor -p /dev/cu.usbserial-2120 -b 115200
```
