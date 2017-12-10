# Arduino Serial to TCP Bridge Client
Arduino client for the [Serial To TCP Bridge Protocol](https://github.com/RoanBrand/SerialToTCPBridgeProtocol) gateway service.

Open a TCP connection to a server from the Arduino using just serial. No Ethernet/WiFi shields necessary.  
Quickly communicate with other servers and make network apps using minimal hardware.  
See [this](https://github.com/RoanBrand/SerialToTCPBridgeProtocol) for more information on the protocol and for the **Protocol Gateway** you will need to run on the host PC the Arduino is connected to serially.  

## Requires:
- Serial 0 (Main serial through USB)
- Timer 1

## Dependencies
- [NeoHWSerial](https://github.com/SlashDevin/NeoHWSerial) - Install manually
- [CRC32](https://github.com/bakercp/CRC32) v2.0.0 - Can be installed from the Arduino *Library Manager*

After installing the dependencies and this library, make sure you have all 3 present inside your **libraries** sub-directory of your sketchbook directory.  

## How to
- Get the [Protocol Gateway](https://github.com/RoanBrand/SerialToTCPBridgeProtocol) and build it.
- Change the gateway's config to listen on the COM port connected to your Arduino and start it.
- Your Arduino app can then use the `ArduinoSerialToTCPBridgeClient` API which is similar to the `EthernetClient` API to make tcp connections to servers as long as they are reachable from the host PC and the gateway service is running.

## Web Client Example
- Modified version of the Ethernet shield's Web Client example.
- The HTTP response data cannot be printed out over USB serial as in original example, because it is already in use by the protocol.
- Instead it uses the software serial library to send it out over another serial connection.
- Note that you would need to connect this second serial line to your PC if you wish to get the data or view the result in the Arduino IDE Serial Monitor.
- On other boards with more hardware serial ports than the Uno the example can be modified to use those instead (Serial1,2,etc.) and remove the software serial lib.

## MQTT Client Example
1. Install [PubSubClient](https://github.com/knolleary/pubsubclient) - Manually, or from the Arduino *Library Manager*.
2. Get a MQTT Broker running on your host PC, listening on `localhost`. I used [HiveMQ](www.hivemq.com).
3. Open and upload the example from the Arduino *Examples menu*.
4. Run the **Protocol Gateway** on the same PC, with the right COM port configuration.
5. When the Arduino is connected to the MQTT broker, it will publish a message and subscribe to the `led` topic.
6. You can use another MQTT client like [MQTT.fx](http://mqttfx.jfx4ee.org) to publish characters `0` and `1` to the topic `led` to toggle the led on and off on the Arduino board.

### Details
- Tested only on Arduino Uno. It would probably not work for the Arduino Due.
- You cannot use the same serial port in your app that is being used by the protocol, e.g. You cannot use `Serial` (Serial0) when the library uses `NeoSerial`, etc. You can modify this lib easily to use other hardware serial ports on bigger boards than the Arduino Uno if you want to free up your USB Serial connection.
- Your app's instance of `ArduinoSerialToTCPBridgeClient` needs to be a pointer and created with `new()`. It doesn't work otherwise and I don't know why yet.

- The protocol provides the app an in order, duplicates free and error checked byte stream by adding a CRC32 and simple retry mechanism. See [this](https://en.wikibooks.org/wiki/Serial_Programming/Error_Correction_Methods) for background.
- The **Protocol Gateway** opens a real TCP connection to a set destination on behalf of the **Protocol Client** running on the Arduino, and forwards traffic bi-directionally.
- `ArduinoSerialToTCPBridgeClient` is derived from the standard Arduino `Client` class. This means existing code written for Ethernet/Wi-Fi shields should work with this.
- `NeoHWSerial` is an alternative for `Serial`, used to gain access to the AVR UART interrupts.
- The protocol cannot run over the standard Serial API or something like software serial because it needs hardware level RX interrupts when bytes arrive.
