// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "polygon_http_api.h"

#include "http_server.h"
#include "roi_store.h"

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

static std::string queryParam(const std::string& query, const std::string& key)
{
    // Very small query parser: "a=b&c=d". No decoding.
    const std::string needle = key + "=";
    size_t pos = 0;
    while (pos < query.size())
    {
        size_t amp = query.find('&', pos);
        if (amp == std::string::npos)
            amp = query.size();
        const std::string part = query.substr(pos, amp - pos);
        if (part.rfind(needle, 0) == 0)
            return part.substr(needle.size());
        pos = amp + 1;
    }
    return {};
}

static int hexValue(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F')
        return 10 + (c - 'A');
    return -1;
}

static std::string urlDecode(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i)
    {
        const char c = s[i];
        if (c == '+')
        {
            out.push_back(' ');
            continue;
        }
        if (c == '%' && i + 2 < s.size())
        {
            const int hi = hexValue(s[i + 1]);
            const int lo = hexValue(s[i + 2]);
            if (hi >= 0 && lo >= 0)
            {
                out.push_back((char) ((hi << 4) | lo));
                i += 2;
                continue;
            }
        }
        out.push_back(c);
    }
    return out;
}

static std::string trimBraces(std::string s)
{
    if (!s.empty() && s.front() == '{')
        s.erase(s.begin());
    if (!s.empty() && s.back() == '}')
        s.pop_back();
    return s;
}

static void appendPolygonIfPresent(
    nx::kit::Json::array& out,
    const std::string& name,
    const std::string& valueJsonStr)
{
    std::string err;
    const nx::kit::Json v = nx::kit::Json::parse(valueJsonStr, err);
    if (!err.empty() || !v.is_object())
        return;

    const auto fig = v["figure"];
    if (!fig.is_object())
        return;

    const auto points = fig["points"];
    if (!points.is_array() || points.array_items().empty())
        return;

    const std::string color = fig["color"].is_string() ? fig["color"].string_value() : "";
    const std::string label = v["label"].is_string() ? v["label"].string_value() : "";
    const bool showOnCamera = v["showOnCamera"].is_bool() ? v["showOnCamera"].bool_value() : false;

    nx::kit::Json::array pts;
    for (const auto& p : points.array_items())
    {
        if (!p.is_array() || p.array_items().size() < 2)
            continue;
        nx::kit::Json::array pt;
        pt.push_back(p.array_items()[0].number_value());
        pt.push_back(p.array_items()[1].number_value());
        pts.push_back(std::move(pt));
    }

    out.push_back(nx::kit::Json(nx::kit::Json::object{
        {"name", name},
        {"points", pts},
        {"color", color},
        {"label", label},
        {"showOnCamera", showOnCamera}
    }));
}

} // namespace

bool startPolygonHttpApi(int port)
{
    server().setHandler("GET", "/health", [](const HttpServer::Request&) {
        return jsonResponse(nx::kit::Json(nx::kit::Json::object{{"ok", true}}));
    });

    // Compatible with scripts/test_polygon_http.py: GET /polygon?camera_id=...
    server().setHandler("GET", "/polygon", [](const HttpServer::Request& req) {
        const std::string cameraIdEncoded = queryParam(req.query, "camera_id");
        if (cameraIdEncoded.empty())
            return badRequest("Missing camera_id");

        const std::string cameraIdRaw = urlDecode(cameraIdEncoded);
        const std::string cameraId = trimBraces(cameraIdRaw);
        const auto settings = RoiStore::instance().settingsForDevice(cameraId);

        nx::kit::Json::array polygons;

        // As requested: expose ONLY "Excluded area".
        const auto it = settings.find("excludedArea.figure");
        if (it != settings.end())
            appendPolygonIfPresent(polygons, "Excluded area", it->second);

        const nx::kit::Json response(nx::kit::Json::object{
            {"event", "polygons_updated"},
            {"camera_id", cameraIdRaw},
            {"polygons", polygons}
        });
        return jsonResponse(response);
    });

    return server().start(port);
}

void stopPolygonHttpApi()
{
    server().stop();
}

} // namespace nx::vms_server_plugins::analytics::stub::common

