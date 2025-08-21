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
	AsyncWebRequestWrapper(AsyncWebServerRequest* request) {
		this->_request = request;
		sendHeader("Server", "ESP Async Web Server");
		sendHeader(asyncsrv::T_Cache_Control, "public,max-age=60");
	}

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
		_headers.push_back(std::make_pair(name, value));
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

		if (_isChunked) {
			DEBUGASYNC_PRINTLN("    Chunked response");
			sendContent(content);
		}
		else {
			DEBUGASYNC_PRINTLN("    Non-chunked response");
			AsyncWebServerResponse* response_ = _request->beginResponse(code, content_type, content);
			// add headers to the response
			for (const auto& header_ : _headers) {
				response_->addHeader(header_.first, header_.second);
			}
			response_->addHeader("Content-Length", content.length());
			_request->send(response_);
			_headers.clear();
		}

	};

	void sendContent(const String& content) override {
		DEBUGASYNC_PRINTLN("AsyncWebRequestWrapper::sendContent");
		_content += content;
	}

	void stop() override {
		DEBUGASYNC_PRINTLN("AsyncWebRequestWrapper::stop");
		DEBUGASYNC_PRINT("    Content length: "); DEBUGASYNC_PRINTLN(_content.length());
		size_t len = _content.length();
		if (len > 0) {
			// create a response object
			AsyncWebServerResponse* response_ = _request->beginResponse(200, "text/html", _content);

			// add headers to the response
			for (const auto& header : _headers) {
				response_->addHeader(header.first, header.second);
			}

			// send the response
			_request->send(response_);
			_headers.clear();
		} else {
			DEBUGASYNC_PRINTLN("    No content to send");
			// If no content, send a 204 No Content response
			AsyncWebServerResponse* response_ = _request->beginResponse(204);
			for (const auto& header : _headers) {
				response_->addHeader(header.first, header.second);
			}
			_request->send(response_);
			_headers.clear();
		}
	}

protected:
	AsyncWebServerRequest* _request;
	
	AsyncWebRequestWrapper();

	bool _isChunked = false;
	size_t _contentLength = CONTENT_LENGTH_UNKNOWN;
	String _content = "";
	std::vector<std::pair<String, String>> _headers;

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