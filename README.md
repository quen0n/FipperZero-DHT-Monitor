# FipperZero-DHT-Monitor
Flipper Zero plugin for monitoring air temperature and humidity using DHT11 and DHT22 (AM2302/AM2301) sensors

# No longer supported. Use [Unitemp](https://github.com/quen0n/unitemp-flipperzero)

![UsageExample](https://sun9-27.userapi.com/impg/hkRTht9w6kq2lwdNi3fvKKxhQhif8Mc-lAVhGQ/HkgLO14EY0g.jpg?size=1914x480&quality=96&sign=b4363ea7edc74cb60e401f0c53876e98&type=album)
## Capabilities
- Support for DHT11/DHT22(AM2302)/AM2301 sensors
- Ability to work with sensors without strapping
- Free choice of I/O ports
- Automatic power on 5V output 1
- Saving settings to an SD card
## Installation
Copy the contents of the repository to the `applications/plugins/dht_monitor` folder and build the project. Flash FZ along with resources.

Or use the plugin included with [Unleashed Firmware](https://github.com/DarkFlippers/unleashed-firmware)
## Connecting
|Sensor pin |Flipper Zero pin|
|--------------|----------------|
|VCC (none, +, VCC, red wire)| 1 (5V) or 9 (3V3)|
|GND (-, GND, black wire)| 8, 18 (GND)|
|DATA (OUT, S, yellow wire)| 2-7, 10, 12-17 (to choose from)|

The selected pin of the DATA line must be specified when adding a sensor in the application.
## Usage
1) Connect the sensor to FZ.
2) Launch the application and press the center button. Select "Add new sensor".
3) Specify sensor parameters - name, type, pin.
4) Click "Save".
The next time you launch the application, the saved sensors will be loaded automatically.
You can delete or change sensor parameters in the main menu.

