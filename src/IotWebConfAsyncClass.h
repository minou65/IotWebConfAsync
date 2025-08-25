/* IotWebConfAsyncClass.h
 *
 * Copyright (c) 2024 Andreas Zogg
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
*/

/************************************************************************//**
 * \file   IotWebConfAsyncClass.h
 *  \brief  This File contains all overrides for the iotwebconf::WebRequestWrapper class. This overrides will be needed to
 *          use an ESPAsyncWebserver.
 * ********************************************************************/

#define IOTWEBCONFASYNC_DEBUG_TO_SERIAL 1 // Set to 1 to enable debug output to serial

#ifndef _IOTWEBCONFASYNCCLASS_h
#define _IOTWEBCONFASYNCCLASS_h

#if defined(ARDUINO) && ARDUINO >= 100
#include "arduino.h"
#else
#include "WProgram.h"
#endif

#ifdef ESP8266
#include <ESPAsyncTCP.h>
#include <Updater.h>
#elif defined(ESP32)
#include <AsyncTCP.h>
#include <Update.h>
#endif

#include <ESPAsyncWebServer.h>
#include <IotWebConfWebServerWrapper.h>
#include <vector>
#include <LittleFS.h>
#include "esp_task_wdt.h"

#if IOTWEBCONFASYNC_DEBUG_TO_SERIAL
bool debugIotAsyncWebRequest = true;
#else
bool debugIotAsyncWebRequest = false;
#endif

#define DEBUGASYNC_PRINT(x) if (debugIotAsyncWebRequest) Serial.print(x) 
#define DEBUGASYNC_PRINTLN(x) if (debugIotAsyncWebRequest) Serial.println(x)
#define DEBUGASYNC_PRINTF(...) if (debugIotAsyncWebRequest) Serial.printf(__VA_ARGS__)

class AsyncWebRequestWrapper : public iotwebconf::WebRequestWrapper {
public:
	AsyncWebRequestWrapper(AsyncWebServerRequest* request) :
		_request(request),
		_response(nullptr),
		_bufferIndex(0),
		_bufferRead(0),
		_isChunked(false),
		_responseSent(false)
	{
		sendHeader("Server", "ESP Async Web Server");
		sendHeader(asyncsrv::T_Cache_Control, "public,max-age=60");
	}

	~AsyncWebRequestWrapper() {
		if (_buffer != nullptr) {
			delete[] _buffer;
			_buffer = nullptr;
		}
	}

	const String hostHeader() const override {
		return this->_request->host();
	};

	IPAddress localIP() override {
		return this->_request->client()->localIP();
	};

	uint16_t localPort() override {
		return this->_request->client()->localPort();
	};

	const String uri() const override {
		return this->_request->url();
	};

	bool authenticate(const char* username, const char* password) override {
		return this->_request->authenticate(username, password);
	};

	void requestAuthentication() override {
		this->_request->requestAuthentication();
	};

	bool hasArg(const String& name) override {
		return this->_request->hasArg(name.c_str());
	};

	String arg(const String name) override {
		return this->_request->arg(name);
	};

	void sendHeader(const String& name, const String& value, bool first = false) override {
		_headers.push_back(std::make_pair(name, value));
	};

	void setContentLength(const size_t contentLength) override {
		this->_contentLength = contentLength;
		if (contentLength == CONTENT_LENGTH_UNKNOWN) {
			_isChunked = true;
			if (_buffer == nullptr) {
				_buffer = new char[BUFFER_SIZE];
				_bufferIndex = 0;
				_bufferRead = 0;
				memset(_buffer, 0, BUFFER_SIZE);
			}
		}
	}

	void send(int code, const char* content_type = nullptr, const String& content = String("")) override {
		DEBUGASYNC_PRINTLN("AsyncWebRequestWrapper::send");
		DEBUGASYNC_PRINT("    Code: "); DEBUGASYNC_PRINTLN(code);
		DEBUGASYNC_PRINT("    Content type: "); DEBUGASYNC_PRINTLN(content_type);
		DEBUGASYNC_PRINT("    Content: "); DEBUGASYNC_PRINTLN(content);
		DEBUGASYNC_PRINT("    Content length: "); DEBUGASYNC_PRINTLN(content.length());

		if (_responseSent) return;

		const char* type = content_type ? content_type : "text/html";

		if (_isChunked) {
			// Nur beim ersten Mal erzeugen!
			if (_response == nullptr) {
				DEBUGASYNC_PRINTLN("    Initialize chunked response");
				// Chunked response initialisieren
				_response = new AsyncChunkedResponse(type, [this](uint8_t* buffer, size_t maxLen, size_t index) -> size_t {
					return this->readChunk(buffer, maxLen);
					});

				// Header vor send() setzen!
				for (auto& h : _headers) {
					_response->addHeader(h.first, h.second);
				}
				_response->setCode(code);
				_request->send(_response);
				_responseSent = true;
			}
		}
		else {
			// Fixed-length response
			AsyncResponseStream* stream = _request->beginResponseStream(type);
			stream->setCode(code);
			stream->setContentLength(_contentLength);
			stream->print(content);

			for (auto& h : _headers) {
				stream->addHeader(h.first, h.second);
			}
			stream->setCode(code);
			_request->send(stream);
			_responseSent = true;
		}
	}

