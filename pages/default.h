#pragma once

#include <ESPAsyncWebServer.h>

namespace webpages {

void handle_default( AsyncWebServerRequest *request, node::Uptime &uptime )
{
  static String reset_reason = ESP.getResetInfo();
    // capture this once; it's not going to change.

  String message;
  message += WIFI_HOSTNAME;
  message += "\n\nBuild: ";
  message += app::build_version;
  message += "\nLast reset: ";
  message += reset_reason;
  message += "\nUptime: ";
  message += uptime.secs();
  message += " seconds\n";

  request->send(200, "text/plain", message);
}

// ----------------------------------------------------------------------------

void register_default( node::Webserver &web, node::Uptime &uptime )
{
  web.add_handler( "/", HTTP_GET,
    [&uptime]( AsyncWebServerRequest *r ){ handle_default( r, uptime ); }
  );
}

} // webpages