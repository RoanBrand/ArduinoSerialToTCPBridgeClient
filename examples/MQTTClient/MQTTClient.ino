/*
 * Basic example of MQTT Client running over the Serial To TCP Bridge Client.
 *
 * The example connects to a Broker serving on "localhost" on the host PC.
 * It will publish millis() as a message every 5s, while connected.
 *
 * Using another MQTT client:
 * Publish char '1' to topic "led" to turn on Led on Arduino Uno and '2' to turn off.
 */

#include <ArduinoSerialToTCPBridgeClient.h>
#include <PubSubClient.h>

ArduinoSerialToTCPBridgeClient*      s;  // Protocol Client running over USB Serial
PubSubClient                    client;  // MQTT Client

const char* broker   = "localhost";
const char* ledTopic = "led";
const char* outTopic = "ArduinoOut";

uint32_t lastPub = 0;

void setup() {
  pinMode(13, OUTPUT);
  s = new ArduinoSerialToTCPBridgeClient();
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
    client.publish(outTopic, "Hello world!");
    client.subscribe(ledTopic);
    return true;
  } else {
    return false;
  }
}

