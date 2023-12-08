#pragma once

#include <ESPAsyncWebServer.h>

namespace webpages {

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

void register_demos( node::Webserver &web )
{
  web.add_handler( "/get_demo",  HTTP_GET,  handle_get_demo );
  web.add_handler( "/post_demo", HTTP_POST, handle_post_demo );
}

} // webpages