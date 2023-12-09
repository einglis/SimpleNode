#pragma once

#include <ESPAsyncWebServer.h>
using node::web::text_type;

namespace webpages {

void handle_default( AsyncWebServerRequest *request, node::Webserver& web,
  node::Uptime &uptime, const char* build_version )
{
  static String reset_reason = ESP.getResetInfo();
    // capture this once; it's not going to change.

   auto buf = web.get_buffer();
  if (!buf)
    return web.server_error_response( request );

  const char *const bend = buf->data() + buf->size();
  char* bp = buf->data();

  if (bend-bp > 0)
    bp += snprintf( bp, bend-bp, "%s\n\n", WIFI_HOSTNAME );
  if (bend-bp > 0)
    bp += snprintf( bp, bend-bp, "Build: %s\n", build_version );
  if (bend-bp > 0)
    bp += snprintf( bp, bend-bp, "Last reset: %s\n", reset_reason.c_str() );
  if (bend-bp > 0)
    bp += snprintf( bp, bend-bp, "Uptime: " );
  if (bend-bp > 0)
    bp += uptime.friendly( bp, bend-bp );

  //Serial.printf("Rendered default page in %u bytes\n", bp-buf->data()+1);
  request->send( 200, text_type, buf->data() );
}

// ----------------------------------------------------------------------------

void register_default( node::Webserver &web, node::Uptime &uptime, const char* build )
{
  web.add_handler( "/", HTTP_GET,
    [&web, &uptime, build]( AsyncWebServerRequest *r ){ handle_default( r, web, uptime, build ); }
  );
}

} // webpages