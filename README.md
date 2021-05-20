# esp-euc
Read speed & battery info from an EUC (ninebot one S2) and display it on ESP32 LCD

<img src=https://github.com/mikerr/esp-euc/blob/main/ninebotones2.png width=100> <img src=https://github.com/mikerr/esp-euc/blob/main/IMG_20210512_155312.jpg width=200>

## Requirements:

- TTGO T-Display ESP32 LCD
- Ninebot ONE wheel .. currently supports A1/A2/S1/S2

## Usage:

Auto connects to wheel on power up.




## Technical Notes:

Info on original Ninebot ONE protocol:

[http://www.gorina.es/9BMetrics/protocol.html]

[http://www.gorina.es/9BMetrics/variables.html]

Ninebot S2 uses different service / characteristic

```
SERVICE - 6e400001-b5a3-f393-e0a9-e50e24dcca9e
WRITE - 6e400002-b5a3-f393-e0a9-e50e24dcca9e
READ - 6e400003-b5a3-f393-e0a9-e50e24dcca9e
```
0x11 instead of 0x09 in data packet position 3

<img src=https://github.com/mikerr/esp-euc/blob/main/IMG_20210511_122921.jpg width=200>
