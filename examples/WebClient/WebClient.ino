/*
  Web client

 This sketch connects to a website (http://www.google.com)
 using the SerialToTCPBridgeProtocol.

 created 18 Dec 2009
 by David A. Mellis
 modified 26 Nov 2017
 by Roan Brand, to work over:
 https://github.com/RoanBrand/SerialToTCPBridgeProtocol

 */

#include <ArduinoSerialToTCPBridgeClient.h>

// if you don't want to use DNS (and reduce your sketch size)
// use the numeric IP instead of the name for the server:
//IPAddress server(74,125,232,128);  // numeric IP for Google (no DNS)
char server[] = "www.google.com";    // name address for Google (using DNS)

// Initialize the Protocol client library
// with the IP address and port of the server
// that you want to connect to (port 80 is default for HTTP):
ArduinoSerialToTCPBridgeClient* client;

void setup() {
  client = new ArduinoSerialToTCPBridgeClient();
  //delay(1000);
  
  if (client->connect(server, 80)) {
    //Serial.println("connected");
    // Make a HTTP request:
    client->println("GET /search?q=arduino HTTP/1.1");
    client->println("Host: www.google.com");
    client->println("Connection: close");
    client->println();
  } else {
    // if you didn't get a connection to the server:
    //Serial.println("connection failed");
  }
}

void loop() {
  // if there are incoming bytes available
  // from the server, read them and print them:
  if (client->available()) {
    char c = client->read();
    //Serial.print(c);
  }

  // if the server's disconnected, stop the client:
  if (!client->connected()) {
    //Serial.println();
    //Serial.println("disconnecting.");
    client->stop();

    // do nothing forevermore:
    while (true);
  }
}
