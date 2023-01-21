#pragma once

#include <ESPAsyncWebServer.h>

namespace webpages {

void handle_default( AsyncWebServerRequest *request, node::Uptime &uptime )
{
  String message;
  message += WIFI_HOSTNAME;
  message += "\n\nBuild ";
  message += app::build_version;
  message += "\nUptime ";
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