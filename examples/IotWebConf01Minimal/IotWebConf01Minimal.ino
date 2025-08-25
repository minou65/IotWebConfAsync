
// the setup function runs once when you press reset or power the board
#include "favicon.h"
#include <IotWebConf.h>
#include "IotWebConfAsyncClass.h"

// -- Configuration specific key. The value should be modified if config structure was changed.
#define CONFIG_VERSION "A1"

// -- When CONFIG_PIN is pulled to ground on startup, the Thing will use the initial
//      password to buld an AP. (E.g. in case of lost password)
#define CONFIG_PIN  -1

// -- Status indicator pin.
//      First it will light up (kept LOW), on Wifi connection it will blink,
//      when connected to the Wifi it will turn off (kept HIGH).
#define STATUS_PIN LED_BUILTIN
#if ESP32 
#define ON_LEVEL HIGH
#else
#define ON_LEVEL LOW
#endif

// -- Initial name of the Thing. Used e.g. as SSID of the own Access Point.
const char thingName[] = "testThing";

// -- Initial password to connect to the Thing, when it creates an own Access Point.
const char wifiInitialApPassword[] = "123456789";

// -- Method declarations.
void handleRoot(AsyncWebServerRequest* request);

DNSServer dnsServer;
AsyncWebServer server(80);
AsyncWebServerWrapper asyncWebServerWrapper(&server);

IotWebConf iotWebConf(thingName, &dnsServer, &asyncWebServerWrapper, wifiInitialApPassword, CONFIG_VERSION);

void setup() {
	Serial.begin(115200);
	Serial.println();
	Serial.println("Starting up...");

	iotWebConf.setStatusPin(STATUS_PIN, ON_LEVEL);
	iotWebConf.setConfigPin(CONFIG_PIN);
	iotWebConf.getApTimeoutParameter()->visible = true;

	// -- Initializing the configuration.
	iotWebConf.init();
	// -- Set up required URL handlers on the web server.
	server.on("/", HTTP_GET, handleRoot);
	server.on("/config", HTTP_ANY, [](AsyncWebServerRequest* request) {
        auto* asyncWebRequestWrapper = new AsyncWebRequestWrapper(request);
        iotWebConf.handleConfig(asyncWebRequestWrapper);
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

	Serial.println("Ready.");
}

void loop(){
	// -- doLoop should be called as frequently as possible.
	iotWebConf.doLoop();
}


void handleRoot(AsyncWebServerRequest* request){
	AsyncWebRequestWrapper asyncWebRequestWrapper(request);
	if (iotWebConf.handleCaptivePortal(&asyncWebRequestWrapper)){
		return;
	}
	String s = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>";
	s += "<title>IotWebConfAsync 01</title></head><body>";
	s += "Go to <a href='config'>configure page</a> to change settings.";
	s += "</body></html>\n";

	request->send(200, "text/html", s);
}
