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

#ifndef IOTWEBCONFASYNC_DEBUG_TO_SERIAL
#define IOTWEBCONFASYNC_DEBUG_TO_SERIAL 0 // Set to 1 to enable debug output to serial
#endif

#if defined(ARDUINO) && ARDUINO >= 100
#include "arduino.h"
#else
#include "WProgram.h"
#endif

#include <ESPAsyncWebServer.h>
#include <IotWebConfWebServerWrapper.h>
#include <queue>

#ifdef ESP8266
#include <ESPAsyncTCP.h>
#include <Updater.h>
#elif defined(ESP32)
#include <AsyncTCP.h>
#include <Update.h>
#endif

extern bool debugIotAsyncWebRequest;
#define DEBUGASYNC_PRINT(x) if (debugIotAsyncWebRequest) Serial.print(x) 
#define DEBUGASYNC_PRINTLN(x) if (debugIotAsyncWebRequest) Serial.println(x)
#define DEBUGASYNC_PRINTF(...) if (debugIotAsyncWebRequest) Serial.printf(__VA_ARGS__)

class AsyncWebRequestWrapper : public iotwebconf::WebRequestWrapper
{
public:
    explicit AsyncWebRequestWrapper(AsyncWebServerRequest* request);
    ~AsyncWebRequestWrapper(){}

	void send(int code, const char* content_type = nullptr, const String& content = String("")) override;
    void sendHeader(const String& name, const String& value, bool first = false) override;
    void sendContent(const String& content) override;
    void setContentLength(const size_t contentLength) override;
    void stop() override;

    const String hostHeader() const override { return _request->host(); }
    IPAddress localIP() override { return _request->client()->localIP(); }
    uint16_t localPort() override { return _request->client()->localPort(); }
    const String uri() const override { return _request->url(); }
    bool authenticate(const char* username, const char* password) override { return _request->authenticate(username, password); }
    void requestAuthentication() override { _request->requestAuthentication(); }
    bool hasArg(const String& name) override { return _request->hasArg(name.c_str()); }
    String arg(const String name) override { return _request->arg(name); }

	bool isChunkQueueEmpty() const { return _chunkQueue.empty(); }

protected:
    AsyncWebServerRequest* _request;
    AsyncWebServerResponse* _response;
    std::vector<std::pair<String, String>> _headers;
    std::queue<String> _chunkQueue;
    size_t _contentLength;
	String _contentType;
    bool _isChunked;
	bool _isFinished;
    
    size_t readChunk(uint8_t* buffer, size_t maxLen);

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

#endif