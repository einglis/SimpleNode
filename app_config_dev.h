#pragma once

#define NODE_HAS_PIXELS  // ~3 kHz cost
#define NODE_HAS_NTP
#define NODE_HAS_MQTT  // ~9 kHz cost
#define NODE_HAS_WEB
#define NODE_HAS_WEB_UPDATE
//#define NODE_HAS_OTA_UPDATE
#define NODE_HAS_INPUTS
#define NODE_HAS_CONFIG

struct AppConfig
{
  int counter;
};

// outputs
#define PIXELS_PIN 13
#define LED_1_PIN  14 // active high
#define LED_2_PIN  16 // active high

// inputs
#define BUTTON_PIN  0 // active low
#define SWITCH_PIN 12 // active low
