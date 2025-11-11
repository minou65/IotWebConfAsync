#include "IotWebConfAsyncClass.h"

#ifndef CONTENT_LENGTH_UNKNOWN
#define CONTENT_LENGTH_UNKNOWN ((size_t)-1)
#endif

#if IOTWEBCONFASYNC_DEBUG_TO_SERIAL == 1
bool debugIotAsyncWebRequest = true;
#else
bool debugIotAsyncWebRequest = false;
#endif

AsyncWebRequestWrapper::AsyncWebRequestWrapper(AsyncWebServerRequest* request) :
    _request(request),
    _contentLength(0),
    _isChunked(false),
	_isFinished(false)
{

    _request->onDisconnect([this]() {
        delete this;
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

        for (const auto& h_ : _headers) response_->addHeader(h_.first, h_.second);
        response_->setCode(code);
        _request->send(response_);

    }
    else {
        auto* stream_ = _request->beginResponseStream(type_);
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

    if (_isChunked) {
        _chunkQueue.push(content);
    }
}

void AsyncWebRequestWrapper::setContentLength(const size_t contentLength) {
    _contentLength = contentLength;
    if (contentLength == CONTENT_LENGTH_UNKNOWN) {
        _isChunked = true;
        _response = new AsyncChunkedResponse(type_, [this](uint8_t* buffer, size_t maxLen, size_t) {
            return this->readChunk(buffer, maxLen);
            });
    }
}

void AsyncWebRequestWrapper::stop() {
    DEBUGASYNC_PRINTLN("AsyncWebRequestWrapper::stop");
	_isFinished = true;
}

size_t AsyncWebRequestWrapper::readChunk(uint8_t* buffer, size_t maxLen) {
    DEBUGASYNC_PRINTLN("AsyncWebRequestWrapper::readChunk");
    DEBUGASYNC_PRINT("    Max length requested: "); DEBUGASYNC_PRINTLN(maxLen);

    size_t totalLen = 0;
    // Combine as many chunks as possible until maxLen is reached or the queue is empty
    while (!_chunkQueue.empty() && totalLen < maxLen) {
        String& chunk = _chunkQueue.front();
        size_t copyLen = std::min(maxLen - totalLen, chunk.length());
        memcpy(buffer + totalLen, chunk.c_str(), copyLen);
        totalLen += copyLen;

        if (copyLen < chunk.length()) {
            chunk = chunk.substring(copyLen);
        }
        else {
            _chunkQueue.pop();
        }
    }

    // If the queue is empty and _isFinished is false, send a dummy byte
    if (_chunkQueue.empty() && totalLen == 0) {
        if (_isFinished) {
            DEBUGASYNC_PRINTLN("    All data has been transmitted. Transfer complete.");
            return 0;
        }
        else {
            // Send a dummy byte to keep the transfer alive
            if (maxLen > 0) {
                buffer[0] = ' ';
                totalLen = 1;
                DEBUGASYNC_PRINTLN("    Queue empty, sending dummy byte to keep connection alive.");
            }
        }
    }

    DEBUGASYNC_PRINTLN("    Returning chunk of length: " + String(totalLen));
    return totalLen;
}