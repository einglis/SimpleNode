#pragma once

//#define NODE_HAS_PIXELS  // ~3 kHz cost
#define NODE_HAS_NTP
#define NODE_HAS_MQTT  // ~9 kHz cost
#define NODE_HAS_WEB
#define NODE_HAS_WEB_UPDATE
//#define NODE_HAS_OTA_UPDATE
#define NODE_HAS_INPUTS
#define NODE_HAS_CONFIG

// ----------------------------------------------------------------------------

// outputs
#define LED_PIN   13 // active low
#define RELAY_PIN 12 // active high

// input
#define BUTTON_PIN 0 // active low

// ----------------------------------------------------------------------------

#define PATTERN_PIN           LED_PIN
#define PATTERN_WIFI_GOT_IP   0xFFFFFFFF // no blinking

#define WIFI_HOSTNAME "sonoff"

#define MQTT_CLIENT "sonoff_client"
#define MQTT_SUB_TOPIC "sonoff/cmd"
#define MQTT_PUB_TOPIC "sonoff/status"

// ----------------------------------------------------------------------------

struct AppConfig
{
  int counter;
};
