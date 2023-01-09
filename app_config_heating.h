#pragma once

//#define NODE_HAS_PIXELS  // ~3 kHz cost
#define NODE_HAS_NTP
#define NODE_HAS_MQTT  // ~9 kHz cost
#define NODE_HAS_WEB
#define NODE_HAS_WEB_UPDATE
//#define NODE_HAS_OTA_UPDATE
#define NODE_HAS_CONFIG

// ----------------------------------------------------------------------------

#define WIFI_HOSTNAME "heat"

#define MQTT_CLIENT "heat_client"
#define MQTT_SUB_TOPIC "heat/cmd"
#define MQTT_PUB_TOPIC "heat/status"

// ----------------------------------------------------------------------------

struct AppConfig
{
  int counter;
};

// ----------------------------------------------------------------------------

// outputs (all active high)
#define DEMAND_HW_PIN  14 // nodeMCU D5
#define DEMAND_CH1_PIN 12 // nodeMCU D6
#define DEMAND_CH2_PIN 13 // nodeMCU D7
#define DEMAND_CH3_PIN 15 // nodeMCU D8

// inputs (all active high)
#define STAT_HW_PIN    16 // nodeMCU D0
#define STAT_CH1_PIN    5 // nodeMCU D1
#define STAT_CH2_PIN    4 // nodeMCU D2

