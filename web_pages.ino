
namespace webpages {

void handle_default( AsyncWebServerRequest* request )
{
String message;
message += "Build ";
message += Version;
message += "\n";
message += "Uptime ";
message += uptime.secs();
message += " seconds\n";

request->send(200, "text/plain", message);
}

// ----------------------------------------------------------------------------

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

// ----------------------------------------------------------------------------

#define PARAM_MESSAGE "message"

void handle_get_demo( AsyncWebServerRequest* request )
{
  int params = request->params();
  for(int i=0;i<params;i++){
    AsyncWebParameter* p = request->getParam(i);
    if(p->isFile()){ //p->isPost() is also true
      Serial.printf("FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
    } else if(p->isPost()){
      Serial.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
    } else {
      Serial.printf("GET[%s]: %s\n", p->name().c_str(), p->value().c_str());
    }
  }

  // Send a GET request to <IP>/get?message=<message>
  String message;
  if (request->hasParam(PARAM_MESSAGE))
    message = request->getParam(PARAM_MESSAGE)->value();
  else
    message = "No message sent";

  request->send(200, "text/plain", "Hello, GET: " + message);
}

void handle_post_demo( AsyncWebServerRequest* request )
{
  // Send a POST request to <IP>/post with a form field message set to <message>
  String message;
  if (request->hasParam(PARAM_MESSAGE, true/*is_post*/))
    message = request->getParam(PARAM_MESSAGE, true)->value();
  else
    message = "No message sent";

  request->send(200, "text/plain", "Hello, POST: " + message);
}

#undef PARAM_MESSAGE

// ----------------------------------------------------------------------------

#ifdef NODE_HAS_WEB_UPDATE

Logger update_log( "UPDATE" );

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
      Update.runAsync(true);
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
  schedule_function( [](){ delay(500); ESP.restart(); } );
}

#endif

}; // webpages

#if defined(NODE_HAS_WEB_UPDATE) || defined(NODE_HAS_OTA_UPDATE)
#include "update_helpers.h"
static MyUpdaterHashClass muh;
static MyUpdaterVerifyClass muv;
#endif

void register_web_pages( Webserver& web)
{
  web.add_handler( "/", HTTP_GET, webpages::handle_default );

  #if defined(NODE_HAS_WEB_UPDATE) || defined(NODE_HAS_OTA_UPDATE)
  Update.installSignature( &muh, &muv );
  #endif

  #ifdef NODE_HAS_WEB_UPDATE
  web.add_handler( "/update", HTTP_GET, webpages::handle_update );
  web.add_handler( "/update", HTTP_POST, webpages::handle_update_done, webpages::handle_update_progress );
  web.add_handler( "/reboot", HTTP_POST, webpages::handle_reboot );
  #endif

  web.add_handler( "/get_demo", HTTP_GET, webpages::handle_get_demo );
  web.add_handler( "/post_demo", HTTP_POST, webpages::handle_post_demo );
}