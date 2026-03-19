// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "overlay_http_api.h"

#include "external_overlays.h"
#include "http_server.h"

#include <nx/kit/json.h>

namespace nx::vms_server_plugins::analytics::stub::common {

namespace {

HttpServer& server()
{
    static HttpServer s;
    return s;
}

HttpServer::Response jsonResponse(nx::kit::Json j, int code = 200)
{
    HttpServer::Response r;
    r.statusCode = code;
    r.contentType = "application/json";
    r.body = j.dump();
    return r;
}

HttpServer::Response badRequest(std::string message)
{
    return jsonResponse(nx::kit::Json(nx::kit::Json::object{
        {"ok", false},
        {"error", std::move(message)}
    }), 400);
}

} // namespace

bool startOverlayHttpApi(int port)
{
    // Handlers are idempotent: safe to set every time.
    server().setHandler("GET", "/health", [](const HttpServer::Request&) {
        return jsonResponse(nx::kit::Json(nx::kit::Json::object{{"ok", true}}));
    });

    // ROI: returns last settings map pushed by ROI module.
    server().setHandler("GET", "/roi", [](const HttpServer::Request&) {
        const auto settings = ExternalOverlays::instance().roiSettingsSnapshot();
        nx::kit::Json::object obj;
        for (const auto& [k, v] : settings)
            obj[k] = v;
        return jsonResponse(nx::kit::Json(nx::kit::Json::object{
            {"ok", true},
            {"settings", obj}
        }));
    });

    // Boxes: accept external bounding boxes to be drawn as object metadata.
    // Request JSON:
    // { "boxes": [ { "x":0.1,"y":0.2,"w":0.3,"h":0.4,"typeId":"nx.base.Person","trackId":"optional","confidence":0.9 } ] }
    server().setHandler("POST", "/boxes", [](const HttpServer::Request& req) {
        std::string errors;
        nx::kit::Json root = nx::kit::Json::parse(req.body, errors);
        if (!errors.empty() || !root.is_object())
            return badRequest("Invalid JSON body");

        const auto& boxesJson = root["boxes"];
        if (!boxesJson.is_array())
            return badRequest("Missing 'boxes' array");

        std::vector<ExternalBox> boxes;
        boxes.reserve(boxesJson.array_items().size());

        for (const auto& item : boxesJson.array_items())
        {
            if (!item.is_object())
                continue;

            ExternalBox b;
            b.rect.x = (float) item.value("x", 0.0).number_value();
            b.rect.y = (float) item.value("y", 0.0).number_value();
            b.rect.width = (float) item.value("w", 0.0).number_value();
            b.rect.height = (float) item.value("h", 0.0).number_value();
            b.typeId = item.value("typeId", b.typeId).string_value();
            b.trackId = item.value("trackId", "").string_value();
            b.confidence = item.value("confidence", 1.0).number_value();

            boxes.push_back(std::move(b));
        }

        ExternalOverlays::instance().setBoxes(std::move(boxes));
        return jsonResponse(nx::kit::Json(nx::kit::Json::object{{"ok", true}}));
    });

    return server().start(port);
}

void stopOverlayHttpApi()
{
    server().stop();
}

} // namespace nx::vms_server_plugins::analytics::stub::common

