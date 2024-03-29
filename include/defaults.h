#pragma once

#include "private_config.h" // not in repository

#ifndef WIFI_SSID
#define WIFI_SSID PRIVATE_WIFI_SSID
#endif
#ifndef WIFI_PASSWD
#define WIFI_PASSWD PRIVATE_WIFI_PASSWD
#endif
#ifndef WIFI_HOSTNAME
#define WIFI_HOSTNAME "simple_node"
#endif

#ifndef SYSLOG_IP
#define SYSLOG_IP PRIVATE_SYSLOG_IP  // must be an IP, not a hostname
#endif

#ifndef NTP_HOST
#define NTP_HOST "europe.pool.ntp.org"
#endif

#ifndef MQTT_HOST
#define MQTT_HOST PRIVATE_MQTT_HOST
#endif

#ifndef EMONCMS_HOST
#define EMONCMS_HOST PRIVATE_EMONCMS_HOST
#endif
#ifndef EMONCMS_API_KEY
#define EMONCMS_API_KEY PRIVATE_EMONCMS_API_KEY
#endif

#ifndef CONFIG_FILENAME
#define CONFIG_FILENAME "/node_config.bin"
#endif
