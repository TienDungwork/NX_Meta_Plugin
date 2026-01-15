// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace object_detection {

static const std::string kDeviceAgentManifest = /*suppress newline*/ 1 + (const char*) R"json(
{
    "typeLibrary":
    {
        "objectTypes": []
    },
    "supportedTypes":
    [
        {
            "objectTypeId": "nx.base.Face",
            "attributes":
            [
                "Gender",
                "Age",
                "Emotion",
                "Name"
            ]
        },
        {
            "objectTypeId": "nx.base.LicensePlate",
            "attributes":
            [
                "Number",
                "Country",
                "State/Province",
                "Size",
                "Color"
            ]
        },
        {
            "objectTypeId": "nx.base.Animal",
            "attributes":
            [
                "Size",
                "Color"
            ]
        },
        {
            "objectTypeId": "nx.base.Unknown",
            "attributes": []
        },
        {
            "objectTypeId": "nx.base.Car",
            "attributes":
            [
                "Type",
                "Color",
                "Speed",
                "Brand",
                "Model",
                "Size",
                "License Plate",
                "Driver buckled up",
                "Lane"
            ]
        },
        {
            "objectTypeId": "nx.base.Truck",
            "attributes":
            [
                "Type",
                "Color",
                "Speed",
                "Brand",
                "Model",
                "Size",
                "License Plate",
                "Driver buckled up",
                "Lane"
            ]
        },
        {
            "objectTypeId": "nx.base.Bus",
            "attributes":
            [
                "Type",
                "Color",
                "Speed",
                "Brand",
                "Model",
                "Size",
                "License Plate",
                "Driver buckled up",
                "Lane"
            ]
        },
        {
            "objectTypeId": "nx.base.Train",
            "attributes":
            [
                "Color",
                "Speed",
                "Brand",
                "Model",
                "Size",
                "License Plate",
                "Driver buckled up",
                "Lane"
            ]
        },
        {
            "objectTypeId": "nx.base.Tram",
            "attributes":
            [
                "Color",
                "Speed",
                "Brand",
                "Model",
                "Size",
                "License Plate",
                "Driver buckled up",
                "Lane"
            ]
        },
        {
            "objectTypeId": "nx.base.Bike",
            "attributes":
            [
                "Type",
                "Color",
                "Speed",
                "Brand",
                "Model",
                "Size",
                "License Plate",
                "Driver buckled up",
                "Lane"
            ]
        },
        {
            "objectTypeId": "nx.base.Special",
            "attributes":
            [
                "Type",
                "Color",
                "Speed",
                "Brand",
                "Model",
                "Size",
                "License Plate",
                "Driver buckled up",
                "Lane"
            ]
        },
        {
            "objectTypeId": "nx.base.WaterTransport",
            "attributes":
            [
                "Color",
                "Speed",
                "Brand",
                "Model",
                "Size",
                "License Plate",
                "Driver buckled up",
                "Lane"
            ]
        },
        {
            "objectTypeId": "nx.base.AirTransport",
            "attributes":
            [
                "Color",
                "Speed",
                "Brand",
                "Model",
                "Size",
                "License Plate",
                "Driver buckled up",
                "Lane"
            ]
        },
        {
            "objectTypeId": "nx.base.Cat",
            "attributes":
            [
                "Size",
                "Color"
            ]
        },
        {
            "objectTypeId": "nx.base.Dog",
            "attributes":
            [
                "Size",
                "Color"
            ]
        },
        {
            "objectTypeId": "nx.base.Fish",
            "attributes":
            [
                "Size",
                "Color"
            ]
        },
        {
            "objectTypeId": "nx.base.Snake",
            "attributes":
            [
                "Size",
                "Color"
            ]
        },
        {
            "objectTypeId": "nx.base.Bird",
            "attributes":
            [
                "Size",
                "Color"
            ]
        },
        {
            "objectTypeId": "nx.base.Helmet",
            "attributes": []
        },
        {
            "objectTypeId": "nx.base.SafetyVest",
            "attributes": []
        },
        {
            "objectTypeId": "nx.base.Forklift",
            "attributes": []
        },
        {
            "objectTypeId": "nx.base.Fire",
            "attributes": []
        },
        {
            "objectTypeId": "nx.base.Smoke",
            "attributes": []
        }
    ]
}
)json";

} // namespace object_detection
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
