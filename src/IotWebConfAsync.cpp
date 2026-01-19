#include "IotWebConfAsync.h"

#ifndef CONTENT_LENGTH_UNKNOWN
#define CONTENT_LENGTH_UNKNOWN ((size_t)-1)
#endif

#define IOTWEBCONFASYNC_DEBUG_TO_SERIAL 1

#if IOTWEBCONFASYNC_DEBUG_TO_SERIAL == 1
bool debugIotAsyncWebRequest = true;
#else
bool debugIotAsyncWebRequest = false;
#endif

#define DEBUGASYNC_PRINT(x) if (debugIotAsyncWebRequest) Serial.print(x) 
#define DEBUGASYNC_PRINTLN(x) if (debugIotAsyncWebRequest) Serial.println(x)
#define DEBUGASYNC_PRINTF(...) if (debugIotAsyncWebRequest) Serial.printf(__VA_ARGS__)

AsyncWebRequestWrapper::AsyncWebRequestWrapper(AsyncWebServerRequest* request) :
    _request(request),
    _contentLength(0),
    _isChunked(false),
	_isFinished(false)
{

    _request->onDisconnect([this]() {
    });

    sendHeader("Server", "ESP Async Web Server");
    sendHeader(asyncsrv::T_Cache_Control, "public,max-age=60");
}

void AsyncWebRequestWrapper::send(int code, const char* content_type, const String& content) {
    DEBUGASYNC_PRINTLN("AsyncWebRequestWrapper::send");
    DEBUGASYNC_PRINT("    Code: "); DEBUGASYNC_PRINTLN(code);
    DEBUGASYNC_PRINT("    Content type: "); DEBUGASYNC_PRINTLN(content_type);
    DEBUGASYNC_PRINT("    Content: "); DEBUGASYNC_PRINTLN(content);
    DEBUGASYNC_PRINT("    Content length: "); DEBUGASYNC_PRINTLN(content.length());

    if (_isChunked) {
        if (_configuration == nullptr) {
            auto* stream_ = _request->beginResponseStream(content_type);
            stream_->setCode(500);
            stream_->print("Internal Server Error: No configuration for chunked response.");
            for (const auto& h_ : _headers) stream_->addHeader(h_.first, h_.second);
            _request->send(stream_);
            return;
        }
        _contentType = content_type;
        _response = new AsyncChunkedResponse(_contentType, [this](uint8_t* buffer, size_t maxLen, size_t) {
            return this->readChunk(buffer, maxLen);
            });
        for (const auto& h_ : _headers) _response->addHeader(h_.first, h_.second);
        _response->setCode(code);
        _request->send(_response);
    }
    else {
        auto* stream_ = _request->beginResponseStream(content_type);
        stream_->setCode(code);
        stream_->setContentLength(_contentLength);
        stream_->print(content);
        for (const auto& h_ : _headers) stream_->addHeader(h_.first, h_.second);
        _request->send(stream_);
    }
}

void AsyncWebRequestWrapper::sendHeader(const String& name, const String& value, bool first) {
    _headers.emplace_back(name, value);
}

void AsyncWebRequestWrapper::sendContent(const String& content) {
    DEBUGASYNC_PRINTLN("AsyncWebRequestWrapper::sendContent");
    DEBUGASYNC_PRINT("    Content length: "); DEBUGASYNC_PRINTLN(content.length());
}

void AsyncWebRequestWrapper::setContentLength(const size_t contentLength) {
    DEBUGASYNC_PRINTLN("AsyncWebRequestWrapper::setContentLength");
    DEBUGASYNC_PRINT("    Content length: "); DEBUGASYNC_PRINTLN(contentLength);
    _contentLength = contentLength;
    if (contentLength == CONTENT_LENGTH_UNKNOWN) {
		DEBUGASYNC_PRINTLN("    Using chunked transfer encoding");
        _isChunked = true;
    }
}

void AsyncWebRequestWrapper::stop() {
    DEBUGASYNC_PRINTLN("AsyncWebRequestWrapper::stop");
	_isFinished = true;

}

void AsyncWebRequestWrapper::setConfiguration(AsyncIotWebConf* configuration) {
    this->_configuration = configuration;
    if (_configuration) {
        _configuration->resetChunkState();
    }
}

