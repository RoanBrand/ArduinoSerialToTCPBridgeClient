/*
 * Basic example of MQTT Client running over the Serial To TCP Bridge Client.
 *
 * It uses the MQTT client package PubSubClient from Nick O’Leary.
 * Install it from the Library Manager, if not already automatically installed.
 *
 * The example connects to the public Broker at "mqtt.eclipseprojects.io".
 * It will publish millis() as a message every 5s to topic "ArduinoOut", while connected.
 *
 * Using a MQTT client (e.g. https://mqttx.app):
 * Publish character '1' to topic "LedIn" to turn on the Led on the Arduino Uno and '2' to turn it off.
 */

#include <TCP_over_Serial.h>
#include <PubSubClient.h>

TCPOverSerialClient* s;       // Protocol Client running over USB Serial
PubSubClient         client;  // MQTT Client

const char* broker   = "mqtt.eclipseprojects.io";
const char* ledTopic = "LedIn";
const char* outTopic = "ArduinoOut";

uint32_t lastPub = 0;

void setup() {
  pinMode(13, OUTPUT);
  s = new TCPOverSerialClient();
  client.setClient(*s);

  // MQTT Broker running on same PC the Arduino is connected to.
  client.setServer(broker, 1883);
  client.setCallback(callback);
}

void loop() {
  // If not connected, retry indefinitely.
  while(!client.loop()) {
    while(!connectToBroker()) {

      // If connection attempt unsuccessful, flash led fast.
      for (uint8_t fc = 0; fc < 10; fc++) {
        digitalWrite(13, !digitalRead(13));
        delay(200);
      }
    }
  }

  // Publish arduino alive time every 5s, while connected to broker.
  uint32_t now = millis();
  if ((now - lastPub) > 5000) {
    String aliveTimeMsg = String(now);
    client.publish(outTopic, aliveTimeMsg.c_str());
    lastPub = now;
  }
}

// MQTT incoming message from broker callback
void callback(char* topic, byte* payload, unsigned int length) {
  // Only proceed if incoming message's topic matches.
  // Toggle led if according to message character.
  if ((strcmp(topic, ledTopic) == 0) && (length == 1)) {
    if (payload[0] == '1') {
      digitalWrite(13, HIGH);
    } else if (payload[0] == '2') {
      digitalWrite(13, LOW);
    }
  }
}

boolean connectToBroker() {
  if (client.connect("arduinoClient")) {
    // Publish first message on connect and subscribe to Led controller.
    client.publish(outTopic, "Arduino connected!");
    client.subscribe(ledTopic);
    return true;
  } else {
    return false;
  }
}

