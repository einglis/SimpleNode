#pragma once

#include <ESPAsyncWebServer.h>
#if ESP8266
#include <Updater.h>
#elif ESP32
#include <Update.h>
#endif

#include "schedule.h"

namespace webpages {

node::Logger update_log( "UPDATE" );

const char form[] PROGMEM =
  "<html><body>"
  //"<form method='POST' action='update'>"
  //"<input name='fo' value='http://mqtt:80/api/arduino/simple_node.ino.generic.bin' id='filename'>"
  //"<input type='submit'>"
  //"</form>"
  "<form method='POST' enctype='multipart/form-data' action='update'>"
  "<input name='ff' value='' id='filename' type='file'>"
  "<input type='submit'>"
  "</form>"
  "</body></html>"
;

void handle_update( AsyncWebServerRequest* request )
{
  request->send_P( 200, "text/html", form );
}

const char reset_form[] PROGMEM =
  "<html><body>"
  "Success"
  "<form method='POST' action='reboot'>"
  "<input type='submit' value='Reboot'>"
  "</form>"
  "</body></html>"
;

// ------------------------------------

void handle_update_done( AsyncWebServerRequest* request )
{
  AsyncWebServerResponse *response = request->beginResponse(200, "text/html", Update.hasError() ? "failed" : reset_form);
  response->addHeader("Connection", "close");
  request->send(response);
}

void handle_update_progress( AsyncWebServerRequest* request, String filename, size_t index, uint8_t *data, size_t len, bool final )
{
  // this is knowingly very fragile; copied from an example to
  // avoid getting hung up on this rather than the actual project
  (void)request;

  if (index == 0)
  {
      update_log.infof("start: %s", filename.c_str());
      //Update.runAsync(true);
      if (!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000))
      {
        update_log.error( F("failed to start") );
        Update.printError(Serial);
      }
  }

  update_log.debugf("data; index %u, len %u", index, len );
  if (!Update.hasError())
  {
    if (Update.write(data, len) != len)
    {
      update_log.error( F("failed to write") );
      Update.printError(Serial);
    }
  }

  if (final)
  {
    if (Update.end(true))
    {
      update_log.info( F("success") );
    }
    else
    {
      update_log.error( F("failed to complete") );
      Update.printError(Serial);
    }
  }
}

void handle_reboot( AsyncWebServerRequest* request )
{
  request->send(200, "text/plain", "Rebooting..." );
  delay(500);
  ESP.restart();
}

// ----------------------------------------------------------------------------

//#include "update_helpers.h"

void register_update( node::Webserver &web )
{
  //static MyUpdaterHashClass muh;
  //static MyUpdaterVerifyClass muv;
  //Update.installSignature( &muh, &muv );

  web.add_handler( "/update", HTTP_GET,  webpages::handle_update );
  web.add_handler( "/update", HTTP_POST, webpages::handle_update_done, webpages::handle_update_progress );
  web.add_handler( "/reboot", HTTP_POST, webpages::handle_reboot );
}

} // webpages
