#pragma once

#include "../private_config.h" // not in repository

#ifndef WIFI_SSID
#define WIFI_SSID PRIVATE_WIFI_SSID
#endif
#ifndef WIFI_PASSWD
#define WIFI_PASSWD PRIVATE_WIFI_PASSWD
#endif
#ifndef WIFI_HOSTNAME
#define WIFI_HOSTNAME "simple_node"
#endif

#ifndef SYSLOG_HOST
#define SYSLOG_HOST "10.23.1.2"
#endif
#ifndef SYSLOG_PORT
#define SYSLOG_PORT 514
#endif

#ifndef NTP_HOST
#define NTP_HOST "europe.pool.ntp.org"
#endif

#ifndef MQTT_HOST
#define MQTT_HOST PRIVATE_MQTT_HOST
#endif
#ifndef MQTT_PORT
#define MQTT_PORT 1883
#endif
#ifndef MQTT_CLIENT
#define MQTT_CLIENT "simple_node"
#endif
#ifndef MQTT_KEEPALIVE
#define MQTT_KEEPALIVE 60  // timeout set to 1.5x this value
#endif
#ifndef MQTT_SUB_TOPIC
#define MQTT_SUB_TOPIC "simple/cmd"
#endif
#ifndef MQTT_PUB_TOPIC
#define MQTT_PUB_TOPIC "simple/status"
#endif

#ifndef PATTERN_PIN
#define PATTERN_PIN LED_BUILTIN
#endif

#ifndef PATTERN_WIFI_DISCONNECTED
#define PATTERN_WIFI_DISCONNECTED 0xAAAAAAAA
#endif
#ifndef PATTERN_WIFI_CONNECTED
#define PATTERN_WIFI_CONNECTED    0xF0F0F0F0
#endif
#ifndef PATTERN_WIFI_GOT_IP
#define PATTERN_WIFI_GOT_IP       0xFFFFFFFE
#endif

#ifndef WEBSERVER_PORT
#define WEBSERVER_PORT 80
#endif

#ifndef CONFIG_FILENAME
#define CONFIG_FILENAME "/node_config.bin"
#endif
