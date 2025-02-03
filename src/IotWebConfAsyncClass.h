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

#define IOTWEBCONFASYNC_DEBUG_TO_SERIAL 0 // Set to 1 to enable debug output to serial

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
	AsyncWebRequestWrapper(AsyncWebServerRequest* request, int bufferSize = 2048) {
		this->_request = request;

		beginResponseStream(bufferSize);
		sendHeader("Server", "ESP Async Web Server");
	};

	~AsyncWebRequestWrapper() {
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
		_responseStream->addHeader(name.c_str(), value.c_str());
	};

	void setContentLength(const size_t contentLength) override {
		this->_contentLength = contentLength;
		if (contentLength == CONTENT_LENGTH_UNKNOWN) {
			_isChunked = true;
		}
	};

	void send(int code, const char* content_type = nullptr, const String& content = String("")) override {
		DEBUGASYNC_PRINTLN("AsyncWebRequestWrapper::send");
		DEBUGASYNC_PRINT("    Code: "); DEBUGASYNC_PRINTLN(code);
		DEBUGASYNC_PRINT("    Content type: "); DEBUGASYNC_PRINTLN(content_type);
		DEBUGASYNC_PRINT("    Content: "); DEBUGASYNC_PRINTLN(content);
		DEBUGASYNC_PRINT("    Content length: "); DEBUGASYNC_PRINTLN(content.length());

		try {
			if (_isChunked) {
				DEBUGASYNC_PRINTLN("    Chunked response");
				_responseStream->print(content);
			}
			else {
				DEBUGASYNC_PRINTLN("    Non-chunked response");
				AsyncWebServerResponse* response_ = _request->beginResponse(code, content_type, content);
				response_->addHeader("Server", "ESP Async Web Server");
				response_->addHeader("Content-Length", content.length());
				_request->send(response_);
			}
		}
		catch (const std::exception& e) {
			DEBUGASYNC_PRINTLN(e.what());
		}
		catch (...) {
			DEBUGASYNC_PRINTLN("Unknown exception");
		}
	};

	void sendContent(const String& content) override {
		DEBUGASYNC_PRINTLN("AsyncWebRequestWrapper::sendContent");

		try {
			DEBUGASYNC_PRINT("Content length: "); DEBUGASYNC_PRINTLN(content.length());
			_responseStream->print(content);
		}
		catch (const std::exception& e) {
			DEBUGASYNC_PRINTLN(e.what());
		}
		catch (...) {
			DEBUGASYNC_PRINTLN("Unknown exception");
		}

	};

	
	void stop() override {
		DEBUGASYNC_PRINTLN("AsyncWebRequestWrapper::stop");
		_request->send(_responseStream);
	};

protected:
	AsyncWebServerRequest* _request;
	AsyncResponseStream* _responseStream;


private:
	AsyncWebRequestWrapper();

	bool _isChunked = false;
	size_t _contentLength = CONTENT_LENGTH_UNKNOWN;
	int _bufferSize;


	void beginResponseStream(int bufferSize = 1024) {
		if (bufferSize <= 0) {
			bufferSize = 1024;
		}
		_responseStream = _request->beginResponseStream("text/html", bufferSize);
		_bufferSize = bufferSize;
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
	AsyncWebServerWrapper() {};
	AsyncWebServer* _server;

	friend class IotWebConf;
	friend class AsyncWebServer;
};

#endif