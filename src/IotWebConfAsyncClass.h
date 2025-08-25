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


#ifndef IOTWEBCONFASYNC_DEBUG_TO_SERIAL
#define IOTWEBCONFASYNC_DEBUG_TO_SERIAL 0 // Set to 1 to enable debug output to serial
#endif

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

#if IOTWEBCONFASYNC_DEBUG_TO_SERIAL == 1
bool debugIotAsyncWebRequest = true;
#else
bool debugIotAsyncWebRequest = false;
#endif

#define DEBUGASYNC_PRINT(x) if (debugIotAsyncWebRequest) Serial.print(x) 
#define DEBUGASYNC_PRINTLN(x) if (debugIotAsyncWebRequest) Serial.println(x)
#define DEBUGASYNC_PRINTF(...) if (debugIotAsyncWebRequest) Serial.printf(__VA_ARGS__)

class AsyncWebRequestWrapper : public iotwebconf::WebRequestWrapper
{
public:
    explicit AsyncWebRequestWrapper(AsyncWebServerRequest* request)
        : _request(request),
        _response(nullptr),
        _contentLength(0),
        _isChunked(false),
        _responseSent(false),
        _finished(false),
        _buffer(nullptr),
        _bufferIndex(0),
        _bufferRead(0)
    {
        sendHeader("Server", "ESP Async Web Server");
        sendHeader(asyncsrv::T_Cache_Control, "public,max-age=60");
    }

    ~AsyncWebRequestWrapper()
    {
        if (_buffer != nullptr)
        {
            delete[] _buffer;
            _buffer = nullptr;
        }
    }

    const String hostHeader() const override { return _request->host(); }
    IPAddress localIP() override { return _request->client()->localIP(); }
    uint16_t localPort() override { return _request->client()->localPort(); }
    const String uri() const override { return _request->url(); }
    bool authenticate(const char* username, const char* password) override { return _request->authenticate(username, password); }
    void requestAuthentication() override { _request->requestAuthentication(); }
    bool hasArg(const String& name) override { return _request->hasArg(name.c_str()); }
    String arg(const String name) override { return _request->arg(name); }

    void sendHeader(const String& name, const String& value, bool first = false) override
    {
        _headers.emplace_back(name, value);
    }

    void setContentLength(const size_t contentLength) override
    {
        _contentLength = contentLength;
        if (contentLength == CONTENT_LENGTH_UNKNOWN)
        {
            _isChunked = true;
            if (_buffer == nullptr)
            {
                _buffer = new char[BUFFER_SIZE]();
                _bufferIndex = 0;
                _bufferRead = 0;
            }
        }
    }

    void send(int code, const char* content_type = nullptr, const String& content = String("")) override
    {
        DEBUGASYNC_PRINTLN("AsyncWebRequestWrapper::send");
        DEBUGASYNC_PRINT("    Code: "); DEBUGASYNC_PRINTLN(code);
        DEBUGASYNC_PRINT("    Content type: "); DEBUGASYNC_PRINTLN(content_type);
        DEBUGASYNC_PRINT("    Content: "); DEBUGASYNC_PRINTLN(content);
        DEBUGASYNC_PRINT("    Content length: "); DEBUGASYNC_PRINTLN(content.length());

        if (_responseSent) return;

        const char* type = content_type ? content_type : "text/html";

        if (_isChunked)
        {
            if (_response == nullptr)
            {
                DEBUGASYNC_PRINTLN("    Initialize chunked response");
                _response = new AsyncChunkedResponse(type, [this](uint8_t* buffer, size_t maxLen, size_t) {
                    return this->readChunk(buffer, maxLen);
                    });
                for (const auto& h : _headers) _response->addHeader(h.first, h.second);
                _response->setCode(code);
                _request->send(_response);
                _responseSent = true;
            }
        }
        else
        {
            auto* stream = _request->beginResponseStream(type);
            stream->setCode(code);
            stream->setContentLength(_contentLength);
            stream->print(content);
            for (const auto& h : _headers) stream->addHeader(h.first, h.second);
            _request->send(stream);
            _responseSent = true;
        }
    }

    void sendContent(const String& content) override
    {
        if (_buffer == nullptr)
        {
            DEBUGASYNC_PRINTLN("Error: Buffer not initialized!");
            return;
        }
        size_t len = content.length();
        size_t written = 0;

        DEBUGASYNC_PRINTLN("AsyncWebRequestWrapper::sendContent");
        DEBUGASYNC_PRINT("    Content length: "); DEBUGASYNC_PRINTLN(len);

        while (written < len)
        {
            size_t space = availableSpace();
            if (space == 0)
            {
                waitForBufferSpace(1);
                continue;
            }
            size_t toWrite = std::min(space, len - written);
            memcpy(_buffer + _bufferIndex, content.c_str() + written, toWrite);
            _bufferIndex += toWrite;
            written += toWrite;
            DEBUGASYNC_PRINT("    Buffer index after write: "); DEBUGASYNC_PRINTLN(_bufferIndex);
        }
    }

