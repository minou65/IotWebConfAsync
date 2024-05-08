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
#elif defined(ESP32)
#include <AsyncTCP.h>
#endif

#include <ESPAsyncWebServer.h>
#include <IotWebConfWebServerWrapper.h>

class AsyncWebRequestWrapper : public iotwebconf::WebRequestWrapper {
public:
	AsyncWebRequestWrapper(AsyncWebServerRequest* request) 
		: _headers(LinkedList<AsyncWebHeader*>([](AsyncWebHeader* h) { delete h; })),
		_first(false),
		_currentVersion(0),
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
		Serial.printf("Content length: %d\n", contentLength);
	};
	
	void send(int code, const char* content_type = nullptr, const String& content = String("")) override {
		AsyncWebServerResponse* response = this->_request->beginResponse(code, content_type, content.c_str());;

		response->setContentLength(this->_contentLength);
		for (const auto& header : _headers) {
			response->addHeader(header->name().c_str(), header->value().c_str());
			Serial.printf("Header: %s: %s added\n", header->name().c_str(), header->value().c_str());
		};
		this->_request->send(response);
	};

	void sendContent(const String& content) override {
		String content_ = String(content);
		AsyncWebServerResponse* response = this->_request->beginChunkedResponse("text/html", [content_](uint8_t* buffer, size_t maxLen, size_t index) -> size_t {

			String chunk = content_.substring(index, index + maxLen);
			chunk.getBytes(buffer, chunk.length());

			//Serial.println("[WEB] Sending chunk: " + chunk);
			//Serial.printf("[WEB] Sending %d bytes\n", chunk.length());
			//Serial.printf("[WEB] max chunk size: %d\n", maxLen);
			//Serial.printf("[WEB] index: %d\n", index);

			// Return the actual length of the chunk (0 for end of file)
			return chunk.length();

		});

		_request->send(response);
	};

	void stop() override {
		//this->_request->client()->stop();
		this->_request->client()->close();
	};
	
protected:
	AsyncWebServerRequest* _request;
	LinkedList<AsyncWebHeader*> _headers;

	size_t _contentLength;

private:
	AsyncWebRequestWrapper();

	bool _first;
	uint8_t _currentVersion;

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

