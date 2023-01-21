#pragma once

#include "build.gen.h"
namespace node {
const char *build_version = XXX_BUILD_REPO_VERSION " (" XXX_BUILD_DATE ")";
};

#include "node/defaults.h" // needs to come first for now

#include "node/configuration.h"
#include "node/inputs.h"
#include "node/logging.h"
#include "node/mqtt.h" // costs about 9k loops/sec
#include "node/ntp.h"
#include "node/patterns.h"
#include "node/pixels.h" // costs about 3k loops/sec
#include "node/uptime.h"
#include "node/webserver.h"
#include "node/wifi.h"
