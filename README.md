# Arduino TCP over Serial Client
Arduino client for the [Serial To TCP Bridge Protocol](https://github.com/RoanBrand/SerialToTCPBridgeProtocol) gateway service.

Open a TCP over Serial connection to a server from the Arduino using the host. No Ethernet/WiFi shields necessary.  
Quickly communicate with other servers and make network apps using minimal hardware.  
See [this](https://github.com/RoanBrand/SerialToTCPBridgeProtocol) for more information on the protocol and for the **Protocol Gateway** you will need to run on the host PC the Arduino is connected to serially.  

## Hardware Requirements:
- Serial 0 (Main serial through USB)
- Timer 1

## Dependencies
- [NeoHWSerial](https://github.com/gicking/NeoHWSerial) v1.6.6
- [CRC32](https://github.com/bakercp/CRC32) v2.0.0
- [PubSubClient](https://github.com/knolleary/pubsubclient) v2.8.0 - Needed for the included **MQTT Client Example**

These should install automatically when installing this library through the Arduino *Library Manager*.

## How to
- Get the [Protocol Gateway](https://github.com/RoanBrand/SerialToTCPBridgeProtocol) and build it.
- Change the gateway's config to listen on the Serial port connected to your Arduino and start it.
- Your Arduino app can then use the `TCPOverSerialClient` API which is similar to the `EthernetClient` API to make tcp connections to servers as long as they are reachable from the host PC and the gateway service is running.

## Web Client Example
- Modified version of the Ethernet shield's Web Client example.
- The HTTP response data cannot be printed out over USB serial as in original example, because it is already in use by the protocol.
- Instead it uses the software serial library to send it out over another serial connection.
- Note that you would need to connect this second serial line to your PC if you wish to get the data or view the result in the Arduino IDE Serial Monitor.
- On other boards with more hardware serial ports than the Uno the example can be modified to use those instead (Serial1,2,etc.) and remove the software serial lib.

## MQTT Client Example
1. Open and upload the example from the Arduino *Examples menu*.
2. Run the **Protocol Gateway** on the same PC, with the correct Serial port configuration.
3. After a short time, the Arduino should connect to the MQTT broker `mqtt.eclipseprojects.io`.
4. Using a MQTT client, like [MQTTX](https://mqttx.app/), connect to the same broker and subscribe to the `ArduinoOut` topic.
5. While the Arduino is connected to the MQTT broker, it will publish a message to the `ArduinoOut` topic every 5s.
6. Using the client, publish characters `1` and `2` to topic `LedIn` to toggle the led on and off on the Arduino board.

### Details
- Tested only on Arduino Uno. It would probably not work for the Arduino Due.
- You cannot use the same serial port in your app that is being used by the protocol, e.g. You cannot use `Serial` (Serial0) when the library uses `NeoSerial`, etc. You can modify this lib easily to use other hardware serial ports on bigger boards than the Arduino Uno if you want to free up your USB Serial connection.
- Your app's instance of `TCPOverSerialClient` needs to be a pointer and created with `new()`. It doesn't work otherwise and I don't know why yet.

- The protocol provides the app an in order, duplicates free and error checked byte stream by adding a CRC32 and simple retry mechanism. See [this](https://en.wikibooks.org/wiki/Serial_Programming/Error_Correction_Methods) for background.
- The **Protocol Gateway** opens a real TCP connection to a set destination on behalf of the **Protocol Client** running on the Arduino, and forwards traffic bi-directionally.
- `TCPOverSerialClient` is derived from the standard Arduino `Client` class. This means existing code written for Ethernet/Wi-Fi shields should work with this.
- `NeoHWSerial` is an alternative for `Serial`, used to gain access to the AVR UART interrupts.
- The protocol cannot run over the standard Serial API or something like software serial because it needs hardware level RX interrupts when bytes arrive.
