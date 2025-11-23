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

#ifndef _IOTWEBCONFASYNCUPDATESERVER_h
#define _IOTWEBCONFASYNCUPDATESERVER_h

#if defined(ARDUINO) && ARDUINO >= 100
#include "arduino.h"
#else
#include "WProgram.h"
#endif

#include <ESPAsyncWebServer.h>

class AsyncWebServer;

class AsyncUpdateServer {
public:
    AsyncUpdateServer(bool serial_debug = false);
    void setup(AsyncWebServer* server);
    void setup(AsyncWebServer* server, const String& path);
#ifdef ESP32
    typedef std::function<void(size_t, size_t)> THandlerFunction_Progress;
    void setup(AsyncWebServer* server, const String& path, THandlerFunction_Progress fn);
#endif
    void setup(AsyncWebServer* server, const String& username, const String& password);
    void setup(AsyncWebServer* server, const String& path, const String& username, const String& password);
    void updateCredentials(const String& username, const String& password);
    bool isUpdating();
    String getUpdaterError();
    bool isFinished();
protected:
    static void handleUpload(AsyncWebServerRequest* request, const String& filename, size_t index, uint8_t* data, size_t len, bool final, bool& handleUpdateFinished, String& updaterError, bool serial_output);
#ifdef ESP32
    static void printProgress(size_t prg, size_t sz);
#endif
    String getFormFirmware(const String& path);
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