#include "IotWebConfAsyncClass.h"

#ifndef CONTENT_LENGTH_UNKNOWN
#define CONTENT_LENGTH_UNKNOWN ((size_t)-1)
#endif

#if IOTWEBCONFASYNC_DEBUG_TO_SERIAL == 0
bool debugIotAsyncWebRequest = true;
#else
bool debugIotAsyncWebRequest = false;
#endif

std::vector<std::unique_ptr<AsyncWebRequestWrapper>> wrapperList;

AsyncWebRequestWrapper::AsyncWebRequestWrapper(AsyncWebServerRequest* request) :
    _request(request),
    _response(nullptr),
    _contentLength(0),
    _isChunked(false),
    _responseSent(false),
    _finished(false)
{
    sendHeader("Server", "ESP Async Web Server");
    sendHeader(asyncsrv::T_Cache_Control, "public,max-age=60");
    wrapperList.push_back(std::unique_ptr<AsyncWebRequestWrapper>(this));
}

void AsyncWebRequestWrapper::send(int code, const char* content_type, const String& content) {
    DEBUGASYNC_PRINTLN("AsyncWebRequestWrapper::send");
    DEBUGASYNC_PRINT("    Code: "); DEBUGASYNC_PRINTLN(code);
    DEBUGASYNC_PRINT("    Content type: "); DEBUGASYNC_PRINTLN(content_type);
    DEBUGASYNC_PRINT("    Content: "); DEBUGASYNC_PRINTLN(content);
    DEBUGASYNC_PRINT("    Content length: "); DEBUGASYNC_PRINTLN(content.length());

    if (_responseSent) return;
    const char* type = content_type ? content_type : "text/html";

    _request->onDisconnect([this]() {
        auto it = std::find_if(wrapperList.begin(), wrapperList.end(),
            [this](const std::unique_ptr<AsyncWebRequestWrapper>& ptr) { return ptr.get() == this; });
        if (it != wrapperList.end()) {
            wrapperList.erase(it);
        }
        });

    if (_isChunked) {
        if (_response == nullptr) {
            _response = new AsyncChunkedResponse(type, [this](uint8_t* buffer, size_t maxLen, size_t) {
                return this->readChunk(buffer, maxLen);
                });
            for (const auto& h : _headers) _response->addHeader(h.first, h.second);
            _response->setCode(code);
            _request->send(_response);
            _responseSent = true;
        }
    }
    else {
        auto* stream = _request->beginResponseStream(type);
        stream->setCode(code);
        stream->setContentLength(_contentLength);
        stream->print(content);
        for (const auto& h : _headers) stream->addHeader(h.first, h.second);
        _request->send(stream);
        _responseSent = true;
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
    }
}

void AsyncWebRequestWrapper::stop() {
    DEBUGASYNC_PRINTLN("AsyncWebRequestWrapper::stop");
    _finished = true;
    if (!_isChunked) {
        if (_deleteReadyTimestamp == 0) {
            _deleteReadyTimestamp = millis();
            DEBUGASYNC_PRINTLN("    Marked for deletion, waiting for safe cleanup");
        }
    }
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

    DEBUGASYNC_PRINTLN("    Returning chunk of length: " + String(totalLen));

    // Only set readyToDelete if finished AND no more data AND this was the last call (totalLen == 0)
    if (_chunkQueue.empty() && _finished && totalLen == 0) {
        DEBUGASYNC_PRINTLN("    Marked for deletion, waiting for safe cleanup");
    }

    return totalLen;
}