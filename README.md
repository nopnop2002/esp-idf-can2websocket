# esp-idf-can-browser
Brows CAN-Frame using esp-idf.
You can brows CAN-FRAME using built-in http server.

![browser-1](https://user-images.githubusercontent.com/6020549/124376395-cf622480-dce1-11eb-9b24-2f69c4a7675b.jpg)

# Software requirement
esp-idf v4.2-dev-2243 or later.   
Use twai(Two-Wire Automotive Interface) driver instead of can driver.   

# Hardware requirements
1. SN65HVD23x CAN-BUS Transceiver   

|SN65HVD23x||ESP32|ESP32-S2||
|:-:|:-:|:-:|:-:|:-:|
|D(CTX)|--|GPIO21|GPIO20|(*1)|
|GND|--|GND|GND||
|Vcc|--|3.3V|3.3V||
|R(CRX)|--|GPIO22|GPIO21|(*1)|
|Vref|--|N/C|N/C||
|CANL|--|||To CAN Bus|
|CANH|--|||To CAN Bus|
|RS|--|GND|GND|(*2)|

(*1) You can change using menuconfig.

(*2) N/C for SN65HVD232

2. Termination resistance   
I used 150 ohms.   


# Test Circuit   
```
   +-----------+ +-----------+ +-----------+ 
   | Atmega328 | | Atmega328 | |   ESP32   | 
   |           | |           | |           | 
   | Transmit  | | Receive   | | 21    22  | 
   +-----------+ +-----------+ +-----------+ 
     |       |    |        |     |       |   
   +-----------+ +-----------+   |       |   
   |           | |           |   |       |   
   |  MCP2515  | |  MCP2515  |   |       |   
   |           | |           |   |       |   
   +-----------+ +-----------+   |       |   
     |      |      |      |      |       |   
   +-----------+ +-----------+ +-----------+ 
   |           | |           | | D       R | 
   |  MCP2551  | |  MCP2551  | |   VP230   | 
   | H      L  | | H      L  | | H       L | 
   +-----------+ +-----------+ +-----------+ 
     |       |     |       |     |       |   
     +--^^^--+     |       |     +--^^^--+
     |   R1  |     |       |     |   R2  |   
 |---+-------|-----+-------|-----+-------|---| BackBorn H
             |             |             |
             |             |             |
             |             |             |
 |-----------+-------------+-------------+---| BackBorn L

      +--^^^--+:Terminaror register
      R1:120 ohms
      R2:150 ohms(Not working at 120 ohms)
```

__NOTE__   
3V CAN Trasnceviers like VP230 are fully interoperable with 5V CAN trasnceviers like MCP2551.   
Check [here](http://www.ti.com/lit/an/slla337/slla337.pdf).


# Installation for ESP32
```
git clone https://github.com/nopnop2002/esp-idf-can-browser
cd esp-idf-can-browser
idf.py set-target esp32
idf.py menuconfig
idf.py flash
```

# Installation for ESP32-S2
```
git clone https://github.com/nopnop2002/esp-idf-can-browser
cd esp-idf-can-browser
idf.py set-target esp32s2
idf.py menuconfig
idf.py flash
```

# Configuration
![config-main](https://user-images.githubusercontent.com/6020549/124376412-dee16d80-dce1-11eb-8f32-e12ef4c29f9f.jpg)
![config-app](https://user-images.githubusercontent.com/6020549/124376415-e0129a80-dce1-11eb-953a-89827f9619d9.jpg)

## CAN Setting
![config-can](https://user-images.githubusercontent.com/6020549/124376426-ea349900-dce1-11eb-99fd-d8b5609d4178.jpg)

## WiFi Setting
![config-wifi](https://user-images.githubusercontent.com/6020549/124376436-f3be0100-dce1-11eb-8c75-a88255f40ed3.jpg)

You can use Staic IP.
![config-static](https://user-images.githubusercontent.com/6020549/124376437-f587c480-dce1-11eb-80f5-efc9819d8c91.jpg)

You can connect using mDNS name.
![config-mDNS](https://user-images.githubusercontent.com/6020549/124376438-f7518800-dce1-11eb-9fc9-ca97921ebf22.jpg)
![browser-2](https://user-images.githubusercontent.com/6020549/124376400-d12be800-dce1-11eb-8c24-46fda2fa4283.jpg)

## HTTP Server Setting
![config-http](https://user-images.githubusercontent.com/6020549/124376453-09332b00-dce2-11eb-90c7-4576564f9ffc.jpg)

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
![format-binary](https://user-images.githubusercontent.com/6020549/124376467-1d772800-dce2-11eb-900c-4492f0dd65ce.jpg)
![format-octal](https://user-images.githubusercontent.com/6020549/124376473-1f40eb80-dce2-11eb-8f0c-12e1841ad9b1.jpg)
![format-decimal](https://user-images.githubusercontent.com/6020549/124376476-21a34580-dce2-11eb-8f30-e4d5f5c16a53.jpg)
![format-hex](https://user-images.githubusercontent.com/6020549/124376478-22d47280-dce2-11eb-8aff-aaeff4eeb2cb.jpg)

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

# Reference

https://github.com/nopnop2002/esp-idf-can2http

https://github.com/nopnop2002/esp-idf-can2mqtt

