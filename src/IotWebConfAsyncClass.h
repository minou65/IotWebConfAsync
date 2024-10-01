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


class AsyncWebRequestWrapper : public iotwebconf::WebRequestWrapper {
public:
	AsyncWebRequestWrapper(AsyncWebServerRequest* request) {
		this->_request = request;
		_first = false;
		_contentLength = 0;
	};

	~AsyncWebRequestWrapper() {
		for (auto header : _headers) {
			delete header;
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
		_headers.push_back(new AsyncWebHeader(name.c_str(), value.c_str()));
		this->_first = first;

	};

	void setContentLength(const size_t contentLength) override {
		this->_contentLength = contentLength;
	};

	void send(int code, const char* content_type = nullptr, const String& content = String("")) override {
		AsyncWebServerResponse* response = this->_request->beginResponse(code, content_type, content.c_str());
		for (const auto& header : _headers) {
			response->addHeader(header->name().c_str(), header->value().c_str());
		};

		if (!isHeaderInList("Content-Length")) {
			response->setContentLength(this->_contentLength);
		}

		this->_request->send(response);

	};

	void sendContent(const String& content) override {
		//Serial.println("AsyncWebRequestWrapper::sendContent()");
		//Serial.print("    Adding content of length: "); Serial.println(content.length());

		// Überprüfen Sie den freien Speicher
		//size_t freeHeap = ESP.getFreeHeap();
		//Serial.print("    Free heap before adding content: "); Serial.println(freeHeap);

		// Überprüfen Sie die Gültigkeit von content
		if (content.length() == 0) {
			ESP_LOGW("AsyncWebRequestWrapper", "Warning: content is empty");
			return;
		}

		// Überprüfen Sie, ob genügend freier Speicher vorhanden ist
		//if (freeHeap < content.length()) {
		//	//Serial.println("    Error: not enough free heap to add content");
		//	return;
		//}

		// Fügen Sie den Inhalt zu _content hinzu
		try {
			String convertedContent = content;
			convertedContent.replace("\r\n", "\n");

			_content.insert(_content.end(), convertedContent.begin(), convertedContent.end());
		}
		catch (const std::exception& e) {
			ESP_LOGE("AsyncWebRequestWrapper", "Error: exception caught while adding content: %s", e.what());
			return;
		}
		catch (...) {
			ESP_LOGE("AsyncWebRequestWrapper", "Error: unknown exception caught while adding content");
			return;
		}

		//Serial.print("    Free heap after adding content: "); Serial.println(ESP.getFreeHeap());
		//Serial.print("    Total content length: "); Serial.println(_content.size());
	}

	void stop() override {
		//Serial.println("AsyncWebRequestWrapper::stop()");

		if (_content.empty()) {
			//Serial.println("    Error: _content is empty");
			return;
		}

		// Überprüfen Sie den freien Speicher
		//size_t freeHeap = ESP.getFreeHeap();
		//Serial.print("    Free heap before creating cache_: "); Serial.println(freeHeap);

		// Überprüfen Sie, ob genügend freier Speicher vorhanden ist
		//if (freeHeap < _content.size() * sizeof(char)) {
		//	Serial.println("    Error: not enough free heap to create cache_");
		//	return;
		//}

		// Erstellen eines shared_ptr für _content
		try {
			auto cache_ = std::make_shared<std::vector<char>>(_content);
			Serial.print("    Total content length: "); Serial.println(_content.size());

			AsyncWebServerResponse* response = this->_request->beginChunkedResponse("text/html", [cache_](uint8_t* buffer, size_t maxLen, size_t index) -> size_t {
				size_t len_ = std::min(cache_->size() - index, maxLen);
				//Serial.print("    Index: "); Serial.print(index);
				//Serial.print("    Len: "); Serial.println(len_);
				//Serial.print("    MaxLen: "); Serial.println(maxLen);
				//Serial.print("    Cache length: "); Serial.println(cache_->size());

				// Überprüfen, ob die Länge und die Speicheradresse gültig sind
				if (len_ > 0 && index < cache_->size()) {
					//Serial.println("    Copying chunk");
					memcpy(buffer, cache_->data() + index, len_);
					return len_;
				}
				else {
					//Serial.println("    Error: Invalid length or index");
					return 0; // Länge auf 0 setzen, um sicherzustellen, dass keine ungültigen Daten kopiert werden
				}
				});

			response->addHeader("Server", "ESP Async Web Server");
			this->_request->send(response);
		}
		catch (const std::bad_alloc& e) {
			ESP_LOGE("AsyncWebRequestWrapper", "Error: bad_alloc exception caught");
		}
		catch (const std::exception& e) {
			ESP_LOGE("AsyncWebRequestWrapper", "Error: exception caught: %s", e.what());
		}
		catch (...) {
			ESP_LOGE("AsyncWebRequestWrapper", "Error: unknown exception caught");
		}

		// Zusätzliche Debugging-Informationen
		//Serial.println("AsyncWebRequestWrapper::stop() completed");
	}




protected:
	AsyncWebServerRequest* _request;
	std::list<AsyncWebHeader*> _headers;
	std::vector<char> _content;

	size_t _contentLength;
	bool _first;

private:
	AsyncWebRequestWrapper();

	bool isHeaderInList(const char* name) {
		for (AsyncWebHeader* header : _headers) {
			if (strcmp(header->name().c_str(), name) == 0) {
				return true;
			}
		}
		return false;
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