	void sendContent(const String& content) override {
		if (_buffer == nullptr) {
			DEBUGASYNC_PRINTLN("Fehler: Buffer nicht initialisiert!");
			return;
		}
		size_t len = content.length();
		size_t written = 0;

		DEBUGASYNC_PRINTLN("AsyncWebRequestWrapper::sendContent");
		DEBUGASYNC_PRINT("    Content length: "); DEBUGASYNC_PRINTLN(len);

		while (written < len) {
			size_t space = availableSpace();
			if (space == 0) {
				waitForBufferSpace(1); // Warte, bis wieder Platz ist
				continue;
			}
			size_t toWrite = std::min(space, len - written);
			memcpy(_buffer + _bufferIndex, content.c_str() + written, toWrite);
			_bufferIndex += toWrite;
			written += toWrite;
			DEBUGASYNC_PRINT("    Buffer index nach write: "); DEBUGASYNC_PRINTLN(_bufferIndex);
		}
	}

	void stop() override {
		DEBUGASYNC_PRINTLN("AsyncWebRequestWrapper::stop");
		_finished = true; // Signalisiere readChunk das Ende
	}

protected:
	AsyncWebServerRequest* _request;
	AsyncWebServerResponse* _response = nullptr;
	std::vector<std::pair<String, String>> _headers;
	size_t _contentLength;
	bool _isChunked;
	bool _responseSent;
	bool _finished = false;
	char* _buffer = nullptr;
	size_t _bufferIndex = 0;
	size_t _bufferRead = 0;
	static constexpr size_t BUFFER_SIZE = 90000;

	size_t availableSpace() const {
		return BUFFER_SIZE - _bufferIndex;
	}

	void waitForBufferSpace(size_t neededSpace) {
		while (availableSpace() < neededSpace) {
			yield(); // Kurzes Warten, damit der Event-Loop weiterläuft
			esp_task_wdt_reset();
		}
	}

	size_t readChunk(uint8_t* buffer, size_t maxLen) {
		DEBUGASYNC_PRINTLN("AsyncWebRequestWrapper::readChunk");
		if (_buffer == nullptr) {
			DEBUGASYNC_PRINTLN("  Fehler: Buffer nicht initialisiert!");
			return 0;
		}
		size_t available = _bufferIndex - _bufferRead;
		DEBUGASYNC_PRINT("  Buffer-Status: _bufferIndex="); DEBUGASYNC_PRINT(_bufferIndex);
		DEBUGASYNC_PRINT(", _bufferRead="); DEBUGASYNC_PRINT(_bufferRead);
		DEBUGASYNC_PRINT(", available="); DEBUGASYNC_PRINTLN(available);
		DEBUGASYNC_PRINT("  maxLen: "); DEBUGASYNC_PRINTLN(maxLen);

		if (available == 0) {
			if (_finished) {
				DEBUGASYNC_PRINTLN("  Buffer leer und Übertragung abgeschlossen, Objekt wird gelöscht.");
				delete this;
			}
			else {
				DEBUGASYNC_PRINTLN("  Keine Daten im Buffer (Ende oder warten auf mehr Daten)");
			}
			return 0;
		}
		size_t len = std::min(maxLen, available);
		DEBUGASYNC_PRINT("  Übertrage Bytes: "); DEBUGASYNC_PRINTLN(len);

		memcpy(buffer, _buffer + _bufferRead, len);
		_bufferRead += len;

		DEBUGASYNC_PRINT("  Neuer _bufferRead: "); DEBUGASYNC_PRINTLN(_bufferRead);

		if (_bufferRead == _bufferIndex) {
			DEBUGASYNC_PRINTLN("  Buffer geleert, Zeiger zurücksetzen.");
			_bufferRead = 0;
			_bufferIndex = 0;
		}
		return len;
	}

	friend class IotWebConf;
	friend class AsyncWebServerRequest;
};

class AsyncWebServerWrapper : public iotwebconf::WebServerWrapper {
public:
	AsyncWebServerWrapper(AsyncWebServer* server) { this->_server = server; };

	void handleClient() override {};
	void begin() override { this->_server->begin(); };

private:
	AsyncWebServer* _server;
	AsyncWebServerWrapper() {};

	friend class IotWebConf;
	friend class AsyncWebServer;
};


class AsyncWebRequestLittleFSWrapper : public AsyncWebRequestWrapper
{
public:
	AsyncWebRequestLittleFSWrapper(AsyncWebServerRequest* request)
		: AsyncWebRequestWrapper(request)
	{}

	~AsyncWebRequestLittleFSWrapper() {
	}

	void sendContent(const String& content) override {
		if (_isChunked) {
			if (!_file) {
				_file = LittleFS.open("/myhtml.html", "a");
				if (!_file) {
					Serial.println("Error opening /myhtml.html!");
					return;
				}
			}
			else {
				_file.print(content);
				esp_task_wdt_reset();
			}
		}
		else {
			DEBUGASYNC_PRINTLN("    Non-chunked response");
			AsyncWebRequestWrapper::sendContent(content);
		}
	}

	void stop() override {
		if (_isChunked) {
			_file.close();

			File file = LittleFS.open("/myhtml.html", "r");
			if (file) {
				AsyncWebServerResponse* response = _request->beginChunkedResponse(
					"text/html",
					[file](uint8_t* buffer, size_t maxLen, size_t index) mutable -> size_t {
						size_t bytesRead = file.read(buffer, maxLen);
						if (bytesRead == 0) {
							file.close();
							LittleFS.remove("/myhtml.html");
						}
						return bytesRead;
					}
				);

				for (const auto& header : _headers) {
					response->addHeader(header.first, header.second);
				}
				_request->send(response);
				_headers.clear();
			}
			else {
				Serial.println("File /myhtml.html not found!");
				_request->send(404, "text/plain", "File not found: /myhtml.html");
			}

		}
		else {
			AsyncWebRequestWrapper::stop();
		}
	}

protected:
	File _file;
};

#endif