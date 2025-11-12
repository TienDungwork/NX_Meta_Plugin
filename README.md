mkdir build

cmake -DmetadataSdkDir=/home/atin/develop/metadata_sdk -B ./build .

cmake --build ./build --config Release -- -j


sudo cp /home/atin/develop/opencv_object_detection_analytics_plugin/build/libcustom_plugin_analytics_plugin.so /opt/networkoptix-metavms/mediaserver/bin/plugins/


sudo systemctl restart networkoptix-metavms-mediaserver