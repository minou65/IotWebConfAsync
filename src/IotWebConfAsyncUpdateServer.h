/**
 * IotWebConfAsyncUpdateServer.h -- IotWebConf is an ESP8266/ESP32
 *   non blocking WiFi/AP web configuration library for Arduino.
 *   https://github.com/prampec/IotWebConf
 *
 * Copyright (C) 2020 Balazs Kelemen <prampec+arduino@gmail.com>
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 *
 * Notes on IotWebConfESP32HTTPUpdateServer:
 * ESP32 doesn't implement a HTTPUpdateServer. However it seams, that to code
 * from ESP8266 covers nearly all the same functionality.
 * So we need to implement our own HTTPUpdateServer for ESP32, and code is
 * reused from
 * https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266HTTPUpdateServer/src/
 * version: 41de43a26381d7c9d29ce879dd5d7c027528371b
 */
#ifdef ESP32

#ifndef _IOTWEBCONFASYNCUPDATESERVER_h
#define _IOTWEBCONFASYNCUPDATESERVER_h

#if defined(ARDUINO) && ARDUINO >= 100
#include "arduino.h"
#else
#include "WProgram.h"
#endif

#include <WiFi.h>
#include <WiFiClient.h>
#include <ESPAsyncWebServer.h>
#include <StreamString.h>
#ifdef ESP8266
#include <Updater.h>
#define U_PART U_FS
#elif defined(ESP32)
#include <Update.h>
#define U_PART U_SPIFFS
size_t  contentlen;
#endif

#define emptyString F("")

void handleUpload(AsyncWebServerRequest* request, const String& filename, size_t index, uint8_t* data, size_t len, bool final){
    if (!index) {
        Serial.println("Update started...");
		WebSerial.println("Update started...");
        size_t _contentlen = request->contentLength();
        // if filename includes spiffs, update the spiffs partition
        int cmd = (filename.indexOf("spiffs") > -1) ? U_PART : U_FLASH;
#ifdef ESP8266
        Update.runAsync(true);
        if (!Update.begin(content_len, cmd)) {
#else
		contentlen = _contentlen;
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, cmd)) {
#endif
            Update.printError(Serial);
        }
    }

    if (Update.write(data, len) != len) {
        Update.printError(Serial);
#ifdef ESP8266
    }
    else {
        Serial.printf("Progress: %d%%\n", (Update.progress() * 100) / Update.size());
#endif

    }

    if (final) {
        AsyncWebServerResponse* response = request->beginResponse(302, "text/plain", "Please wait while the device reboots");
        response->addHeader("Refresh", "15");
        response->addHeader("Location", "/");
        request->client()->setNoDelay(true); // Disable Nagle's algorithm so the client gets the 302 response immediately
        request->send(response);
        if (!Update.end(true)) {
            Update.printError(Serial);
            request->send(200, "text/plain", String(F("Update error: ")) + Update.getError());
        }
        else {
			delay(500);
            Serial.println("Update completed. Will reboot esp");
            Serial.flush();
            ESP.restart();
        }
    }
}

#ifdef ESP32    
void printProgress(size_t prg, size_t sz) {
    static size_t lastPrinted = 0;
    size_t currentPercent = (prg * 100) / sz;

    if (currentPercent % 5 == 0 && currentPercent != lastPrinted) {
        Serial.printf("Progress: %d%%\n", currentPercent);
        WebSerial.printf("Progress: %d%%\n", currentPercent);
        lastPrinted = currentPercent;
    }
}
#endif

class AsyncWebServer;

class AsyncUpdateServer {
public:
    AsyncUpdateServer(bool serial_debug = false)
    {
        _serial_output = serial_debug;
        _server = nullptr;
        _username = emptyString;
        _password = emptyString;
        _authenticated = false;
    }

    void setup(AsyncWebServer* server) {
        setup(server, emptyString, emptyString);
    }

    void setup(AsyncWebServer* server, const String& path) {
        setup(server, path, emptyString, emptyString);
    }

    void setup(AsyncWebServer* server, const String& username, const String& password) {
        setup(server, "/update", username, password);
    }

    void setup(AsyncWebServer* server, const String& path, const String& username, const String& password) {
        _server = server;
        _username = username;
        _password = password;

        // handler for the /update form page
        _server->on(path.c_str(), HTTP_GET, 
            [&](AsyncWebServerRequest* request) {
                _authenticated = (_username == emptyString || _password == emptyString || request->authenticate(_username.c_str(), _password.c_str()));
                if (!_authenticated) {
                    request->requestAuthentication();
                    return;
                }
                request->send(200, "text/html", formFirmware);
            }
        );


        // handler for the /update form POST (once file upload finishes)
        _server->on(path.c_str(), HTTP_POST, 
            [](AsyncWebServerRequest* request) {
				Serial.println("Update POST request");
            },
            [](AsyncWebServerRequest* request, const String& filename, size_t index, uint8_t* data, size_t len, bool final) {
                handleUpload(request, filename, index, data, len, final);
			}
        );

#ifdef ESP32
        Update.onProgress(printProgress);
#endif
    }

    void updateCredentials(const String& username, const String& password) {
        _username = username;
        _password = password;
    }

protected:
    void _setUpdaterError() {
        if (_serial_output) Update.printError(Serial);
        StreamString str;
        Update.printError(str); 
        _updaterError = str.c_str();
    }

private:
    bool _serial_output;
    AsyncWebServer* _server;
    String _username;
    String _password;
    bool _authenticated;
    String _updaterError;
    size_t _contentlen;
    const char* formFirmware PROGMEM = R"(
<!DOCTYPE html>
<html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>
<style> 
	.de{background-color:#ffaaaa;} 
    .em{font-size:0.8em;color:#bb0000;padding-bottom:0px;} 
    .c{text-align: center;} 
    div,input,select{padding:5px;font-size:1em;} 
    input{width:95%;} select{width:100%} 
    input[type=checkbox]{width:auto;scale:1.5;margin:10px;} 
    body{text-align: center;font-family:verdana;} 
    button{border:0;border-radius:0.3rem;background-color:#16A1E7;color:#fff;line-height:2.4rem;font-size:1.2rem;width:100%;} 
    fieldset{border-radius:0.3rem;margin: 0px;}
</style> 
</head><body> 
    <table border="0" align="center"> 
        <tbody><tr><td> 
    </head><body>
        <table border=0 align=center width=50%>
            <tr><td>
                <form method = "POST" action = "/firmware" enctype = "multipart/form-data">
                    <fieldset style="border: 1px solid">
                        <legend>Firmware update</legend>
                        <input type="file" name="update" id="updateFile" style="width: 500px"><br>
                        <button type="submit">Upload</button>

                    </fieldset>
                </form>
            </td></td>
        </table>
        <table border=0 align=center>
            <tr><td align=left>Go to <a href = 'config'>configure page</a> to change configuration.</td></tr>
            <tr><td align=left>Go to <a href='/'>main page</a>.</td></tr>
		</table>
	</body></html>  
)";
        
};

/////////////////////////////////////////////////////////////////////////////////

#endif

#endif
