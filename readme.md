# IotWebConfAsync

IotWebConfAsync is an extension for the great [IotWebConf](https://github.com/prampec/IotWebConf) library by [Balazs Kelemen](prampec+arduino@gmail.com).

With this library, you can use IotWebConf together with ESPAsyncWebServer. This makes web interfaces much faster and enables features like WebSerial and firmware updates.

## License

This library is licensed under the GNU General Public License v3.0 (see LICENSE file).

Copyright (c) 2024 Andreas Zogg

## Dependencies

You also need to install the following libraries:
- [AsyncTCP](https://github.com/ESP32Async/AsyncTCP) (for ESP32)
- [ESPAsyncWebServer](https://github.com/ESP32Async/ESPAsyncWebServer)
- [IotWebConf](https://github.com/prampec/IotWebConf)
- [WebSerial](https://github.com/ayushsharma82/WebSerial) (optional, for WebSerial example)

## Examples

See the `examples` folder for ready-to-use sketches:
- **IotWebConf01Minimal**: Minimal example for basic configuration
- **IotWebConf02WebSerial**: Example with WebSerial integration
- **IotWebConf03Firmware**: Example with firmware update support

## Basic Usage

Initialize the `AsyncWebServerWrapper` with the `AsyncWebServer` object and create the `IotWebConf` instance:

```cpp
#include <IotWebConfAsyncClass.h>
#include <IotWebConf.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>

DNSServer dnsServer;
AsyncWebServer server(80);
AsyncWebServerWrapper asyncWebServerWrapper(&server);

IotWebConf iotWebConf(thingName, &dnsServer, &asyncWebServerWrapper, wifiInitialApPassword, CONFIG_VERSION);
```

### Setup function

```cpp
void setup() {
  Serial.begin(115200);
  iotWebConf.setStatusPin(STATUS_PIN, ON_LEVEL);
  iotWebConf.setConfigPin(CONFIG_PIN);
  iotWebConf.getApTimeoutParameter()->visible = true;

  iotWebConf.init();
  server.on("/", HTTP_GET, handleRoot);
  server.on("/config", HTTP_ANY, [](AsyncWebServerRequest* request) {
    // IMPORTANT: You must create the AsyncWebRequestWrapper with 'new' and do NOT delete it manually.
    // The object will delete itself when it is no longer needed, to ensure it lives long enough.
    auto* asyncWebRequestWrapper = new AsyncWebRequestWrapper(request);
    iotWebConf.handleConfig(asyncWebRequestWrapper);
  });
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest* request) {
    AsyncWebServerResponse* response = request->beginResponse_P(200, "image/x-icon", favicon_ico_gz, favicon_ico_gz_len);
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });
  server.onNotFound([](AsyncWebServerRequest* request) {
    AsyncWebRequestWrapper asyncWebRequestWrapper(request);
    iotWebConf.handleNotFound(&asyncWebRequestWrapper);
  });
}
```

### Loop function

```cpp
void loop() {
  iotWebConf.doLoop();
}
```

### WebSerial Example

To use WebSerial, include the library and call `WebSerial.begin(&server);` in `setup()`. See `examples/IotWebConf02WebSerial/IotWebConf02WebSerial.ino` for details.

### Firmware Update Example

To enable firmware updates, use the `IotWebConfAsyncUpdateServer` and see `examples/IotWebConf03Firmware/IotWebConf03Firmware.ino` for a full example.

## Debugging

To enable debug output, add the following define before including the library:

```cpp
#define IOTWEBCONFASYNC_DEBUG_TO_SERIAL 1
```

**Note:**
- Always allocate `AsyncWebRequestWrapper` with `new` as shown above.
- Do **not** delete the pointer yourself. The object will delete itself when it is no longer needed, ensuring it lives long enough for asynchronous operations.