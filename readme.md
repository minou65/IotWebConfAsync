

You also need to install the following libraries
AsyncTCP
ESPAsyncWebServer
https://github.com/me-no-dev/ESPAsyncWebServer



void WebServer::sendContent(const char *content, size_t contentLength) {
  const char *footer = "\r\n";
  if (_chunked) {
    char *chunkSize = (char *)malloc(11);
    if (chunkSize) {
      sprintf(chunkSize, "%x%s", contentLength, footer);
      _currentClientWrite(chunkSize, strlen(chunkSize));
      free(chunkSize);
    }
  }
  _currentClientWrite(content, contentLength);
  if (_chunked) {
    _currentClient.write(footer, 2);
    if (contentLength == 0) {
      _chunked = false;
    }
  }
}