size_t AsyncWebRequestWrapper::readChunk(uint8_t* buffer, size_t maxLen) {
    DEBUGASYNC_PRINTLN("AsyncWebRequestWrapper::readChunk");
    if (_configuration) {
        size_t chunkSize = _configuration->getNextChunk(buffer, maxLen);
        return chunkSize;
    }
    DEBUGASYNC_PRINTLN("    No configuration available, returning 0.");
    return 0;
}

AsyncIotWebConf::AsyncIotWebConf(const char* defaultThingName, DNSServer* dnsServer, 
    AsyncWebServerWrapper* webServerWrapper, const char* initialApPassword, const char* configVersion) :
    IotWebConf(defaultThingName, dnsServer, webServerWrapper, initialApPassword, configVersion) {

	resetChunkState();
}

void AsyncIotWebConf::handleConfig(AsyncWebRequestWrapper* webRequestWrapper) {
    if (this->getState() == iotwebconf::OnLine) {
        // -- Authenticate
        if (!webRequestWrapper->authenticate(
            IOTWEBCONF_ADMIN_USER_NAME, this->getApPassword())) {
            IOTWEBCONF_DEBUG_LINE(F("Requesting authentication."));
            webRequestWrapper->requestAuthentication();
            return;
        }
    }
	_webRequestWrapper = webRequestWrapper;
    bool dataArrived = webRequestWrapper->hasArg("iotSave");
    if (!dataArrived || !this->validateForm(webRequestWrapper)) {
        // -- Display config portal
        IOTWEBCONF_DEBUG_LINE(F("Configuration page requested."));

        webRequestWrapper->setConfiguration(this);
        webRequestWrapper->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        webRequestWrapper->sendHeader("Pragma", "no-cache");
        webRequestWrapper->sendHeader("Expires", "-1");
        webRequestWrapper->setContentLength(CONTENT_LENGTH_UNKNOWN);
        webRequestWrapper->send(200, "text/html; charset=UTF-8", "");
        webRequestWrapper->stop();
    }
    else {
        IotWebConf::handleConfig(webRequestWrapper);
		DEBUGASYNC_PRINTLN("Configuration saved, sending saved page.");
    }

}

