/**
 * IotWebConf04Tab.ino -- Example demonstrating tab support in AsyncIotWebConf
 * 
 * This example shows how to use IotWebConfAsyncTab to create a configuration
 * page with multiple tabs for better organization of settings.
 * 
 * Copyright (c) 2024 Andreas Zogg
 * 
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include <IotWebConfAsyncClass.h>
#include <IotWebConfAsyncTab.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>

// -- Configuration specific constants --
#define CONFIG_VERSION "tab1"
#define CONFIG_PIN 2
#define STATUS_PIN LED_BUILTIN
#define ON_LEVEL LOW

// -- Initial name and password for the access point --
const char thingName[] = "testThing";
const char wifiInitialApPassword[] = "12345678";

// -- Web server and DNS server objects --
DNSServer dnsServer;
AsyncWebServer server(80);
AsyncWebServerWrapper asyncWebServerWrapper(&server);

// -- IotWebConfAsyncTab instance --
AsyncIotWebConfTab iotWebConf(thingName, &dnsServer, &asyncWebServerWrapper, wifiInitialApPassword, CONFIG_VERSION);

// -- Parameter groups for different tabs --

// Network Tab Parameters
char networkHostValue[STRING_LEN];
char networkPortValue[NUMBER_LEN];

iotwebconf::ParameterGroup networkGroup = iotwebconf::ParameterGroup("network", "Network Settings");
iotwebconf::TextParameter networkHostParam = iotwebconf::TextParameter("Host", "networkHost", networkHostValue, STRING_LEN, "server.example.com");
iotwebconf::NumberParameter networkPortParam = iotwebconf::NumberParameter("Port", "networkPort", networkPortValue, NUMBER_LEN, "8080", "1..65535", "min='1' max='65535' step='1'");

// MQTT Tab Parameters
char mqttServerValue[STRING_LEN];
char mqttPortValue[NUMBER_LEN];
char mqttUserValue[STRING_LEN];
char mqttPasswordValue[PASSWORD_LEN];

iotwebconf::ParameterGroup mqttGroup = iotwebconf::ParameterGroup("mqtt", "MQTT Settings");
iotwebconf::TextParameter mqttServerParam = iotwebconf::TextParameter("MQTT Server", "mqttServer", mqttServerValue, STRING_LEN, "mqtt.example.com");
iotwebconf::NumberParameter mqttPortParam = iotwebconf::NumberParameter("MQTT Port", "mqttPort", mqttPortValue, NUMBER_LEN, "1883", "1..65535", "min='1' max='65535' step='1'");
iotwebconf::TextParameter mqttUserParam = iotwebconf::TextParameter("MQTT User", "mqttUser", mqttUserValue, STRING_LEN);
iotwebconf::PasswordParameter mqttPasswordParam = iotwebconf::PasswordParameter("MQTT Password", "mqttPassword", mqttPasswordValue, PASSWORD_LEN);

// Sensor Tab Parameters
char sensorIntervalValue[NUMBER_LEN];
iotwebconf::CheckboxParameter sensorEnabledParam = iotwebconf::CheckboxParameter("Enable Sensor", "sensorEnabled");

iotwebconf::ParameterGroup sensorGroup = iotwebconf::ParameterGroup("sensor", "Sensor Settings");
iotwebconf::NumberParameter sensorIntervalParam = iotwebconf::NumberParameter("Read Interval (s)", "sensorInterval", sensorIntervalValue, NUMBER_LEN, "60", "10..3600", "min='10' max='3600' step='1'");

// System Tab Parameters (custom parameters for system tab)
char customSystemValue[STRING_LEN];
iotwebconf::ParameterGroup systemCustomGroup = iotwebconf::ParameterGroup("systemCustom", "Custom System Settings");
iotwebconf::TextParameter customSystemParam = iotwebconf::TextParameter("Device Name", "deviceName", customSystemValue, STRING_LEN, "MyDevice");

// -- Callback for configuration changes --
void configSaved()
{
  Serial.println("Configuration saved!");
  Serial.print("Network Host: ");
  Serial.println(networkHostValue);
  Serial.print("MQTT Server: ");
  Serial.println(mqttServerValue);
  Serial.print("Sensor Interval: ");
  Serial.println(sensorIntervalValue);
}

// -- Callback for form validation --
bool formValidator(iotwebconf::WebRequestWrapper* webRequestWrapper)
{
  Serial.println("Validating form...");

  // Example: Validate MQTT port
  int port = atoi(mqttPortValue);
  if (port < 1 || port > 65535)
  {
    mqttPortParam.errorMessage = "Invalid port number!";
    return false;
  }

  return true;
}

// -- Root page handler --
void handleRoot(AsyncWebServerRequest* request)
{
  if (iotWebConf.handleCaptivePortal(request))
  {
    return;
  }

  String html = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>";
  html += "<title>";
  html += iotWebConf.getThingName();
  html += "</title></head><body>";
  html += "<h1>";
  html += iotWebConf.getThingName();
  html += "</h1>";
  html += "<p>IotWebConfAsyncTab example with multiple tabs.</p>";
  html += "<ul>";
  html += "<li>Network Host: ";
  html += networkHostValue;
  html += "</li>";
  html += "<li>MQTT Server: ";
  html += mqttServerValue;
  html += "</li>";
  html += "<li>Sensor Interval: ";
  html += sensorIntervalValue;
  html += " seconds</li>";
  html += "<li>Sensor Enabled: ";
  html += sensorEnabledParam.isChecked() ? "Yes" : "No";
  html += "</li>";
  html += "</ul>";
  html += "<p>Go to <a href='config'>configuration page</a> to change values.</p>";
  html += "</body></html>\n";

  request->send(200, "text/html", html);
}

void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.println("IotWebConfAsyncTab Example - Starting up...");

  // -- Setup parameter groups --
  networkGroup.addItem(&networkHostParam);
  networkGroup.addItem(&networkPortParam);

  mqttGroup.addItem(&mqttServerParam);
  mqttGroup.addItem(&mqttPortParam);
  mqttGroup.addItem(&mqttUserParam);
  mqttGroup.addItem(&mqttPasswordParam);

  sensorGroup.addItem(&sensorEnabledParam);
  sensorGroup.addItem(&sensorIntervalParam);

  systemCustomGroup.addItem(&customSystemParam);

  // -- Add parameter groups to tabs --
  // System tab will be at position 0 (first) by default
  iotWebConf.setSystemTabName("System");
  iotWebConf.setSystemTabPosition(0);  // 0 = first, -1 = last, or any position

  // Add custom system parameters to system tab
  iotWebConf.addParameterGroup(&systemCustomGroup, "System");

  // Add other parameter groups to their respective tabs
  iotWebConf.addParameterGroup(&networkGroup, "Network");
  iotWebConf.addParameterGroup(&mqttGroup, "MQTT");
  iotWebConf.addParameterGroup(&sensorGroup, "Sensor");

  // -- Optional: Set container width for better layout --
  iotWebConf.setContainerWidth(500, 700);

  // -- Setup status and config pins --
  iotWebConf.setStatusPin(STATUS_PIN, ON_LEVEL);
  iotWebConf.setConfigPin(CONFIG_PIN);

  // -- Set callbacks --
  iotWebConf.setConfigSavedCallback(&configSaved);
  iotWebConf.setFormValidator(&formValidator);

  // -- Make AP timeout visible --
  iotWebConf.getApTimeoutParameter()->visible = true;

  // -- Initialize IotWebConf --
  iotWebConf.init();

  // -- Setup web server routes --
  server.on("/", HTTP_GET, handleRoot);

  server.on("/config", HTTP_ANY, [](AsyncWebServerRequest* request) {
    // IMPORTANT: You must create the AsyncWebRequestWrapper with 'new' and do NOT delete it manually.
    // The object will delete itself when it is no longer needed.
    auto* asyncWebRequestWrapper = new AsyncWebRequestWrapper(request);
    iotWebConf.handleConfig(asyncWebRequestWrapper);
  });

  server.onNotFound([](AsyncWebServerRequest* request) {
    AsyncWebRequestWrapper asyncWebRequestWrapper(request);
    iotWebConf.handleNotFound(&asyncWebRequestWrapper);
  });

  Serial.println("Ready.");
}

void loop()
{
  iotWebConf.doLoop();

  // -- Add your application logic here --
}