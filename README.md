# ESP32-WiFi-device-detector

This turns a TTGO T-Display into a tool for finding all transmitting 2.4GHz wifi devices in range. It rotates through a set range of wifi channels, captures packets in promiscuous mode and puts the sending MAC address of each Packet into a list along with other information.

## Setup

1. Install Visual Studio Code
2. Install the PlatformIO Extention for VSCode
3. Clone this repo and open it in VSCode
4. Build and upload the code to your ESP32 development board

## Usage

Once the ESP32 has power, it will run the program and start collecting packets. A growing list of MAC addresses will start appearing on the display (given there are packets in the air). Next to each MAC address you will see the RSSI of the last received packet from that address. If no packet was captured from an address for one minute, it will show the time since the last received packet.

The Ui has two modes. NORMAL and WATCHLIST. Normal is the default mode that will list all seen MAC addreses. WATCHLIST will display only selected devices.

| Button | what it does |
|---|---|
| **click UP / DOWN** | scroll the list |
| **hold UP** | select / unselect device from NORMAL mode for WATCHLIST mode |
| **hold DOWN** | switch modes |
