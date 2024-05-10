// IotWebConfAsyncClass.h

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
#include <StreamString.h>

class AsyncWebRequestWrapper : public iotwebconf::WebRequestWrapper {
public:
	AsyncWebRequestWrapper(AsyncWebServerRequest* request)
		: _headers(LinkedList<AsyncWebHeader*>([](AsyncWebHeader* h) { delete h; })),
		_first(false),
		_contentLength(0)
	{
		this->_request = request;
	};

	~AsyncWebRequestWrapper() {
		_headers.free();
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
		_headers.add(new AsyncWebHeader(name.c_str(), value.c_str()));
		this->_first = first;

	};

	void setContentLength(const size_t contentLength) override {
		this->_contentLength = contentLength;
	};

	void send(int code, const char* content_type = nullptr, const String& content = String("")) override {
		AsyncWebServerResponse* response = this->_request->beginResponse(code, content_type, content.c_str());;
		for (const auto& header : _headers) {
			response->addHeader(header->name().c_str(), header->value().c_str());
		};

		if (!isHeaderInList("Content-Length")) {
			response->setContentLength(this->_contentLength);
		}

		this->_request->send(response);

	};

	void sendContent(const String& content) override {
		std::string content_ = content.c_str();
		_content.push_back(content_);
	};

	void stop() override {

		//Serial.println("[WEB] Stopping request");
		std::vector<std::string> content_ = _content;
		std::shared_ptr<uint16_t> counter_ = std::make_shared<uint16_t>(0);

		AsyncWebServerResponse* response = this->_request->beginChunkedResponse("text/html", [content_, counter_](uint8_t* buffer, size_t maxLen, size_t index) -> size_t {

			std::string chunk_ = "";

			for (*counter_; *counter_ < content_.size(); *counter_ += 1) {
				//Serial.printf("[CHUNK] counter: %i\n", *counter_);
				if (chunk_.length() + content_[*counter_].length() < maxLen) {
					//Serial.println("[CHUNK] adding chunk");
					chunk_ += content_[*counter_];
				}
				else {
					//Serial.println("[CHUNK] breaking");
					//Serial.printf("[CHUNK] index: %i\n", index);
					//Serial.printf("[CHUNK] maxLen: %i\n", maxLen);
					//Serial.printf("[CHUNK] chunk space: %i\n", chunk_.length() + content_[*counter_].length());
					if (chunk_.length() == 0) {
						chunk_ = "\r\n";
					}
					break;
				}
			}
			//Serial.printf("[CHUNK] sending: %s\n", chunk_.c_str());
			//Serial.printf("[CHUNK] sending: %d bytes\n", chunk_.length());
			chunk_.copy((char*)buffer, chunk_.length());
			if (*counter_ <= content_.size()) {
				return chunk_.length();
			}
			return 0;
			});
		this->_request->send(response);
	};

protected:
	AsyncWebServerRequest* _request;
	LinkedList<AsyncWebHeader*> _headers;
	std::vector<std::string> _content;

	size_t _contentLength;

private:
	AsyncWebRequestWrapper();

	bool _first;

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

