#include "IotWebConfAsyncClass.h"

#ifndef CONTENT_LENGTH_UNKNOWN
#define CONTENT_LENGTH_UNKNOWN ((size_t)-1)
#endif

std::vector<AsyncWebRequestWrapper*> wrapperList;

AsyncWebRequestWrapper::AsyncWebRequestWrapper(AsyncWebServerRequest* request) :
    _request(request),
    _reponse(nullptr),
    _contentLength(0),
    _isChunked(false),
    _responseSent(false),
    _finished(false)
{
    sendHeader("Server", "ESP Async Web Server");
    sendHeader(asyncsrv::T_Cache_Control, "public,max-age=60");
    wrapperList.push_back(this);
}

void AsyncWebRequestWrapper::send(int code, const char* content_type, const String& content) {
    DEBUGASYNC_PRINTLN("AsyncWebRequestWrapper::send");
    DEBUGASYNC_PRINT("    Code: "); DEBUGASYNC_PRINTLN(code);
    DEBUGASYNC_PRINT("    Content type: "); DEBUGASYNC_PRINTLN(content_type);
    DEBUGASYNC_PRINT("    Content: "); DEBUGASYNC_PRINTLN(content);
    DEBUGASYNC_PRINT("    Content length: "); DEBUGASYNC_PRINTLN(content.length());

    if (_responseSent) return;
    const char* type = content_type ? content_type : "text/html";

    if (_isChunked) {
        if (_reponse == nullptr) {
            _reponse = new AsyncChunkedResponse(type, [this](uint8_t* buffer, size_t maxLen, size_t) {
                return this->readChunk(buffer, maxLen);
                });
            for (const auto& h : _headers) _reponse->addHeader(h.first, h.second);
            _reponse->setCode(code);
            _request->send(_reponse);
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
        _readyToDelete = true;
    }
}

bool AsyncWebRequestWrapper::readyToDelete() const {
    return _readyToDelete;
}

size_t AsyncWebRequestWrapper::readChunk(uint8_t* buffer, size_t maxLen) {
    DEBUGASYNC_PRINTLN("AsyncWebRequestWrapper::readChunk");
    DEBUGASYNC_PRINT("    Max length requested: "); DEBUGASYNC_PRINTLN(maxLen);

    size_t totalLen = 0;
    // Füge so viele Chunks wie möglich zusammen, bis maxLen erreicht ist oder die Queue leer ist
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

    // Wenn keine Daten mehr und Übertragung beendet, Objekt löschen
    if (_chunkQueue.empty() && _finished) {
        _readyToDelete = true;
        DEBUGASYNC_PRINTLN("    No more chunks, finishing response");
    }

    return totalLen;
}

void AsyncWebServerWrapper::cleanupWrappers() {
    for (auto it = wrapperList.begin(); it != wrapperList.end(); ) {
        if ((*it)->readyToDelete()) {
            delete* it;
            it = wrapperList.erase(it);
        }
        else {
            ++it;
        }
    }
}