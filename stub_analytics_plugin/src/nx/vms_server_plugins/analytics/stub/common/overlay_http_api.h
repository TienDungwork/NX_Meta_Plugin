// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

namespace nx::vms_server_plugins::analytics::stub::common {

/** Starts (or restarts) a lightweight HTTP API for feeding overlays into the plugin. */
bool startOverlayHttpApi(int port);

/** Stops the HTTP API if running. */
void stopOverlayHttpApi();

} // namespace nx::vms_server_plugins::analytics::stub::common

