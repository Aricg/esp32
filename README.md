# esp32

esp32 projects

```
python -m esptool --port /dev/cu.usbserial-2120 erase_flash

```

```
esptool.py --chip esp32 --port /dev/ttyUSB0 write_flash -z 0x1000 firmware.bin
```

platform.io

```
platformio init --board esp32dev
platformio init --board esp32cam
```

```
platformio lib install
platformio run
platformio run --target upload
```

```
platformio device monitor -p /dev/cu.usbserial-2120 -b 115200
```
