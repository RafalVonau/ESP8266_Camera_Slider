#include <stdarg.h>
#include <inttypes.h>
#include <ESP8266WiFi.h>
#include "ESPAsyncUDP.h"

#if 0
#undef DEBUG_ENABLED
#define pdebug(fmt, args...)
#define pwrite(fmt, len)
#else
#define DEBUG_ENABLED
#define pdebug(fmt, args...) UdpLogger.printf(fmt, ## args)
#define pwrite(fmt, len) UdpLogger.print(fmt);
//#define pdebug(fmt, args...) Serial.printf(fmt, ## args)
//#define pwrite(fmt, len) Serial.write(fmt, len)
#endif


class UdpLoggerClass
{
	AsyncUDP _udp;
	int _port;
	String _current;
	String _prefix;

public:
	UdpLoggerClass()    { _port = 12345; }
	void init(int port) { _port = port;  }
	void init(int port, const char* prefix) {
		_port = port;
		_prefix = prefix;
		_current = prefix;
	}

	void WriteStartMessage() {
		println("Logging");
	}

	void print(int number) {
		char buffer[33];
		itoa(number, buffer, 10);
		print(buffer);
	}

	void print(String message) {
		_current += message;
	}

	void println(String message) {
		print(message);
		transmit();
	}

	void println(int number) {
		print(number);
		transmit();
	}


	void transmit() {
		_udp.broadcastTo(_current.c_str(), _port);
		_current = _prefix;
	}

	size_t printf(const char * format, ...)  __attribute__ ((format (printf, 2, 3))) {
		va_list arg;
		va_start(arg, format);
		char temp[64];
		char* buffer = temp;
		size_t len = vsnprintf(temp, sizeof(temp), format, arg);
		va_end(arg);
		if (len > sizeof(temp) - 1) {
			buffer = new char[len + 1];
			if (!buffer) {return 0;}
			va_start(arg, format);
			vsnprintf(buffer, len + 1, format, arg);
			va_end(arg);
		}
		this->print(buffer);
		if (buffer != temp) {delete[] buffer;}
		transmit();
		return len;
	}
};

extern UdpLoggerClass UdpLogger;
