# esp-idf-can2websocket
Brows CAN-Frame using esp-idf.   
You can browse the CAN-Frame in __real time__ using the built-in WebSocket server.   
Similar to [this](https://github.com/nopnop2002/esp-idf-can2http), but this project uses the WebSocket protocol instead of the HTTP protocol.   

![format-binary](https://user-images.githubusercontent.com/6020549/140626005-ed586eef-3cb4-47e0-9d31-e97df281b09a.jpg)

I used [this](https://github.com/Molorius/esp32-websocket) component.   
This component can communicate directly with the browser.   
There is an example of using the component [here](https://github.com/Molorius/ESP32-Examples).
It's a great job.   

# Software requirement
ESP-IDF V4.4/V5.x.   
ESP-IDF V5.1 is required when using ESP32C6.   

# Hardware requirements
- SN65HVD23x CAN-BUS Transceiver   
SN65HVD23x series has 230/231/232.   
They differ in standby/sleep mode functionality.   
Other features are the same.   

- Termination resistance   
I used 150 ohms.   

# Wireing   
|SN65HVD23x||ESP32|ESP32-S2/S3|ESP32-C3/C6||
|:-:|:-:|:-:|:-:|:-:|:-:|
|D(CTX)|--|GPIO21|GPIO17|GPIO0|(*1)|
|GND|--|GND|GND|GND||
|Vcc|--|3.3V|3.3V|3.3V||
|R(CRX)|--|GPIO22|GPIO18|GPIO1|(*1)|
|Vref|--|N/C|N/C|N/C||
|CANL|--||||To CAN Bus|
|CANH|--||||To CAN Bus|
|RS|--|GND|GND|GND|(*2)|

(*1) You can change using menuconfig. But it may not work with other GPIOs.  

(*2) N/C for SN65HVD232

# Test Circuit   
```
   +-----------+   +-----------+   +-----------+ 
   | Atmega328 |   | Atmega328 |   |   ESP32   | 
   |           |   |           |   |           | 
   | Transmit  |   | Receive   |   | 21    22  | 
   +-----------+   +-----------+   +-----------+ 
     |       |      |        |       |       |   
   +-----------+   +-----------+     |       |   
   |           |   |           |     |       |   
   |  MCP2515  |   |  MCP2515  |     |       |   
   |           |   |           |     |       |   
   +-----------+   +-----------+     |       |   
     |      |        |      |        |       |   
   +-----------+   +-----------+   +-----------+ 
   |           |   |           |   | D       R | 
   |  MCP2551  |   |  MCP2551  |   |   VP230   | 
   | H      L  |   | H      L  |   | H       L | 
   +-----------+   +-----------+   +-----------+ 
     |       |       |       |       |       |   
     +--^^^--+       |       |       +--^^^--+
     |   R1  |       |       |       |   R2  |   
 |---+-------|-------+-------|-------+-------|---| BackBorn H
             |               |               |
             |               |               |
             |               |               |
 |-----------+---------------+---------------+---| BackBorn L

      +--^^^--+:Terminaror register
      R1:120 ohms
      R2:150 ohms(Not working at 120 ohms)
```

__NOTE__   
3V CAN Trasnceviers like VP230 are fully interoperable with 5V CAN trasnceviers like MCP2551.   
Check [here](http://www.ti.com/lit/an/slla337/slla337.pdf).


# Installation
```
git clone https://github.com/nopnop2002/esp-idf-can2websocket
cd esp-idf-can2websocket
git clone https://github.com/Molorius/esp32-websocket components/websocket
idf.py set-target {esp32/esp32s2/esp32s3/esp32c3/esp32c6}
idf.py menuconfig
idf.py flash
```


# Configuration
![config-main](https://user-images.githubusercontent.com/6020549/124376412-dee16d80-dce1-11eb-8f32-e12ef4c29f9f.jpg)
![config-app](https://user-images.githubusercontent.com/6020549/140625989-cbad6cf9-937d-4319-aaa4-b491c80efef9.jpg)

## CAN Setting
![config-can](https://user-images.githubusercontent.com/6020549/124376426-ea349900-dce1-11eb-99fd-d8b5609d4178.jpg)

Display all received frames to STDOUT.
![stdout](https://user-images.githubusercontent.com/6020549/140626592-7a16386a-1846-4cce-a1ea-068c38b80332.jpg)

## WiFi Setting
![config-wifi](https://user-images.githubusercontent.com/6020549/124376436-f3be0100-dce1-11eb-8c75-a88255f40ed3.jpg)

You can use Static IP.   
![config-static](https://user-images.githubusercontent.com/6020549/124376437-f587c480-dce1-11eb-80f5-efc9819d8c91.jpg)

You can connect using mDNS name.   
![config-mDNS](https://user-images.githubusercontent.com/6020549/124376438-f7518800-dce1-11eb-9fc9-ca97921ebf22.jpg)
![mDNS](https://user-images.githubusercontent.com/6020549/140626470-20629948-fe89-46ae-ba2e-a4b8032d9778.jpg)


# Definition CANbus Frame
When CANbus data is received, it is sent by built-in HTTP server according to csv/can2http.csv.   
The file can2http.csv has three columns.   
In the first column you need to specify the CAN Frame type.   
The CAN frame type is either S(Standard frame) or E(Extended frame).   
In the second column you have to specify the CAN-ID as a __hexdecimal number__.    
In the last column you have to specify the Frame Name.  
A descriptive name for CAN Frame.   

```
S,101,Water Temperature
E,101,Water Pressure
S,103,Gas Temperature
E,103,Gas Pressure
```

# Format conversion   
- Binary   
![format-binary](https://user-images.githubusercontent.com/6020549/140626005-ed586eef-3cb4-47e0-9d31-e97df281b09a.jpg)

- Octal   
![format-octal](https://user-images.githubusercontent.com/6020549/140626012-92de1550-c3ef-4345-a7f7-f5f2317a1cc8.jpg)

- Decimal   
![format-decimal](https://user-images.githubusercontent.com/6020549/140626018-43ceef90-5c34-4fad-b603-f13fe833568c.jpg)

- Hexadecimal   
![format-hex](https://user-images.githubusercontent.com/6020549/140626028-8a26e474-9e73-490b-b2d2-e32718f428d7.jpg)

# Troubleshooting   
There is a module of SN65HVD230 like this.   
![SN65HVD230-1](https://user-images.githubusercontent.com/6020549/80897499-4d204e00-8d34-11ea-80c9-3dc41b1addab.JPG)

There is a __120 ohms__ terminating resistor on the left side.   
![SN65HVD230-22](https://user-images.githubusercontent.com/6020549/89281044-74185400-d684-11ea-9f55-830e0e9e6424.JPG)

I have removed the terminating resistor.   
And I used a external resistance of __150 ohms__.   
A transmission fail is fixed.   
![SN65HVD230-33](https://user-images.githubusercontent.com/6020549/89280710-f7857580-d683-11ea-9b36-12e36910e7d9.JPG)

If the transmission fails, these are the possible causes.   
- There is no receiving app on CanBus.
- The speed does not match the receiver.
- There is no terminating resistor on the CanBus.
- There are three terminating resistors on the CanBus.
- The resistance value of the terminating resistor is incorrect.
- Stub length in CAN bus is too long. See [here](https://e2e.ti.com/support/interface-group/interface/f/interface-forum/378932/iso1050-can-bus-stub-length).

# WEB Page   
The WEB page is stored in the html folder.   
You can change it as you like.   

# Task Structure Diagram
![Task_structure_diagram](https://user-images.githubusercontent.com/6020549/140627132-eee6eaff-c635-45c8-9b13-d09625124891.JPG)


# Reference
https://github.com/nopnop2002/esp-idf-can2http

https://github.com/nopnop2002/esp-idf-can2mqtt

