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
 * ESP32 doesn't implement a HTTPUpdateServer. However it seems, that the code
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
#endif

class AsyncWebServer;

class AsyncUpdateServer {
public:
    AsyncUpdateServer(bool serial_debug = false)
        : _serial_output(serial_debug),
        _server(nullptr),
        _username(String()),
        _password(String()),
        _authenticated(false),
        _updaterError(""),
        _handleUpdateFinished(false)
    {
    }

    void setup(AsyncWebServer* server) {
        setup(server, String(), String());
    }

    void setup(AsyncWebServer* server, const String& path) {
        setup(server, path, String(), String());
#ifdef ESP32
        Update.onProgress(printProgress);
#endif
    }

#ifdef ESP32
    typedef std::function<void(size_t, size_t)> THandlerFunction_Progress;

    void setup(AsyncWebServer* server, const String& path, THandlerFunction_Progress fn) {
        setup(server, path, String(), String());
        Update.onProgress(fn);
    }
#endif

    void setup(AsyncWebServer* server, const String& username, const String& password) {
        setup(server, "/update", username, password);
#ifdef ESP32
        Update.onProgress(printProgress);
#endif
    }

    void setup(AsyncWebServer* server, const String& path, const String& username, const String& password) {
        _server = server;
        _username = username;
        _password = password;

        // handler for the /update form page
        _server->on(path.c_str(), HTTP_GET,
            [this, path](AsyncWebServerRequest* request) {
                _authenticated = (_username == String() || _password == String() || request->authenticate(_username.c_str(), _password.c_str()));
                if (!_authenticated) {
                    request->requestAuthentication();
                    return;
                }
                String form = getFormFirmware(path);
                request->send(200, "text/html", form);
            }
        );

        // handler for the /update form POST (once file upload finishes)
        _server->on(path.c_str(), HTTP_POST,
            [](AsyncWebServerRequest* request) {
                Serial.println("Update POST request");
            },
            [this](AsyncWebServerRequest* request, const String& filename, size_t index, uint8_t* data, size_t len, bool final) {
                handleUpload(request, filename, index, data, len, final, _handleUpdateFinished, _updaterError, _serial_output);
            }
        );
    }

    void updateCredentials(const String& username, const String& password) {
        _username = username;
        _password = password;
    }

    bool isUpdating() {
        return Update.isRunning();
    }

    String getUpdaterError() {
        return _updaterError;
    }

    bool isFinished() {
        return _handleUpdateFinished;
    }

protected:
    static void handleUpload(AsyncWebServerRequest* request, const String& filename, size_t index, uint8_t* data, size_t len, bool final, bool& handleUpdateFinished, String& updaterError, bool serial_output) {
        String html_ = R"(
<html>
<head>
<meta http-equiv="refresh" content="15; url=/">
<title>Rebooting...</title>
</head>
<body>
[Message]
<br>
You will be redirected to the homepage shortly.
</body>
</html>
)";

        if (!index) {
            Serial.println("Update started...");
            size_t content_len = request->contentLength();
            int cmd = (filename.indexOf("spiffs") > -1) ? U_PART : U_FLASH;
#ifdef ESP8266
            Update.runAsync(true);
            if (!Update.begin(content_len, cmd)) {
#else
            if (!Update.begin(UPDATE_SIZE_UNKNOWN, cmd)) {
#endif
                Update.printError(Serial);
            }
            }

        if (Update.write(data, len) != len) {
            Update.printError(Serial);
        }

        if (final) {
            if (!Update.end(true)) {
                StreamString str;
                Update.printError(str);
                updaterError = str.c_str();
                html_.replace("[Message]", "Update error: " + updaterError);
            }
            else {
                html_.replace("[Message]", "Update completed. Please wait while the device is rebooting...");
                Serial.println("Update completed. Please wait while the device is rebooting...");
                handleUpdateFinished = true;
            }
            AsyncWebServerResponse* response = request->beginResponse(200, "text/html", html_);
            request->client()->setNoDelay(true);
            request->send(response);
        }
        }

#ifdef ESP32
    static void printProgress(size_t prg, size_t sz) {
        static size_t lastPrinted = 0;
        size_t currentPercent = (prg * 100) / sz;
        if (currentPercent != lastPrinted) {
            Serial.printf("Progress: %d%%\n", (int)currentPercent);
            lastPrinted = currentPercent;
        }
    }
#endif

    String getFormFirmware(const String & path) {
        // The action must match the update path
        String form = R"(
<!DOCTYPE html>
<html lang="en"><head><meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no"/>
<style>
    .de{background-color:#ffaaaa;}
    .em{font-size:0.8em;color:#bb0000;padding-bottom:0px;}
    .c{text-align: center;}
    div,input,select{padding:5px;font-size:1em;}
    input{width:95%;}
    select{width:100%}
    input[type=checkbox]{width:auto;scale:1.5;margin:10px;}
    body{text-align: center;font-family:verdana;}
    button{border:0;border-radius:0.3rem;background-color:#16A1E7;color:#fff;line-height:2.4rem;font-size:1.2rem;width:100%;}
    fieldset{border-radius:0.3rem;margin: 0px;}
</style>
</head><body>
    <table border="0" align="center">
        <tbody><tr><td>
            <form method="POST" action="[PATH]" enctype="multipart/form-data">
                <fieldset style="border: 1px solid">
                    <legend>Firmware update</legend>
                    <input type="file" name="update" id="updateFile" style="width: 500px"><br>
                    <button type="submit">Upload</button>
                </fieldset>
            </form>
        </td></tr>
    </table>
    <table border=0 align=center>
        <tr><td align=left>Go to <a href='config'>configure page</a> to change configuration.</td></tr>
        <tr><td align=left>Go to <a href='/'>main page</a>.</td></tr>
    </table>
</body></html>
)";
        form.replace("[PATH]", path);
        return form;
    }

private:
    bool _serial_output;
    AsyncWebServer* _server;
    String _username;
    String _password;
    bool _authenticated;
    String _updaterError;
    bool _handleUpdateFinished;
    };

#endif

#endif