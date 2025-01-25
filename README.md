# esp32

esp32 projects

```
md5sum ~/Documents/Arduino/CameraWebServer/build/esp32.esp32.esp32cam/CameraWebServer.ino.merged.bin
lvim ~/Library/Arduino15/packages/esp32/tools/esp32-arduino-libs/idf-release_v5.3-cfea4f7c-v1/esp32/sdkconfig
ls /dev/tty.u*
screen /dev/tty.usbserial-2110 115200
ctrl-a k (I haven't used screen is years..)
```

```
python -m esptool --port /dev/cu.usbserial-2120 erase_flash

```

```
esptool.py --chip esp32 --port /dev/ttyUSB0 write_flash -z 0x1000 firmware.bin
```

ESP-IDF (hard mode)

```
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh
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
