# ESP32-WiFi-device-detector

This turns a TTGO T-Display into a tool for finding all transmitting 2.4GHz wifi devices in range. It rotates through a set range of wifi channels, captures packets in promiscuous mode and puts the sending MAC address of each Packet into a list along with other information.

## Setup

1. Clone this repo
2. Get [Platformio](https://platformio.org/get-started)
2. Build and upload the code to your ESP32 development board using Platformio.

## Usage

Once the ESP32 has power, it will run the program and start collecting packets. A growing list of MAC addresses will start appearing on the display (given there are packets in the air). Next to each MAC address you will see the RSSI of the last received packet from that address. If no packet was captured from an address for one minute, it will show the time passed since the last packet was received.

The Ui has two modes. NORMAL and WATCHLIST. Normal is the default mode that will list all seen MAC addreses. WATCHLIST will display only selected devices.

| Button | what it does |
|---|---|
| **click UP / DOWN** | scroll the list |
| **hold UP** | select / unselect device from NORMAL mode for WATCHLIST mode |
| **hold DOWN** | switch modes |
