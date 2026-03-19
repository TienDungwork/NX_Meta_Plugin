#!/bin/bash
PLUGIN_DIR="/home/atin/project/NX_Meta_Plugin/stub_analytics_plugin"
METADATA_SDK="/home/atin/project/NX_Meta_Plugin/server_plugin_sdk"
PLUGIN_INSTALL_DIR="/opt/networkoptix-metavms/mediaserver/bin/plugins"
PLUGIN_FILE="libstub_analytics_plugin.so"

cd "$PLUGIN_DIR"
cmake -DmetadataSdkDir="$METADATA_SDK" -B ./build .
cmake --build ./build --config Release -- -j$(nproc)
sudo cp "./build/$PLUGIN_FILE" "$PLUGIN_INSTALL_DIR/"
sudo systemctl restart networkoptix-metavms-mediaserver
# sudo systemctl status networkoptix-metavms-mediaserver
