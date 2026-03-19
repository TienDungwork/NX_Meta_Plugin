---
name: nx-stub-roi-object-detection-overlay
description: Hướng dẫn chỉnh `stub_analytics_plugin` để chỉ build ROI + Object Detection, HTTP cho polygon (ROI) và MQTT cho bbox (object_detection).
---

## Mục tiêu

- **Chỉ giữ 3 instance** trong plugin:
  - `roi` (HTTP polygon)
  - `object_detection` (vẽ bbox từ MQTT)
  - `mqtt` (UI cấu hình broker/port/user/pass dùng chung)
- **Bounding box “vẽ lên frame”** trong Nx là việc plugin tạo **Object Metadata** với `boundingBox` (tọa độ chuẩn hoá 0..1) và gọi `pushMetadataPacket()`.
- **Nhận bounding box từ client khác qua MQTT** → client publish detections lên topic theo camera id.
- **Expose ROI polygon qua HTTP** → client gọi HTTP `GET /polygon?camera_id=...` để lấy polygons.

## Hàm “bounding box” trong code

- `stub_analytics_plugin/src/nx/vms_server_plugins/analytics/stub/object_detection/device_agent.cpp`
  - Hàm tạo metadata: `DeviceAgent::generateObjectMetadataPacket()`
  - Chỗ gán bbox: `objectMetadata->setBoundingBox(...)`
  - Chỗ bắn lên VMS: `pushMetadataPacket(objectMetadataPacket)`

## MQTT cho bbox (Object Detection)

- Subscriber MQTT được khởi tạo trong `object_detection::DeviceAgent` theo **camera id**:
  - Topic: `vms/ai/detections/<CAMERA_ID>`
  - `CAMERA_ID` lấy từ `deviceInfo->id()` (uuid có thể kèm `{}` → plugin đã trim).
- MQTT broker config lấy từ **store dùng chung** `common::MqttConfigStore`:
  - Default: host `192.168.1.196`, port `1883`, không user/pass
  - Có thể chỉnh trong UI của stub `nx.stub.mqtt` (mục bên dưới)

Payload JSON (theo scripts của bạn, ví dụ `scripts/mqtt_moving_bbox.py`):

```json
{
  "detections": [
    {
      "label": "Face",
      "confidence": 0.95,
      "bbox": [0.15, 0.20, 0.15, 0.15],
      "trackId": 1
    }
  ]
}
```

Ghi chú:
- `bbox = [x, y, w, h]` đều **normalized 0..1**.
- Mapping label → `typeId` hiện đang tối giản: `typeId = "nx.base." + label` (bạn có thể map lại theo taxonomy bạn muốn).

## HTTP cho polygon (ROI)

Plugin mở HTTP server (nhẹ) để trả polygons theo camera:
- Code: `stub_analytics_plugin/src/nx/vms_server_plugins/analytics/stub/common/polygon_http_api.*`
- HTTP core: `stub_analytics_plugin/src/nx/vms_server_plugins/analytics/stub/common/http_server.*`
- Lưu settings theo camera: `stub_analytics_plugin/src/nx/vms_server_plugins/analytics/stub/common/roi_store.*`

Endpoint tương thích `scripts/test_polygon_http.py`:
- `GET /polygon?camera_id={uuid}` → trả:
  - `polygons`: mảng polygon với `points`, `color`, `label`, `showOnCamera`

### Hành vi theo yêu cầu mới

- HTTP **tự bật luôn** khi ROI DeviceAgent nhận settings (không cần toggle).
- Port mặc định: **8090** (khớp script `scripts/test_polygon_http.py`).
- API chỉ trả **duy nhất** polygon **Excluded area**.
- UI ROI chỉ còn đúng 1 control: **Excluded area**.

## UI cấu hình MQTT (stub riêng `mqtt/`)

- Stub id: `nx.stub.mqtt`
- Mục đích: chỉ để nhập:
  - `mqtt.enabled`
  - `mqtt.host`
  - `mqtt.port`
  - `mqtt.username` / `mqtt.password` (optional)
- Khi bạn Save settings, module này sẽ update `common::MqttConfigStore`. Module `object_detection` sẽ tự detect config đổi và restart subscriber.

## Chỉ build ROI + Object Detection

- Entry point được giới hạn ở:
  - `stub_analytics_plugin/src/nx/vms_server_plugins/analytics/stub/main.cpp`
    - index 0 → ROI
    - index 1 → Object Detection
    - index 2 → MQTT (settings)
- `stub_analytics_plugin/CMakeLists.txt` chỉ compile source trong:
  - `stub/common/*`
  - `stub/roi/*`
  - `stub/object_detection/*`
  - `stub/mqtt/*`
  - `stub/utils.*`
  - `stub/main.cpp`

## Build & deploy

```bash
cd /home/atin/project/NX_Meta_Plugin
bash build_and_deploy.sh
```

Hoặc build riêng:

```bash
cd /home/atin/project/NX_Meta_Plugin/stub_analytics_plugin
cmake -DmetadataSdkDir="/home/atin/project/NX_Meta_Plugin/server_plugin_sdk" -B build .
cmake --build build --config Release -- -j"$(nproc)"
```

