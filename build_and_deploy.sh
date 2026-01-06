#!/bin/bash
PLUGIN_DIR="/home/atin/develop/plugin/NX_Meta_Plugin"
METADATA_SDK="/home/atin/develop/metadata_sdk"
PLUGIN_INSTALL_DIR="/opt/networkoptix-metavms/mediaserver/bin/plugins"
PLUGIN_FILE="libstub_analytics_plugin.so"

cd "$PLUGIN_DIR"
cmake -DmetadataSdkDir="$METADATA_SDK" -B ./build .
cmake --build ./build --config Release -- -j$(nproc)
sudo cp "./build/$PLUGIN_FILE" "$PLUGIN_INSTALL_DIR/"
sudo systemctl restart networkoptix-metavms-mediaserver
# sudo systemctl status networkoptix-metavms-mediaserver

