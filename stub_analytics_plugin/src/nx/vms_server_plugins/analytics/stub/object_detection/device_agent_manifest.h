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
    "supportedTypes":
    [
        {
            "objectTypeId": "nx.base.Vehicle",
            "attributes":
            [
                "Color",
                "Speed",
                "Brand",
                "Model",
                "Size",
                "License Plate",
                "License Plate.Number",
                "License Plate.Country",
                "License Plate.State/Province",
                "License Plate.Size",
                "License Plate.Color",
                "Driver buckled up",
                "Lane"
            ]
        },
        {
            "objectTypeId": "nx.base.Face",
            "attributes":
            [
                "Gender",
                "Race",
                "Age",
                "Shape",
                "Length",
                "Emotion",
                "Hat",
                "Hat.Color",
                "Hat.Type",
                "Hair Color",
                "Hair Type",
                "Eyelid",
                "Eyebrow Width",
                "Eyebrow Space",
                "Eyebrow Color",
                "Eyes",
                "Mouth",
                "Eyes Shape",
                "Eyes Color",
                "Nose Length",
                "Nose Bridge",
                "Nose Wing",
                "Nose End",
                "Facial Hair",
                "Facial Hair.Type",
                "Ear Type",
                "Lip Type",
                "Chin Type",
                "Freckles",
                "Tattoo",
                "Mole",
                "Scar",
                "Temperature",
                "Name",
                "Cigarette",
                "Cigarette.Type",
                "Mask",
                "Glasses",
                "Glasses.Type",
                "Helmet"
            ]
        },
        {
            "objectTypeId": "nx.base.Person",
            "attributes":
            [
                "Gender",
                "Age",
                "Hat.Type",
                "Name",
                "Bag.Color",
                "Weapon",
                "Mask",
                "Glasses.Type",
                "Helmet"
            ]
        },
        {
            "objectTypeId": "nx.base.Unknown",
            "attributes":
            [
                "Name"
            ]
        }
    ]
}
)json";

} // namespace object_detection
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