    void stop() override
    {
        DEBUGASYNC_PRINTLN("AsyncWebRequestWrapper::stop");
        _finished = true;
    }

protected:
    AsyncWebServerRequest* _request;
    AsyncWebServerResponse* _response;
    std::vector<std::pair<String, String>> _headers;
    size_t _contentLength;
    bool _isChunked;
    bool _responseSent;
    bool _finished;
    char* _buffer;
    size_t _bufferIndex;
    size_t _bufferRead;
    static constexpr size_t BUFFER_SIZE = 90000;

    size_t availableSpace() const { return BUFFER_SIZE - _bufferIndex; }

    void waitForBufferSpace(size_t neededSpace)
    {
        while (availableSpace() < neededSpace)
        {
            yield();
            esp_task_wdt_reset();
        }
    }

    size_t readChunk(uint8_t* buffer, size_t maxLen)
    {
        DEBUGASYNC_PRINTLN("AsyncWebRequestWrapper::readChunk");
        if (_buffer == nullptr)
        {
            DEBUGASYNC_PRINTLN("  Error: Buffer not initialized!");
            return 0;
        }
        size_t available = _bufferIndex - _bufferRead;
        DEBUGASYNC_PRINT("  Buffer status: _bufferIndex="); DEBUGASYNC_PRINT(_bufferIndex);
        DEBUGASYNC_PRINT(", _bufferRead="); DEBUGASYNC_PRINT(_bufferRead);
        DEBUGASYNC_PRINT(", available="); DEBUGASYNC_PRINTLN(available);
        DEBUGASYNC_PRINT("  maxLen: "); DEBUGASYNC_PRINTLN(maxLen);

        if (available == 0)
        {
            if (_finished)
            {
                DEBUGASYNC_PRINTLN("  Buffer empty and transfer finished, deleting object.");
                delete this;
            }
            else
            {
                DEBUGASYNC_PRINTLN("  No data in buffer (waiting for more data or end).");
            }
            return 0;
        }
        size_t len = std::min(maxLen, available);
        DEBUGASYNC_PRINT("  Transferring bytes: "); DEBUGASYNC_PRINTLN(len);

        memcpy(buffer, _buffer + _bufferRead, len);
        _bufferRead += len;

        DEBUGASYNC_PRINT("  New _bufferRead: "); DEBUGASYNC_PRINTLN(_bufferRead);

        if (_bufferRead == _bufferIndex)
        {
            DEBUGASYNC_PRINTLN("  Buffer emptied, resetting pointers.");
            _bufferRead = 0;
            _bufferIndex = 0;
        }
        return len;
    }

    friend class IotWebConf;
    friend class AsyncWebServerRequest;
};

class AsyncWebRequestLittleFSWrapper : public AsyncWebRequestWrapper
{
public:
    explicit AsyncWebRequestLittleFSWrapper(AsyncWebServerRequest* request)
        : AsyncWebRequestWrapper(request)
    {
    }

    ~AsyncWebRequestLittleFSWrapper()
    {
        if (_file) {
            _file.close();
        }
    }

    void sendContent(const String& content) override
    {
        if (_isChunked)
        {
            if (!_file)
            {
                _file = LittleFS.open("/myhtml.html", "a");
                if (!_file)
                {
                    Serial.println("Error opening /myhtml.html!");
                    return;
                }
            }
            _file.print(content);
            esp_task_wdt_reset();
        }
        else
        {
            DEBUGASYNC_PRINTLN("    Non-chunked response");
            AsyncWebRequestWrapper::sendContent(content);
        }
    }

    void stop() override
    {
        if (_isChunked)
        {
            if (_file) {
                _file.close();
            }

            File file = LittleFS.open("/myhtml.html", "r");
            if (file)
            {
                AsyncWebServerResponse* response = _request->beginChunkedResponse(
                    "text/html",
                    [file](uint8_t* buffer, size_t maxLen, size_t) mutable -> size_t {
                        size_t bytesRead = file.read(buffer, maxLen);
                        if (bytesRead == 0)
                        {
                            file.close();
                            LittleFS.remove("/myhtml.html");
                        }
                        return bytesRead;
                    }
                );

                for (const auto& header : _headers)
                {
                    response->addHeader(header.first, header.second);
                }
                _request->send(response);
                _headers.clear();
            }
            else
            {
                Serial.println("File /myhtml.html not found!");
                _request->send(404, "text/plain", "File not found: /myhtml.html");
            }
        }
        else
        {
            AsyncWebRequestWrapper::stop();
        }
    }

protected:
    File _file;
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