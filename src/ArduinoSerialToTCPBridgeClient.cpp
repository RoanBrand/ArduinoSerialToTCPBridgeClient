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
	reset();
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
	DOMAIN_NOT_FOUND    -5
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

size_t ArduinoSerialToTCPBridgeClient::write(const uint8_t *buf, size_t size) {
	size_t written = 0;

	while (written < size) {
		size_t left = size - written;
		uint8_t toWrite = (left > 250) ? 250 : left;

		while(ackOutstanding && (state == STATE_CONNECTED));
		if (state != STATE_CONNECTED)
			return 0;

		uint8_t cmd = PROTOCOL_PUBLISH;
		if (pubSequence)
			cmd |= 0x80;

		if (!writePacket(cmd, buf + written, toWrite))
			return 0;

		lastTx_cmd = cmd;
		lastTx_buf = (uint8_t*) (buf + written);
		lastTx_size = toWrite;

		ackOutstanding = true;
		startAckTimer();
		written += toWrite;
	}

	while(ackOutstanding && (state == STATE_CONNECTED));
	if (state != STATE_CONNECTED)
		return 0;

	return size;
}

int ArduinoSerialToTCPBridgeClient::available() {
	if (rxBufisFull)
		return 256;

	if (rxBufpT >= rxBufpH) {
		return (int) (rxBufpT - rxBufpH);
	} else {
		return 256 - (int) (rxBufpH - rxBufpT);
	}
}

int ArduinoSerialToTCPBridgeClient::read() {
	if (available() == 0)
		return -1;

	uint8_t ch = rxBuf[rxBufpH++];
	rxBufisFull = false;
	return ch;
}

int ArduinoSerialToTCPBridgeClient::read(uint8_t *buf, size_t size) {
	int have = available();
	if (have == 0)
		return -1;

	int toRead = (size > have) ? have : size;

	for (size_t i = 0; i < toRead; i++)
		buf[i] = rxBuf[rxBufpH++];

	rxBufisFull = false;
	// should we return rather when number of bytes requested is read?
	return toRead;
}

int ArduinoSerialToTCPBridgeClient::peek() {
	return rxBuf[rxBufpH];
}

void ArduinoSerialToTCPBridgeClient::flush() {
	NeoSerial.flush();
}

void ArduinoSerialToTCPBridgeClient::stop() {
	writePacket(PROTOCOL_DISCONNECT, NULL, 0);
	flush();
	reset();
	//NeoSerial.end();
}

uint8_t ArduinoSerialToTCPBridgeClient::connected() {
	return (state == STATE_CONNECTED) ? 1 : 0;
}

ArduinoSerialToTCPBridgeClient::operator bool() {
	return 1;
}

void ArduinoSerialToTCPBridgeClient::reset() {
	stopAckTimer();
	state = STATE_DISCONNECTED;
	ackOutstanding = false;
	expectedRxSeqFlag = false;
	pubSequence = false;
	tx_retries = 0;
	rxBufpH = 0;
	rxBufpT = 0;
	rxBufisFull = false;
}

boolean ArduinoSerialToTCPBridgeClient::writePacket(uint8_t command, uint8_t* payload, uint8_t pLength) {
	if (pLength > 250)
		return false;

	CRC32 crc;
	uint8_t h_length = pLength + 5;

	while(NeoSerial.availableForWrite() < 2);
	NeoSerial.write(h_length);
	crc.update(h_length);
	NeoSerial.write(command);
	crc.update(command);

	if (payload != NULL) {
		uint8_t written = 0;

		while(written < pLength) {
			int space = NeoSerial.availableForWrite();

			if (space > 0) {
				uint8_t left = pLength - written;
				uint8_t toWrite = (left > space) ? space : left;

				for (uint8_t i = written; i < (written + toWrite); i++) {
					NeoSerial.write(payload[i]);
					crc.update(payload[i]);
				}
				written += toWrite;
			}
		}
	}

	uint32_t checksum = crc.finalize();

	while(NeoSerial.availableForWrite() < 4);
	NeoSerial.write(checksum);
	NeoSerial.write(checksum >> 8);
	NeoSerial.write(checksum >> 16);
	NeoSerial.write(checksum >> 24);

	return true;
}

void ArduinoSerialToTCPBridgeClient::rxCallback(uint8_t c) {
	static uint16_t byteCount = 0;
	static uint8_t rxState = RX_PACKET_IDLE;

	static uint8_t newBufPtr;
	static uint8_t p_length;
	static uint8_t p_cmd;
	static uint32_t p_crc;
	static CRC32 crc;

	byteCount++;

	switch (rxState) {

	case RX_PACKET_IDLE:
		p_length = c;
		crc.reset();
		crc.update(c);
		rxState = RX_PACKET_GOTLENGTH;
		break;

	case RX_PACKET_GOTLENGTH:
		p_cmd = c;
		p_crc = 0;
		crc.update(c);
		if (p_length > 5) {
			newBufPtr = rxBufpT;
			rxState = RX_PACKET_GOTCOMMAND;
		} else {
			rxState = RX_PACKET_GOTPAYLOAD;
		}
		break;

	case RX_PACKET_GOTCOMMAND:
		if (!rxBufisFull)
			rxBuf[newBufPtr++] = c;
		rxBufisFull = (rxBufpH == newBufPtr);
		crc.update(c);
		if (byteCount == (p_length - 3))
			rxState = RX_PACKET_GOTPAYLOAD;
		break;

	case RX_PACKET_GOTPAYLOAD:
		// Integrity checking.
		p_crc |= (uint32_t)c << (8 * (byteCount - p_length + 2));

		if (byteCount == (uint16_t)p_length + 1) {
			uint32_t crcCode = crc.finalize();
			byteCount = 0;
			rxState = RX_PACKET_IDLE;

			// Received packet valid.
			if (p_crc == crcCode) {
				boolean rxSeqFlag = (p_cmd & 0x80) > 0;

				switch (p_cmd & 0x7F) {
				// Connection established with destination.
				case PROTOCOL_CONNACK:
					if (p_length == 5)
						state = STATE_CONNECTED;
					break;
				// Upstream tcp connection closed.
				case PROTOCOL_DISCONNECT:
					if (p_length == 5 && (state == STATE_CONNECTED))
						reset();
					break;
				// Incoming data.
				case PROTOCOL_PUBLISH:
					writePacket(PROTOCOL_ACK | (p_cmd & 0x80), NULL, 0);
					if (rxSeqFlag == expectedRxSeqFlag) {
						expectedRxSeqFlag = !expectedRxSeqFlag;
						rxBufpT = newBufPtr;
					}
					break;
				// Protocol Acknowledge.
				case PROTOCOL_ACK:
					if (ackOutstanding && (rxSeqFlag == pubSequence)) {
						stopAckTimer();
						pubSequence = !pubSequence;
						tx_retries = 0;
						ackOutstanding = false;
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
