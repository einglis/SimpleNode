#pragma once

#include <ESPAsyncWebServer.h>

namespace webpages {

void handle_default( AsyncWebServerRequest *request, node::Uptime &uptime, const char* build_version )
{
  static String reset_reason = ESP.getResetInfo();
    // capture this once; it's not going to change.

  String message;
  message += WIFI_HOSTNAME;
  message += "\n\nBuild: ";
  message += build_version;
  message += "\nLast reset: ";
  message += reset_reason;
  message += "\nUptime: ";

  char buf[32] = { 0 };
  (void)uptime.friendly( buf, sizeof(buf) );
  message += buf;

  request->send(200, "text/plain", message);
}

// ----------------------------------------------------------------------------

void register_default( node::Webserver &web, node::Uptime &uptime, const char* build )
{
  web.add_handler( "/", HTTP_GET,
    [&uptime, build]( AsyncWebServerRequest *r ){ handle_default( r, uptime, build ); }
  );
}

} // webpages