# esp32 Projects:

#Timelapse
![Timelapse Demo](./timelapse.gif)

# Commands

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
platformio device monitor -p /dev/cu.usbserial-10 -b 115200
```
