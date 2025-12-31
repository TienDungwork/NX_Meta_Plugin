
```sh
# Create build directory
mkdir build

# Configure the project with CMake
cmake -DmetadataSdkDir=/home/atin/develop/metadata_sdk -B ./build .

# Build the project in Release mode
cmake --build ./build --config Release -- -j

# Copy the custom analytics plugin to the plugins directory
sudo cp /home/atin/develop/custom_plugin/build/libstub_analytics_plugin.so /opt/networkoptix-metavms/mediaserver/bin/plugins/

# Restart the mediaserver service
sudo systemctl restart networkoptix-metavms-mediaserver
```
