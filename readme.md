# IotWebConfAsync

IotWebConfAsync is an extension for the [IotWebConf](https://github.com/minou65/IotWebConf) library.

With this library, you can use IotWebConf together with ESPAsyncWebServer. This makes web interfaces much faster and enables features like WebSerial and firmware updates.

## Table of Contents
- [Why Use IotWebConfAsync?](#why-use-iotwebconfasync)
- [Installation](#installation)
- [Dependencies](#dependencies)
- [Quick Start](#quick-start)
- [Features](#features)
- [Examples](#examples)
- [Basic Usage](#basic-usage)
- [Tab Support](#tab-support-asynciotwebconftab)
- [Firmware Update Support](#firmware-update-support-asyncupdateserver)
- [Debugging](#debugging)
- [Technical Details](#technical-details)
- [API Reference](#api-reference)
- [FAQ](#faq)
- [Contributing](#contributing)
- [License](#license)

## Why Use IotWebConfAsync?

- **Non-blocking**: Asynchronous web server allows concurrent connections
- **Better performance**: Faster response times and smoother user experience
- **Memory efficient**: Chunked responses handle large pages without memory issues
- **Feature-rich**: Built-in support for OTA updates, WebSerial, and tabbed configuration
- **Easy migration**: Drop-in replacement for standard IotWebConf
- **ESP32 & ESP8266**: Full support for both platforms

## Installation

### Arduino Library Manager
1. Open Arduino IDE
2. Go to Sketch → Include Library → Manage Libraries
3. Search for "IotWebConfAsync"
4. Click Install

### Manual Installation
1. Download the latest release from GitHub
2. Extract to your Arduino libraries folder
3. Restart Arduino IDE

### PlatformIO
Add to your `platformio.ini`:
```ini
lib_deps =
    IotWebConfAsync
    ESPAsyncWebServer
    IotWebConf
```

## Dependencies

When you install **IotWebConfAsync**, the following libraries will also be needed:

### Required Libraries (automatically installed as dependencies)
- [IotWebConf](https://github.com/minou65/IotWebConf) - The base configuration library (dependency of IotWebConfAsync)
- [ESPAsyncWebServer](https://github.com/ESP32Async/ESPAsyncWebServer) - Async web server for ESP32/ESP8266

### Platform-Specific Libraries
- **ESP32**: [AsyncTCP](https://github.com/ESP32Async/AsyncTCP)
- **ESP8266**: [ESPAsyncTCP](https://github.com/me-no-dev/ESPAsyncTCP)

### Optional Libraries
- [WebSerial](https://github.com/ayushsharma82/WebSerial) - For browser-based serial monitoring (see [IotWebConf02WebSerial](examples/IotWebConf02WebSerial))

**Note**: When using the Arduino Library Manager or PlatformIO, dependencies are usually installed automatically. If you're installing manually, make sure to also install the required libraries above.

## Quick Start

Here's a minimal example to get started:

```cpp
#include <IotWebConfAsync.h>
#include <IotWebConf.h>

DNSServer dnsServer;
AsyncWebServer server(80);
AsyncWebServerWrapper asyncWebServerWrapper(&server);

AsyncIotWebConf iotWebConf("MyDevice", &dnsServer, &asyncWebServerWrapper, 
                            "12345678", "v1.0");

void setup() {
    Serial.begin(115200);
    
    // Initialize IotWebConf
    iotWebConf.init();
    
    // Setup web routes
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(200, "text/html", "<h1>Hello from IotWebConfAsync!</h1>");
    });
    
    server.on("/config", HTTP_ANY, [](AsyncWebServerRequest* request) {
        auto* wrapper = new AsyncWebRequestWrapper(request);
        iotWebConf.handleConfig(wrapper);
    });
    
    server.onNotFound([](AsyncWebServerRequest* request) { 
        AsyncWebRequestWrapper wrapper(request);
        iotWebConf.handleNotFound(&wrapper);
    });
    
    Serial.println("Ready!");
}

void loop() {
    iotWebConf.doLoop();
}
```

Connect to the device's access point (default password: 12345678) and navigate to `192.168.4.1/config` to configure WiFi settings.

For complete examples, see the [examples](examples) folder.

## Features

This library provides three main components:

### 1. AsyncIotWebConf
The core class that enables IotWebConf to work with ESPAsyncWebServer. Features include:
- **AsyncWebRequestWrapper**: Wraps AsyncWebServerRequest for compatibility with IotWebConf
- **AsyncWebServerWrapper**: Provides the web server interface
- **Chunked responses**: Efficiently streams configuration pages to avoid memory issues
- **Non-blocking operation**: Fully asynchronous handling of web requests

### 2. AsyncIotWebConfTab
An extended version of AsyncIotWebConf that adds tabbed configuration pages:
- Organize parameters into multiple tabs for better user experience
- Customizable tab names and positioning
- System tab can be placed at start, end, or any position
- Responsive tab layout with CSS styling
- Configurable container width for consistent display

### 3. AsyncUpdateServer
OTA (Over-The-Air) firmware update support for ESP32/ESP8266:
- Secure firmware updates with optional username/password
- Progress tracking
- Error handling and reporting
- Compatible with Arduino OTA update mechanism

## Examples

See the `examples` folder for complete, ready-to-use sketches:

### [IotWebConf01Minimal](examples/IotWebConf01Minimal)
Basic example showing minimal configuration setup:
- WiFi configuration
- Access Point mode
- Basic web interface
- Status LED indication
- Perfect starting point for new projects

### [IotWebConf02WebSerial](examples/IotWebConf02WebSerial)
Integration with WebSerial for browser-based serial monitoring:
- All features from minimal example
- WebSerial web interface at `/webserial`
- Remote serial debugging without USB connection
- Real-time logging to browser console

### [IotWebConf03Firmware](examples/IotWebConf03Firmware)
OTA firmware update functionality:
- Secure firmware updates via web interface
- Update progress indication
- Automatic restart after successful update
- Password-protected update page
- Error handling and recovery

### [IotWebConf04Tab](examples/IotWebConf04Tab)
Advanced example with tabbed configuration interface:
- Multiple configuration tabs (System, Network, MQTT, Sensor)
- Custom tab organization and positioning
- Form validation
- Configuration callbacks
- Best practices for complex configurations
- See [IotWebConf04Tab/README.md](examples/IotWebConf04Tab/README.md) for detailed documentation

## Basic Usage

### Include Required Headers

```cpp
#include <IotWebConfAsync.h>  // or IotWebConfAsyncTab.h for tab support
#include <IotWebConf.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
```

### Initialize Components

Initialize the `AsyncWebServerWrapper` with the `AsyncWebServer` object and create the `AsyncIotWebConf` instance:

```cpp
DNSServer dnsServer;
AsyncWebServer server(80);
AsyncWebServerWrapper asyncWebServerWrapper(&server);

AsyncIotWebConf iotWebConf(thingName, &dnsServer, &asyncWebServerWrapper, 
                            wifiInitialApPassword, CONFIG_VERSION);
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

### WebSerial Integration

To use WebSerial for browser-based serial monitoring, include the library and initialize it:

```cpp
#include <WebSerial.h>

void setup() {
    // ... IotWebConf initialization ...
    
    // Initialize WebSerial
    WebSerial.begin(&server);
    
    iotWebConf.init();
}

void loop() {
    iotWebConf.doLoop();
    
    // Use WebSerial for logging
    WebSerial.println("Status update");
}
```

WebSerial provides:
- Real-time serial output in browser at `/webserial`
- No USB cable needed for debugging
- Multiple simultaneous connections
- Full bidirectional communication

See [examples/IotWebConf02WebSerial](examples/IotWebConf02WebSerial) for a complete example.

## Tab Support (AsyncIotWebConfTab)

The `AsyncIotWebConfTab` class extends `AsyncIotWebConf` with support for organizing configuration parameters into multiple tabs for better user experience.

### Basic Tab Usage

```cpp
#include "IotWebConfAsyncTab.h"

DNSServer dnsServer;
AsyncWebServer server(80);
AsyncWebServerWrapper asyncWebServerWrapper(&server);

// Use AsyncIotWebConfTab instead of AsyncIotWebConf
AsyncIotWebConfTab iotWebConf(thingName, &dnsServer, &asyncWebServerWrapper, 
                               wifiInitialApPassword, CONFIG_VERSION);

// Create parameter groups
iotwebconf::ParameterGroup networkGroup = iotwebconf::ParameterGroup("network", "Network Settings");
iotwebconf::ParameterGroup mqttGroup = iotwebconf::ParameterGroup("mqtt", "MQTT Settings");

// Add your parameters to the groups
iotwebconf::TextParameter hostParam = iotwebconf::TextParameter("Host", "host", hostValue, 64);
networkGroup.addItem(&hostParam);

// Add groups to specific tabs
iotWebConf.addParameterGroup(&networkGroup, "Network");
iotWebConf.addParameterGroup(&mqttGroup, "MQTT");
```

### Advanced Tab Features

#### Custom System Tab Name
```cpp
// Change the default "System" tab name
iotWebConf.setSystemTabName("Device");
```

#### System Tab Positioning
```cpp
// Position the system tab:
iotWebConf.setSystemTabPosition(0);   // First position (default)
iotWebConf.setSystemTabPosition(1);   // Second position
iotWebConf.setSystemTabPosition(-1);  // Last position
```

#### Container Width Configuration
```cpp
// Set custom width constraints for the configuration page
AsyncTabHtmlFormatProvider* formatProvider = 
    static_cast<AsyncTabHtmlFormatProvider*>(iotWebConf.getHtmlFormatProvider());
formatProvider->setContainerWidth(400, 700);  // min: 400px, max: 700px
```

### Tab Organization Best Practices

1. **Group related parameters**: Put similar settings in the same tab
2. **Limit tabs**: Keep the number of tabs reasonable (3-6 tabs recommended)
3. **Clear naming**: Use descriptive tab names
4. **System tab placement**: Position based on importance (first for most users, last for advanced settings)

For a complete working example, see [examples/IotWebConf04Tab](examples/IotWebConf04Tab).

## Firmware Update Support (AsyncUpdateServer)

The `AsyncUpdateServer` class provides secure OTA firmware updates.

### Basic Update Server Setup

```cpp
#include "IotWebConfAsyncUpdateServer.h"

AsyncUpdateServer asyncUpdater;

void setup() {
    // Setup update server with IotWebConf
    iotWebConf.setupUpdateServer(
        [](const char* updatePath) { 
            asyncUpdater.setup(&server, updatePath); 
        },
        [](const char* userName, char* password) { 
            asyncUpdater.updateCredentials(userName, password); 
        }
    );
    
    iotWebConf.init();
}

void loop() {
    iotWebConf.doLoop();
    
    // Check if update finished and restart
    if (asyncUpdater.isFinished()) {
        Serial.println("Firmware update finished.");
        ESP.restart();
    }
}
```

### Update Server Features

- **Password protection**: Uses IotWebConf credentials
- **Progress tracking**: Monitor update progress
- **Error handling**: Check errors with `asyncUpdater.getUpdaterError()`
- **Status checking**: Use `asyncUpdater.isUpdating()` to check update status
- **Automatic recovery**: Built-in error handling and recovery mechanisms

For a complete example, see [examples/IotWebConf03Firmware](examples/IotWebConf03Firmware).

## Debugging

To enable debug output, add the following define before including the library:

```cpp
#define IOTWEBCONFASYNC_DEBUG_TO_SERIAL 1
#include <IotWebConfAsync.h>
```

## Technical Details

### Memory Management

**Important**: The `AsyncWebRequestWrapper` has special memory management requirements:

- Always allocate `AsyncWebRequestWrapper` with `new` when handling config requests
- Do **not** delete the pointer manually
- The object automatically deletes itself when no longer needed
- This ensures the object lives long enough for asynchronous operations

Example:
```cpp
server.on("/config", HTTP_ANY, [](AsyncWebServerRequest* request) {
    // CORRECT: Allocate with 'new', do NOT delete
    auto* asyncWebRequestWrapper = new AsyncWebRequestWrapper(request);
    iotWebConf.handleConfig(asyncWebRequestWrapper);
});
```

### Chunked Response Streaming

The library uses chunked responses to handle large configuration pages efficiently:
- Reduces memory footprint
- Prevents ESP32/ESP8266 from running out of RAM
- Configurable chunk size (default: 32KB internal buffer)
- Automatic buffer management

### Supported Platforms

- **ESP32**: Fully supported with AsyncTCP
- **ESP8266**: Supported with ESPAsyncTCP

Both platforms support:
- WiFi configuration
- Access Point mode
- OTA updates
- WebSerial integration
- Tab-based configuration

## API Reference

### AsyncIotWebConf Class

Main class for async web configuration.

**Constructor:**
```cpp
AsyncIotWebConf(const char* defaultThingName, 
                DNSServer* dnsServer, 
                AsyncWebServerWrapper* webServerWrapper,
                const char* initialApPassword, 
                const char* configVersion = "init");
```

**Key Methods:**
- `void handleConfig(AsyncWebRequestWrapper* webRequestWrapper)` - Handle configuration page requests
- `void init()` - Initialize the configuration system
- `void doLoop()` - Must be called in main loop
- `size_t getNextChunk(uint8_t* buffer, size_t maxLen)` - Get next chunk of response data
- `void resetChunkState()` - Reset chunked response state

### AsyncIotWebConfTab Class

Extended class with tab support (inherits from AsyncIotWebConf).

**Additional Methods:**
- `void addParameterGroup(iotwebconf::ParameterGroup* group, const char* tabName)` - Add parameter group to specific tab
- `void setSystemTabName(const char* tabName)` - Set custom name for system tab
- `void setSystemTabPosition(int position)` - Set system tab position (0=first, -1=last)
- `std::vector<AsyncTabInfo>* getTabsVector()` - Get vector of all tabs

### AsyncUpdateServer Class

Handles OTA firmware updates.

**Constructor:**
```cpp
AsyncUpdateServer(bool serial_debug = false);
```

**Methods:**
- `void setup(AsyncWebServer* server, const String& path)` - Setup update server
- `void updateCredentials(const String& username, const String& password)` - Update auth credentials
- `bool isUpdating()` - Check if update is in progress
- `bool isFinished()` - Check if update is finished
- `String getUpdaterError()` - Get last error message

## FAQ

### Q: Why do I need to use `new` for AsyncWebRequestWrapper?
**A:** The async web server processes requests asynchronously. Using `new` ensures the wrapper object lives long enough for the async operations to complete. The object deletes itself automatically when done.

### Q: Can I use this with the original IotWebConf examples?
**A:** Yes, with minimal changes. Replace `HTTPWebServer` with `AsyncWebServer`, wrap it in `AsyncWebServerWrapper`, and change `AsyncIotWebConf` for async support. See [examples/IotWebConf01Minimal](examples/IotWebConf01Minimal) for reference.

### Q: How much memory does the library use?
**A:** The library uses chunked responses to minimize memory usage. The default internal buffer is 32KB, but only temporary during page generation. Actual runtime overhead is minimal.

### Q: Does this work with ESP32-S2/S3/C3?
**A:** Yes, all ESP32 variants are supported as long as AsyncTCP and ESPAsyncWebServer support them.

### Q: Can I customize the tab styling?
**A:** Yes, you can extend `AsyncTabHtmlFormatProvider` and override the `getStyleInner()` method to customize CSS styles.

### Q: Why is my configuration page not loading?
**A:** Check that:
1. You're using `AsyncWebRequestWrapper` correctly with `new`
2. All routes are properly registered before calling `iotWebConf.init()`
3. The `/config` route handler is set up correctly
4. Debug output is enabled to see potential errors

### Q: How do I migrate from standard IotWebConf?
**A:** 
1. Replace `#include <IotWebConf.h>` with `#include <IotWebConfAsync.h>`
2. Change web server from `HTTPWebServer` to `AsyncWebServer`
3. Wrap with `AsyncWebServerWrapper`
4. Update route handlers to use `AsyncWebServerRequest*`
5. Use `new AsyncWebRequestWrapper(request)` for config handler

For detailed examples, see the [examples](examples) folder.

### Q: Can I use multiple tabs with different parameter groups?
**A:** Yes! Use `AsyncIotWebConfTab` and call `addParameterGroup(group, "TabName")` for each group. See [examples/IotWebConf04Tab](examples/IotWebConf04Tab) for a complete example.

## Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues on GitHub.

## Credits

- Based on [IotWebConf](https://github.com/minou65/IotWebConf)
- Uses [ESPAsyncWebServer](https://github.com/ESP32Async/ESPAsyncWebServer)
- Update server code adapted from ESP8266HTTPUpdateServer

## License

This library is licensed under the GNU General Public License v3.0 (see LICENSE file).

Copyright (c) 2024 Andreas Zogg