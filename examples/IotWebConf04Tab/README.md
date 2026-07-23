# IotWebConf04Tab Example

This example demonstrates how to use `AsyncIotWebConfTab` to create a configuration page with multiple tabs for better organization of settings.

## Features

- **Multiple Tabs**: Configuration parameters are organized into different tabs:
  - **System**: WiFi, AP settings, and custom system parameters
  - **Network**: General network settings (host, port)
  - **MQTT**: MQTT broker configuration
  - **Sensor**: Sensor-related settings

- **Tab Positioning**: Shows how to control the position of the system tab using `setSystemTabPosition()`

- **Container Width**: Demonstrates setting custom container width for better layout

- **Form Validation**: Example of custom form validation

- **Callbacks**: Shows how to use configuration saved callback

## Hardware Requirements

- ESP32 board
- Optional: LED connected to STATUS_PIN
- Optional: Button connected to CONFIG_PIN for forcing config mode

## Pin Configuration

- `STATUS_PIN`: Built-in LED (shows status)
- `CONFIG_PIN`: GPIO 2 (pull low to enter config mode)

## Usage

1. Upload the sketch to your ESP32
2. The device creates an access point named "testThing"
3. Connect to it with password "12345678"
4. Navigate to 192.168.4.1
5. Configure WiFi and other settings using the tabbed interface
6. Click "Apply" to save

## Tab Organization

The example shows different ways to organize parameters:
