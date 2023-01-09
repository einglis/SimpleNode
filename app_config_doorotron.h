#pragma once

//#define NODE_HAS_PIXELS  // ~3 kHz cost
//#define NODE_HAS_NTP
#define NODE_HAS_MQTT  // ~9 kHz cost
#define NODE_HAS_WEB
#define NODE_HAS_WEB_UPDATE
//#define NODE_HAS_OTA_UPDATE
//#define NODE_HAS_CONFIG

// ----------------------------------------------------------------------------

#define WIFI_HOSTNAME "garage"

#define MQTT_CLIENT "garage_client"
#define MQTT_SUB_TOPIC "garage/cmd"
#define MQTT_PUB_TOPIC "garage/status"

// ----------------------------------------------------------------------------

// outputs
#define LED_1_PIN     5 // nodeMCU D1
#define LED_2_PIN     4 // nodeMCU D2
#define BUTTON_A_PIN 14 // nodeMCU D5
#define BUTTON_B_PIN 13 // nodeMCU D7

//inputs
#define SWITCH_A_PIN 16 // nodeMCU D0
#define SWITCH_B_PIN 12 // nodeMCU D6
