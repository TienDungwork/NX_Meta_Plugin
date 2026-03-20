#!/usr/bin/env python3
"""
Subscribe to the same MQTT topic as mqtt_moving_bbox.py and draw detections on a black 1920x1080 canvas.

Edit BROKER / PORT / CAMERA_ID / USERNAME / PASSWORD below to match mqtt_moving_bbox.py (or your broker).

Dependencies: pip install paho-mqtt opencv-python numpy

Example:
  python3 mqtt_bbox_camera_viewer.py
"""

from __future__ import annotations

import json
import sys
import threading
from typing import Any

import cv2
import numpy as np
import paho.mqtt.client as mqtt

# --- MQTT: defaults aligned with scripts/mqtt_moving_bbox.py (change here, no CLI) ---
BROKER = "localhost"
PORT = 1883
CAMERA_ID = "0fdba7df-64c1-ebd4-9e73-677f2d77f211"
# USERNAME = "admin"
# PASSWORD = "Ab@123456"
USERNAME = None
PASSWORD = None

# Black canvas size (fixed).
CANVAS_WIDTH = 1920
CANVAS_HEIGHT = 1080


def normalize_camera_id(camera_id: str) -> str:
    s = camera_id.strip()
    if len(s) > 2 and s[0] == "{" and s[-1] == "}":
        return s[1:-1]
    return s


def topic_for_camera_id(camera_id: str) -> str:
    return f"vms/ai/detections/{normalize_camera_id(camera_id)}"


def parse_detections_payload(payload: bytes) -> list[dict[str, Any]]:
    try:
        data = json.loads(payload.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError):
        return []
    dets = data.get("detections")
    if not isinstance(dets, list):
        return []
    return [d for d in dets if isinstance(d, dict)]


def norm_bbox_to_pixels(
    bbox: list[Any], frame_w: int, frame_h: int
) -> tuple[int, int, int, int] | None:
    if len(bbox) < 4:
        return None
    try:
        x, y, w, h = (float(bbox[0]), float(bbox[1]), float(bbox[2]), float(bbox[3]))
    except (TypeError, ValueError):
        return None
    x1 = int(round(x * frame_w))
    y1 = int(round(y * frame_h))
    x2 = int(round((x + w) * frame_w))
    y2 = int(round((y + h) * frame_h))
    x1 = max(0, min(frame_w - 1, x1))
    y1 = max(0, min(frame_h - 1, y1))
    x2 = max(0, min(frame_w, x2))
    y2 = max(0, min(frame_h, y2))
    if x2 <= x1 or y2 <= y1:
        return None
    return x1, y1, x2, y2


def color_bgr_for_label(label: str) -> tuple[int, int, int]:
    l = (label or "").lower()
    if l in ("face", "person", "people"):
        return (0, 200, 0)
    if l in ("vehicle", "car"):
        return (200, 80, 0)
    return (0, 255, 255)


def main() -> int:
    w, h = CANVAS_WIDTH, CANVAS_HEIGHT
    if w < 16 or h < 16:
        print("CANVAS_WIDTH and CANVAS_HEIGHT must be at least 16.", file=sys.stderr)
        return 1

    topic = topic_for_camera_id(CAMERA_ID)
    lock = threading.Lock()
    detections: list[dict[str, Any]] = []

    def on_connect(
        client: mqtt.Client,
        userdata: Any,
        flags: dict[str, int],
        rc: int,
        properties: Any = None,
    ) -> None:
        if rc == 0:
            client.subscribe(topic, qos=0)
            print(f"Subscribed: {topic}")
        else:
            print(f"MQTT connect failed, rc={rc}", file=sys.stderr)

    def on_message(client: mqtt.Client, userdata: Any, msg: mqtt.MQTTMessage) -> None:
        nonlocal detections
        dets = parse_detections_payload(msg.payload)
        with lock:
            detections = dets

    client = mqtt.Client(
        callback_api_version=mqtt.CallbackAPIVersion.VERSION1,
        client_id="vms_bbox_opencv_viewer",
    )
    client.on_connect = on_connect
    client.on_message = on_message
    if USERNAME is not None:
        client.username_pw_set(USERNAME, PASSWORD or "")

    print(f"Broker: {BROKER}:{PORT}")
    print(f"Topic:  {topic}")
    print(f"Canvas: {w}x{h} black (normalized bbox overlay)")
    print("Press q or ESC to quit.")

    try:
        client.connect(BROKER, PORT, keepalive=60)
        client.loop_start()
    except OSError as e:
        print(f"MQTT connection error: {e}", file=sys.stderr)
        return 1

    window = "MQTT bbox (black canvas)"
    try:
        while True:
            frame = np.zeros((h, w, 3), dtype=np.uint8)
            with lock:
                dets_snapshot = list(detections)

            for det in dets_snapshot:
                bbox = det.get("bbox")
                if not isinstance(bbox, list):
                    continue
                rect = norm_bbox_to_pixels(bbox, w, h)
                if rect is None:
                    continue
                x1, y1, x2, y2 = rect
                label = det.get("label", "")
                if not isinstance(label, str):
                    label = str(label)
                conf = det.get("confidence", "")
                color = color_bgr_for_label(label)
                cv2.rectangle(frame, (x1, y1), (x2, y2), color, 2)
                tail = f" {conf:.2f}" if isinstance(conf, (int, float)) else (f" {conf}" if conf != "" else "")
                text = f"{label}{tail}".strip() or "det"
                cv2.putText(
                    frame,
                    text,
                    (x1, max(0, y1 - 6)),
                    cv2.FONT_HERSHEY_SIMPLEX,
                    0.5,
                    color,
                    1,
                    cv2.LINE_AA,
                )

            cv2.imshow(window, frame)
            key = cv2.waitKey(1) & 0xFF
            if key in (ord("q"), 27):
                break
    finally:
        client.loop_stop()
        try:
            client.disconnect()
        except Exception:
            pass
        cv2.destroyAllWindows()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
