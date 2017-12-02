#include "ArduinoSerialToTCPBridgeClient.h"
#include <NeoHWSerial.h>
#include <CRC32.h>

static ArduinoSerialToTCPBridgeClient* ser0;

void rxISR0(uint8_t c) {
	ser0->rxCallback(c);
}

void ackTimeout() {
	ser0->stopAckTimer();
	ser0->tx_retries++;
	if (ser0->tx_retries >= 5) {
		ser0->stop();
		return;
	}
	if (!ser0->writePacket(ser0->lastTx_cmd, ser0->lastTx_buf, ser0->lastTx_size)) {
		ser0->stop();
		return;
	}
	ser0->startAckTimer();
}

ISR(TIMER1_COMPA_vect) {
	ackTimeout();
}

ArduinoSerialToTCPBridgeClient::ArduinoSerialToTCPBridgeClient() {
	ackOutstanding = false;
	expectedRxSeqFlag = false;
	pubSequence = false;
	tx_retries = 0;
	readBufpH = 0;
	readBufpT = 0;
	readBufisFull = false;
	state = STATE_DISCONNECTED;
	ser0 = this;
	NeoSerial.attachInterrupt(rxISR0);
	NeoSerial.begin(115200);
	setupAckTimer();
}

/*
	http://forum.arduino.cc/index.php?topic=315271.msg2183707#msg2183707

	SUCCESS              1
	FAILED               0
	TIMED_OUT           -1
	INVALID_SERVER      -2
	TRUNCATED           -3
	INVALID_RESPONSE    -4
	DOMAIN_NOT_FOUND     5
*/
int ArduinoSerialToTCPBridgeClient::connect(IPAddress ip, uint16_t port) {
	uint8_t destination[6] = {
		(uint8_t) ((uint32_t) ip),
		(uint8_t) ((uint32_t) ip >> 8),
		(uint8_t) ((uint32_t) ip >> 16),
		(uint8_t) ((uint32_t) ip >> 24),
		(uint8_t) port,
		(uint8_t) (port >> 8)
	};

	if (!writePacket(PROTOCOL_CONNECT, destination, 6)) {
		return 0;
	}

	lastInAct = millis();
	while (state != STATE_CONNECTED) {
		uint32_t now = millis();
		if (now - lastInAct >= 5000) {
			return -1;
		}
	}

	lastInAct = millis();
	return 1;
}

// Maximum host string of 248 bytes.
int ArduinoSerialToTCPBridgeClient::connect(const char *host, uint16_t port) {
	uint8_t destination[250];
	uint8_t len = 0;

	while (host[len] != '\0') {
		if (len == 248) {
			// I can't find out what TRUNCATED return type means. I just use it here.
			return -3;
		}
		destination[len] = host[len];
		len++;
	}

	destination[len++] = (uint8_t) port;
	destination[len++] = (uint8_t) (port >> 8);

	if (!writePacket(PROTOCOL_CONNECT | 0x80, destination, len)) {
		return 0;
	}

	lastInAct = millis();
	while (state != STATE_CONNECTED) {
		uint32_t now = millis();
		if (now - lastInAct >= 5000) {
			return -1;
		}
	}

	lastInAct = millis();
	return 1;
}

size_t ArduinoSerialToTCPBridgeClient::write(uint8_t b) {
	return write(&b, 1);
}

// TODO: Buffer bytes to send. May be too much for the Uno.
size_t ArduinoSerialToTCPBridgeClient::write(const uint8_t *buf, size_t size) {
	if (size > 250)
		return 0;

	while(ackOutstanding && (state == STATE_CONNECTED));
	if (state != STATE_CONNECTED)
		return 0;

	uint8_t cmd = PROTOCOL_PUBLISH;
	if (pubSequence) {
		cmd |= 0x80;
	}

	lastTx_cmd = cmd;
	lastTx_buf = (uint8_t*) buf;
	lastTx_size = size;

	if (!writePacket(lastTx_cmd, lastTx_buf, lastTx_size)) {
		return 0;
	}

	ackOutstanding = true;
	startAckTimer();

	return size;
}

int ArduinoSerialToTCPBridgeClient::available() {
	if (readBufisFull) {
		return 256;
	}
	if (readBufpT >= readBufpH) {
		return (int) (readBufpT - readBufpH);
	} else {
		return 256 - (int) (readBufpH - readBufpT);
	}
}

int ArduinoSerialToTCPBridgeClient::read() {
	if (!available()) {
		return -1;
	}

	uint8_t ch = readBuf[readBufpH++];
	readBufisFull = false;
	return ch;
}

int ArduinoSerialToTCPBridgeClient::read(uint8_t *buf, size_t size) {
	if (!available()) {
		return -1;
	}
}

int ArduinoSerialToTCPBridgeClient::peek() {
	return readBuf[readBufpH];
}

