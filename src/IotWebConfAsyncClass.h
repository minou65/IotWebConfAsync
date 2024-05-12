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


class AsyncWebRequestWrapper : public iotwebconf::WebRequestWrapper {
public:
	AsyncWebRequestWrapper(AsyncWebServerRequest* request) 
		: _headers(LinkedList<AsyncWebHeader*>([](AsyncWebHeader* h) { delete h; })),
		_first(false),
		_content("")
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
		_content += content.c_str();
	};

	void stop() override {
		std::string cache_ = _content;

		AsyncWebServerResponse* response = this->_request->beginChunkedResponse("text/html", [cache_](uint8_t* buffer, size_t maxLen, size_t index) -> size_t {

			std::string chunk_ = "";
			size_t len_ = min(cache_.length() - index, maxLen);
			if (len_ > 0) {
				chunk_ = cache_.substr(index, len_);
				chunk_.copy((char*)buffer, chunk_.length());
			}
			if (index + len_ <= cache_.length())
				return chunk_.length();
			else
				return 0;

			});
		response->addHeader("Server", "ESP Async Web Server");
		this->_request->send(response);
	};
	
protected:
	AsyncWebServerRequest* _request;
	LinkedList<AsyncWebHeader*> _headers;
	std::string  _content;

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
	AsyncWebServerWrapper(AsyncWebServer* server) { this->_server = server;  };

	void handleClient() override {};
	void begin() override { this->_server->begin(); };

private:
	AsyncWebServerWrapper() {};
	AsyncWebServer* _server;

	friend class IotWebConf;
	friend class AsyncWebServer;
};

#endif

