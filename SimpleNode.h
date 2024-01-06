#pragma once

#include "include/defaults.h" // needs to come first for now

#include "include/configuration.h"
#include "include/emoncms.h"
#include "include/inputs.h"
#include "include/logging.h"
#include "include/mqtt.h" // costs about 9k loops/sec
#include "include/ntp.h"
#include "include/patterns.h"
#include "include/pixels.h" // costs about 3k loops/sec
#include "include/uptime.h"
#include "include/webserver.h"
#include "include/wifi.h"
