# Camera slider on ESP8266

## Info

This repository contains the software for my ESP8266 based Camera slider.

Features:
- No collision-detection!!

Uncomment and modify Wifi client settings in secrets.h file:
* #define WIFI_SSID                "Slider"
* #define WIFI_PASS                "SliderPass"

# Electrical parts needed:
* ESP8266,
* DC/DC converter 12V to 3.3V,
* 1 x TMC2208,
* 1 x Stepper mottor Nema 17,
* wires,
* connectors,

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


# D1 mini CONNECTIONS
* STEP      - GPIO14 (D5)
* DIR1      - GPIO13 (D7)
* MOTTOR EN - GPIO2  (D4)
* RX        - TMC2208 single wire uart
* TX        - TMC2208 single wire uart
# Wiring

![alt tag](https://github.com/BubuHub/ESP8266_Camera_Slider/blob/main/blob/assets/schematic.png)

## Building

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

Enjoy :-)

