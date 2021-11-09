# Camera slider on ESP8266

## Info

This repository contains the software for my ESP8266 based Camera slider.

Features:
- No collision-detection!!
- Simple commands over TCP (port 2500),
- Simple commands over HTTP POST ( http://slider.local/post )
- Set mottor current and microsteps per step by a command (software).

![alt tag](https://github.com/BubuHub/ESP8266_Camera_Slider/blob/main/blob/assets/slider.jpg)


# Mechanical parts needed:
* 3D printed pars,
* 1 x T8 Trapezoidal Lead Screw L=500mm,
* 1 x T8 Brass Nut,
* 2 x KP08 Bore Ball Bearing Pillow Block Mounted Support 8mm,
* 2 x 8mm Inner Dia L=34.5mm SCS8UU Linear Bearing,
* 2 x Aluminum 8mm Innner Dia 42x32x11mm SK8 Linear Rod Rail Support Guide Shaft Bearing
* 2 x Linear Shaft Optical Axis 8mm L=500mm,
* 4 x SK8 Linear Rod Rail Shaft,
* 1 x T8 Nut Conversion Seat,
* screws


![alt tag](https://github.com/BubuHub/ESP8266_Camera_Slider/blob/main/blob/assets/front.jpg)

![alt tag](https://github.com/BubuHub/ESP8266_Camera_Slider/blob/main/blob/assets/rear.jpg)


# Electrical parts needed:
* ESP8266,
* DC/DC converter 12V to 3.3V,
* 1 x TMC2208,
* 1 x Stepper mottor Nema 17,
* wires,
* connectors,


# D1 mini CONNECTIONS
* STEP      - GPIO14 (D5)
* DIR1      - GPIO13 (D7)
* MOTTOR EN - GPIO2  (D4)
* RX        - TMC2208 single wire uart
* TX        - TMC2208 single wire uart
# Wiring

![alt tag](https://github.com/BubuHub/ESP8266_Camera_Slider/blob/main/blob/assets/schematic.png)

## Building

Uncomment and modify Wifi client settings in secrets.h file:
* #define WIFI_SSID                "Slider"
* #define WIFI_PASS                "SliderPass"

The project uses platformio build environment. 
[PlatformIO](https://platformio.org/) - Professional collaborative platform for embedded development.

* install PlatformIO
* enter project directory (esp8266_firmware)
* connect ESP8266 PC computer over USB to serial cable.
* type in terminal:
  platformio run -t upload

You can also use IDE to build this project on Linux/Windows/Mac. My fvorite ones:
* [Code](https://code.visualstudio.com/) 
* [Atom](https://atom.io/)


# Commands
C - set mottor current in mA
Example (set mottor curent to 400mA):

C,400

S - set microstps per step (1,2,3,8,16,32,64,128,256)
Example (set 32 misrosteps per step):

S,32

M - Absolute move ( parameters: time in ms, start position, end position)
Example (move from position 1 to 45 in 30000 miliseconds):
M,30000,1,45

MR - Relative move ( parameters: time in miliseconds, position incremental value):
Example (move 1 revolution in backward direction in 2 seconds):
MR,2000,1


Enjoy :-)