size_t AsyncIotWebConf::getNextChunk(uint8_t* buffer, size_t maxLen) {
    DEBUGASYNC_PRINTLN("AsyncIotWebConf::getNextChunk");
    DEBUGASYNC_PRINT("  Current chunk step: "); DEBUGASYNC_PRINTLN(_currentChunkStep);
    bool dataArrived_ = false;
    size_t written_ = 0;
    bool currentStepFinished_ = true;  // Track if current step is complete

    while (_currentChunkStep != CHUNK_DONE && written_ < maxLen) {
        yield();

        // Generate new chunk data if buffer is empty or exhausted
        if (_chunkBufferPos >= _chunkBuffer.length()) {
            _chunkBuffer = "";
            _chunkBufferPos = 0;

            HtmlChunkCallback writer_ = [&](const char* data, size_t len) -> bool {
                yield();
                _chunkBuffer += String(data).substring(0, len);
                return true;
            };

            switch (_currentChunkStep) {
            case CHUNK_HEAD:
                _chunkBuffer = this->getHtmlFormatProvider()->getHead();
                _chunkBuffer.replace("{v}", String("Config ") + this->getThingName());
                currentStepFinished_ = true;
                break;
            case CHUNK_SCRIPT:
                _chunkBuffer = this->getHtmlFormatProvider()->getScript();
                currentStepFinished_ = true;
                break;
            case CHUNK_STYLE:
                _chunkBuffer = this->getHtmlFormatProvider()->getStyle();
                currentStepFinished_ = true;
                break;
            case CHUNK_HEADEXT:
                _chunkBuffer = this->getHtmlFormatProvider()->getHeadExtension();
                currentStepFinished_ = true;
                break;
            case CHUNK_HEADEND:
                _chunkBuffer = this->getHtmlFormatProvider()->getHeadEnd();
                currentStepFinished_ = true;
                break;
            case CHUNK_FORMSTART:
                _chunkBuffer = this->getHtmlFormatProvider()->getFormStart();
                currentStepFinished_ = true;
                break;
            case CHUNK_SYSTEMPARAMS:
                currentStepFinished_ = this->getSystemParameterGroup()->renderHtml(dataArrived_, _webRequestWrapper, writer_);
                DEBUGASYNC_PRINT("  CHUNK_SYSTEMPARAMS finish: "); DEBUGASYNC_PRINTLN(currentStepFinished_);
                break;
            case CHUNK_CUSTOMPARAMS:
                currentStepFinished_ = this->getCustomParameterGroup()->renderHtml(dataArrived_, _webRequestWrapper, writer_);
                DEBUGASYNC_PRINT("  CHUNK_CUSTOMPARAMS finish: "); DEBUGASYNC_PRINTLN(currentStepFinished_);
                break;
            case CHUNK_FORMEND:
                _chunkBuffer = this->getHtmlFormatProvider()->getFormEnd();
                currentStepFinished_ = true;
                break;
            case CHUNK_UPDATE:
                _chunkBuffer = getUpdateLinkHtml();
                currentStepFinished_ = true;
                break;
            case CHUNK_CONFIGVER:
                _chunkBuffer = getConfigVersionHtml();
                currentStepFinished_ = true;
                break;
            case CHUNK_END:
                _chunkBuffer = this->getHtmlFormatProvider()->getEnd();
                currentStepFinished_ = true;
                break;
            default:
                _chunkBuffer = "";
                currentStepFinished_ = true;
                break;
            }
            
            _chunkBufferPos = 0;
            _maxChunkSize = max(_maxChunkSize, _chunkBuffer.length());

            DEBUGASYNC_PRINTF("  Generated chunk data, length: %u bytes, stepFinished: %d\n", 
                (unsigned int)_chunkBuffer.length(), currentStepFinished_);

            // If buffer is empty and step is not finished, wait for next call
            if (_chunkBuffer.length() == 0 && !currentStepFinished_) {
                DEBUGASYNC_PRINTLN("  Step incomplete but no data generated, will retry next call");
                break;
            }
            
            // If buffer is empty and step is finished, move to next step
            if (_chunkBuffer.length() == 0 && currentStepFinished_) {
                DEBUGASYNC_PRINTLN("  Empty chunk, moving to next step");
                _currentChunkStep = static_cast<ChunkStep>(_currentChunkStep + 1);
                continue;
            }
        }

		DEBUGASYNC_PRINTF("  Requested max chunk length: %u bytes\n", (unsigned int)maxLen);
		DEBUGASYNC_PRINTF("  Chunk buffer length: %u bytes\n", (unsigned int)_chunkBuffer.length());
		DEBUGASYNC_PRINTF("  Chunk buffer pos: %u bytes\n", (unsigned int)_chunkBufferPos);

        // Copy as much as possible from _chunkBuffer to output buffer
        size_t toCopy_ = std::min(maxLen - written_, _chunkBuffer.length() - _chunkBufferPos);
        memcpy(buffer + written_, _chunkBuffer.c_str() + _chunkBufferPos, toCopy_);
        _chunkBufferPos += toCopy_;
        written_ += toCopy_;
        _totalBytesSent += toCopy_;

		DEBUGASYNC_PRINTF("  Copied %u bytes, total written: %u bytes\n", (unsigned int)toCopy_, (unsigned int)written_);

        // Check if we've sent all data from current chunk buffer
        if (_chunkBufferPos >= _chunkBuffer.length()) {
            DEBUGASYNC_PRINTLN("  Current chunk buffer completely sent");
            // Move to next step ONLY if this step is complete
            if (currentStepFinished_) {
                DEBUGASYNC_PRINTLN("  Step finished, moving to next step");
                _currentChunkStep = static_cast<ChunkStep>(_currentChunkStep + 1);
            } else {
                DEBUGASYNC_PRINTLN("  Step not finished yet, staying on same step");
                // Buffer is empty but step not finished - will call renderHtml again next time
            }
            // Exit loop to return current data
            break;
        } else {
            // Still more data in buffer, will continue next call
            DEBUGASYNC_PRINTLN("  More data in buffer, will continue next call");
            break;
        }
    }

    if (_currentChunkStep == CHUNK_DONE) {
        DEBUGASYNC_PRINTLN("All chunks sent, resetting chunk state.");
		DEBUGASYNC_PRINTF("  Max chunk size sent: %u bytes\n", (unsigned int)_maxChunkSize);
        DEBUGASYNC_PRINTF("  Total bytes sent: %u bytes\n", (unsigned int)_totalBytesSent);
		resetChunkState();
		return 0;
	}

    return written_;
}

void AsyncIotWebConf::resetChunkState() {
    _currentChunkStep = CHUNK_HEAD;
    _chunkBuffer = "";
    _chunkBufferPos = 0;
}