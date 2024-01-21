#pragma once

#include "app_config.h"
#include <SimpleNode.h>

extern node::Configuration< app::Config > configuration;
extern node::EmonCms emoncms;
extern node::Mqtt mqtt;
extern node::Ntp ntp;
extern node::Syslog syslog;
extern node::Uptime uptime;
extern node::Webserver web;
extern node::WiFi wifi;
extern node::WifiPatterns patterns;
extern node::Logger app_log;

void app_setup( );
bool parse_cmd( const char *str );
