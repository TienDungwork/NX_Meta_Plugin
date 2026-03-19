// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

namespace nx::vms_server_plugins::analytics::stub::common {

/** Starts (or restarts) HTTP server for ROI polygon endpoint. */
bool startPolygonHttpApi(int port);

/** Stops polygon HTTP API if running. */
void stopPolygonHttpApi();

} // namespace nx::vms_server_plugins::analytics::stub::common

