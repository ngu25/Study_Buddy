# Study_Buddy
This is a compact, modular desk gadget designed not just to boost productivity and discipline during study or work, but also to improve personal comfort and wellness. It combines time-management, environmental monitoring, user interaction, and smart notifications, all configurable via web dashboard.

# Hardware
Study_Buddy is based on [PCB Cupid](https://pcbcupid.com/) [GLYPH-C3 Board](https://learn.pcbcupid.com/boards/glyph-c3/overview). The latest version of this project is a stackable build, made using zero PCB, rapid prototyped. However, building a custom designed PCB for this would be a spectacular option and if you're interested, reach out to [navadeep](https://ngu25.github.io/). We'd provide you the support to make this happen.

It is a 3-board stackup with:
- IO Board
    - 4 x Pushbuttons
    - 6 x RG LEDs
- Main Board
    - GLYHP-C3
    - Piezo Buzzer
    - HC-SR04 Sensor
- Display Board
    - OLED Display SSD1306
    - DHT22 Sensor

## Snapshots
### Top
<p align="left">
  <img src="assets/images/sb_top.jpg" alt="Front View" width="500">
</p>

### Bottom
<p align="left">
  <img src="assets/images/sb_bottom.jpg" alt="Back View" width="500">
</p>

### Assembled Device
<p align="left">
  <img src="assets/images/proto_asm.jpg" alt="Assembled Prototype View" width="500">
</p>

# Setup
- Clone this repository as `git clone https://github.com/ngu25/Study_Buddy.git` into your local and use the Arduino IDE, following [these](https://learn.pcbcupid.com/boards/needs/arduino-ide-setup) steps to flash the firmware.

- Configure your network credentials in the code under 
```
// ---------- WiFi ----------
const char* ssid = "<your-ssid>";
const char* password = "your-password";
```

# Technical Details
## Controller Pin Mapping

| GLYPH-C3 Pin | Pin Number | Peripheral          | Application                |
| ------------ | ---------- | ------------------- | -------------------------- |
| SCL          | 1          | I2C SCL             | Display                    |
| SDA          | 2          | I2C SDA             |
| D8           | 3          | RG LED - RED SET1   | Breathing LED Left         |
| D9           | 4          | RG LED - GREEN SET1 | Breathing LED Right        |
| D3           | 6          | ADC 1               | Potentiometer 1            |
| D2           | 7          | ADC 2               | Potentiometer 2            |
| TX           | 27         | Digital Out         | Breathing LED - RED SET2   |
| RX           | 26         | Digital Out         | Breathing LED - GREEN SET2 |
| MI           | 25         | Digital Out         | TRIG Ultrasonic (5V VCC)   |
| MO           | 24         | Digital IN          | ECHO Ultrasonic (5V VCC)   |
| SCK          | 23         | Digital Out         | Buzzer                     |
| D+           | 22         | ADC 3               | LDR Sensor                 |
| D-           | 21         | Digital IN          | Pushbutton 4 (Spare)       |
| A3           | 20         | Digital IN          | Pushbutton 3               |
| A2           | 19         | Digital IN          | Pushbutton 2               |
| A1           | 18         | Digital IN          | Pushbutton 1               |
| A0           | 17         | Digital IN          | DHT22 Sensor               |

## Feature Explanation
### Table Clock
Turn the device ON and as it connects to the network, Live Clock will be displayed in the `HH:MM:SS` format along with the ambient temperature and humidity.

### Pomodoro Timer

### Web Server Usage
