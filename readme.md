# IotWebConfAsync

This is a extenstion for the great libray . 

This library is an extension for the [IotWebConf](https://github.com/prampec/IotWebConf) library by [Balazs Kelemen](prampec+arduino@gmail.com). With this library, it is possible to use the ESPAsyncWebServer. As a result, websites become significantly faster, and WebSerial can be used.

## License
This library is licensed under the MIT License.

Copyright (c) 2024 Andreas Zogg

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.


## Librarys
You also need to install the following libraries
AsyncTCP
[ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer)

## Usage
See also the examples in the examples folder.

�nit the AsyncWebServerWrapper with the AsyncWebServer object.
```cpp
#include <IotWebConfAsync.h>

DNSServer dnsServer;
AsyncWebServer server(80);
AsyncWebServerWrapper asyncWebServerWrapper(&server);

IotWebConf iotWebConf(thingName, &dnsServer, &asyncWebServerWrapper, wifiInitialApPassword, CONFIG_VERSION);
```

Initialize the IotWebConf object and set up the required URL handlers on the web server.
```cpp
void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.println("Booting");

	// -- Initializing the configuration.
	iotWebConf.init();
	// -- Set up required URL handlers on the web server.
	server.on("/", HTTP_GET, handleRoot);
	server.on("/config", HTTP_ANY, [](AsyncWebServerRequest* request) {
			AsyncWebRequestWrapper asyncWebRequestWrapper(request);
			iotWebConf.handleConfig(&asyncWebRequestWrapper);
		}
	);
	server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest* request) {
		AsyncWebServerResponse* response = request->beginResponse_P(200, "image/x-icon", favicon_ico_gz, favicon_ico_gz_len);
		response->addHeader("Content-Encoding", "gzip");
		request->send(response);
		}
	);
	server.onNotFound([](AsyncWebServerRequest* request) { 
			AsyncWebRequestWrapper asyncWebRequestWrapper(request);
			iotWebConf.handleNotFound(&asyncWebRequestWrapper);
		}
	);
}
```

Call the doLoop method in the loop function as frequently as possible.
```cpp
void loop()
{
  iotWebConf.doLoop();
}
```
## Debugging
If you add the following define in your code, you can enable debugging
```c++
#define IOTWEBCONFASYNC_DEBUG_TO_SERIAL
```