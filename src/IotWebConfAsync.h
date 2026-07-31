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

#ifndef _IOTWEBCONFASYNC_h
#define _IOTWEBCONFASYNC_h

#if defined(ARDUINO) && ARDUINO >= 100
#include "arduino.h"
#else
#include "WProgram.h"
#endif

#include <IotWebConf.h>
#include <IotWebConfOptionalGroup.h>
#include <IotWebConfWebServerWrapper.h>
#include <ESPAsyncWebServer.h>

#ifdef ESP8266
#include <ESPAsyncTCP.h>
#include <Updater.h>
#elif defined(ESP32)
#include <AsyncTCP.h>
#include <Update.h>
#endif

#include <DNSServer.h> 

class AsyncIotWebConf;

/**
 * Custom HTML format provider that combines tab support with optional groups
 */
class TabOptionalGroupHtmlFormatProvider : public iotwebconf::OptionalGroupHtmlFormatProvider {
protected:
    virtual String getStyleInner() override {
        String style_ = iotwebconf::OptionalGroupHtmlFormatProvider::getStyleInner();

        // Add Tab styling
        style_ += F("body > div{min-width:260px;max-width:600px;width:100%;box-sizing:border-box;}\n");
        style_ += F(".tab{overflow:hidden;border-bottom:2px solid #16A1E7;background-color:#f1f1f1;margin-bottom:10px;display:flex;}\n");
        style_ += F(".tab button{background-color:#f1f1f1 !important;flex:1 1 0;min-width:0;border:1px solid #ccc !important;outline:none;cursor:pointer;");
        style_ += F("padding:14px 16px;transition:0.3s;font-size:16px;border-top-left-radius:5px;border-top-right-radius:5px;");
        style_ += F("margin-right:2px;border-bottom:none !important;color:#333 !important;line-height:normal !important;width:auto !important;box-sizing:border-box;}\n");
        style_ += F(".tab button:hover{background-color:#ddd !important;}\n");
        style_ += F(".tab button.active{background-color:#16A1E7 !important;color:white !important;border:1px solid #16A1E7 !important;");
        style_ += F("border-bottom:2px solid #fff !important;position:relative;z-index:1;}\n");
        style_ += F(".tabcontent{display:none;padding:12px;border:1px solid #ccc;border-top:none;background-color:#fff;}\n");
        style_ += F("fieldset{width:100%;box-sizing:border-box;}\n");

        return style_;
    }
};


class AsyncWebRequestWrapper : public iotwebconf::WebRequestWrapper {
public:
    explicit AsyncWebRequestWrapper(AsyncWebServerRequest* request);
    ~AsyncWebRequestWrapper() {}

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

    void setConfiguration(AsyncIotWebConf* configuration);

protected:
    AsyncWebServerRequest* _request;
    AsyncWebServerResponse* _response;
    AsyncIotWebConf* _configuration;
    std::vector<std::pair<String, String>> _headers;
    size_t _contentLength;
    String _contentType;
    bool _isChunked;
    bool _isFinished;

    size_t readChunk(uint8_t* buffer, size_t maxLen);

    friend class AsyncIotWebConf;
};

class AsyncWebServerWrapper : public iotwebconf::WebServerWrapper {
public:
    AsyncWebServerWrapper(AsyncWebServer* server) { this->_server = server; };

    void handleClient() override {};
    void begin() override { this->_server->begin(); };
private:
    AsyncWebServer* _server;
    AsyncWebServerWrapper() {};
};

class AsyncIotWebConf : public iotwebconf::IotWebConf {
public:
    enum ChunkStep {
        CHUNK_HEAD,
        CHUNK_SCRIPT,
        CHUNK_STYLE,
        CHUNK_HEADEXT,
        CHUNK_HEADEND,
        CHUNK_FORMSTART,
        CHUNK_SYSTEMPARAMS,
        CHUNK_CUSTOMPARAMS,
        CHUNK_FORMEND,
        CHUNK_UPDATE,
        CHUNK_CONFIGVER,
        CHUNK_END,
        CHUNK_DONE
    };
    AsyncIotWebConf(
        const char* defaultThingName, DNSServer* dnsServer, AsyncWebServerWrapper* webServerWrapper,
        const char* initialApPassword, const char* configVersion = "init");
    void handleConfig(AsyncWebRequestWrapper* webRequestWrapper);
    virtual size_t getNextChunk(uint8_t* buffer, size_t maxLen);

    virtual void resetChunkState();

protected:
    ChunkStep _currentChunkStep = CHUNK_HEAD;
    String _chunkBuffer;
    size_t _chunkBufferPos = 0;
    bool _lastStepFinished = true;

    size_t _maxChunkSize = 0;
    size_t _totalBytesSent = 0;

    AsyncWebRequestWrapper* _webRequestWrapper = nullptr;

    friend class AsyncWebRequestWrapper;
    friend class IotWebConf;
    friend class AsyncIotWebConfTab;

};

#endif