void ArduinoSerialToTCPBridgeClient::flush() {
	NeoSerial.flush();
}

void ArduinoSerialToTCPBridgeClient::stop() {
	stopAckTimer();
	writePacket(PROTOCOL_DISCONNECT, NULL, 0);
	flush();
	//NeoSerial.end();
}

uint8_t ArduinoSerialToTCPBridgeClient::connected() {
	return (state == STATE_CONNECTED) ? 1 : 0;
}

ArduinoSerialToTCPBridgeClient::operator bool() {
	return 1;
}

boolean ArduinoSerialToTCPBridgeClient::writePacket(uint8_t command, uint8_t* payload, uint8_t pLength) {
	workBuffer[0] = pLength + 5;
	workBuffer[1] = command;
	if (payload != NULL) {
		for (uint8_t i = 2; i < pLength + 2; i++) {
			workBuffer[i] = payload[i - 2];
		}
	}

	uint32_t crcCode = CRC32::checksum(workBuffer, pLength + 2);
	workBuffer[pLength + 2] = crcCode & 0x000000FF;
	workBuffer[pLength + 3] = (crcCode & 0x0000FF00) >> 8;
	workBuffer[pLength + 4] = (crcCode & 0x00FF0000) >> 16;
	workBuffer[pLength + 5] = (crcCode & 0xFF000000) >> 24;

	if ((int) (pLength) + 6 > NeoSerial.availableForWrite()) {
		return false;
	}

	for (int i = 0; i < pLength + 6; i++) {
		NeoSerial.write(workBuffer[i]);
	}

	return true;
}

void ArduinoSerialToTCPBridgeClient::rxCallback(uint8_t c) {
	static uint16_t packetCount = 0;
	static uint8_t rxState = RX_PACKET_IDLE;

	rxBuffer[packetCount++] = c;

	switch (rxState) {

	case RX_PACKET_IDLE:
		rxState = RX_PACKET_GOTLENGTH;
		break;

	case RX_PACKET_GOTLENGTH:
		rxState = RX_PACKET_GOTCOMMAND;
		break;

	case RX_PACKET_GOTCOMMAND:
		uint8_t packetLength = rxBuffer[0];

		if (packetCount == (uint16_t)packetLength + 1) {
			packetCount = 0;
			rxState = RX_PACKET_IDLE;

			// Integrity checking.
			uint32_t crcRx = (uint32_t) rxBuffer[packetLength - 3] | ((uint32_t) rxBuffer[packetLength - 2] << 8)
				| ((uint32_t) rxBuffer[packetLength - 1] << 16) | ((uint32_t) rxBuffer[packetLength] << 24);
			uint32_t crcCode = CRC32::checksum(rxBuffer, packetLength - 3);

			// Received packet valid.
			if (crcRx == crcCode) {
				boolean rxSeqFlag = (rxBuffer[1] & 0x80) > 0;

				switch (rxBuffer[1] & 0x7F) {
				// Connection established with destination.
				case PROTOCOL_CONNACK:
					if (rxBuffer[0] == 5) {
						state = STATE_CONNECTED;
					}
					break;
				// Incoming data.
				case PROTOCOL_PUBLISH:
					writePacket(PROTOCOL_ACK | (rxBuffer[1] & 0x80), NULL, 0);
					if (rxSeqFlag == expectedRxSeqFlag) {
						expectedRxSeqFlag = !expectedRxSeqFlag;
						if (rxBuffer[0] > 5) {
							for (uint8_t i = 0; i < rxBuffer[0] - 5; i++) {
								readBuf[readBufpT++] = rxBuffer[2 + i];
							}
							readBufisFull = (readBufpH == readBufpT);
						}
					}
					break;
				// Protocol Acknowledge
				case PROTOCOL_ACK:
					if (ackOutstanding) {
						if (rxSeqFlag == pubSequence) {
							stopAckTimer();
							pubSequence = !pubSequence;
							tx_retries = 0;
							ackOutstanding = false;
						}
					}
					break;
				}
			}
		}
		break;
	}
}

// http://www.engblaze.com/microcontroller-tutorial-avr-and-arduino-timer-interrupts/
void ArduinoSerialToTCPBridgeClient::setupAckTimer() {
	cli();
	TCCR1A = 0;
	TCCR1B = 0;
	OCR1A = 12499; // 200ms
	TCCR1B |= (1 << WGM12); // ctc mode
	TIMSK1 |= (1 << OCIE1A);
	sei();
}

void ArduinoSerialToTCPBridgeClient::startAckTimer() {
	TCCR1B |= (1 << CS12); // 256 prescaler
}

void ArduinoSerialToTCPBridgeClient::stopAckTimer() {
	TCCR1B &= 0xF8; // remove clk source
	TCNT1 = 0; // reset counter
}
