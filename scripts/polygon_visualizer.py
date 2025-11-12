import paho.mqtt.client as mqtt
import json
import cv2
import numpy as np
from datetime import datetime
import threading
import time

# Cấu hình MQTT
MQTT_BROKER = "192.168.1.215"
MQTT_PORT = 1883
MQTT_TOPIC = "vms/roi/polygon"
MQTT_CLIENT_ID = "polygon_visualizer"

# Cấu hình RTSP
RTSP_URL = "rtsp://admin:T4123456@192.168.1.20/Streaming/Channels/1"

# Global variables
current_polygons = []
polygons_lock = threading.Lock()
frame_count = 0

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"[{datetime.now()}] ✅ Kết nối MQTT Broker thành công!")
        print(f"Listening on topic: {MQTT_TOPIC}")
        client.subscribe(MQTT_TOPIC)
    else:
        print(f"[{datetime.now()}] ❌ Kết nối thất bại, code: {rc}")

def on_message(client, userdata, msg):
    global current_polygons
    
    try:
        payload = msg.payload.decode('utf-8')
        data = json.loads(payload)
        
        print(f"\n{'='*80}")
        print(f"[{datetime.now()}] 📥 Nhận polygon mới từ VMS")
        
        if 'polygons' in data and data['polygons']:
            with polygons_lock:
                current_polygons = data['polygons']
            
            print(f"✓ Cập nhật {len(current_polygons)} polygon(s)")
            
            for polygon in current_polygons:
                print(f"  - {polygon['name']}: {len(polygon['points'])} points")
        else:
            print("⚠ Không có polygon")
            with polygons_lock:
                current_polygons = []
        
        print(f"{'='*80}\n")
        
    except Exception as e:
        print(f"❌ Error processing MQTT message: {e}")

def on_disconnect(client, userdata, rc):
    if rc != 0:
        print(f"[{datetime.now()}] ⚠️  Mất kết nối MQTT!")

def mqtt_thread():
    """Thread riêng cho MQTT client"""
    client = mqtt.Client(client_id=MQTT_CLIENT_ID)
    client.on_connect = on_connect
    client.on_message = on_message
    client.on_disconnect = on_disconnect
    
    try:
        print(f"🔄 Đang kết nối MQTT: {MQTT_BROKER}:{MQTT_PORT}")
        client.connect(MQTT_BROKER, MQTT_PORT, 60)
        client.loop_forever()
    except Exception as e:
        print(f"❌ MQTT Error: {e}")

def draw_polygons_on_frame(frame, polygons):
    """Vẽ polygon lên frame"""
    h, w = frame.shape[:2]
    
    for polygon in polygons:
        if 'points' not in polygon or not polygon['points']:
            continue
        
        # Convert normalized coordinates (0-1) to pixel coordinates
        points = []
        for pt in polygon['points']:
            x = int(pt[0] * w)
            y = int(pt[1] * h)
            points.append([x, y])
        
        points_array = np.array(points, dtype=np.int32)
        
        # Parse color (hex to BGR)
        color_hex = polygon.get('color', '#e040fb')
        # Remove '#' and convert hex to RGB
        color_hex = color_hex.lstrip('#')
        r = int(color_hex[0:2], 16)
        g = int(color_hex[2:4], 16)
        b = int(color_hex[4:6], 16)
        color_bgr = (b, g, r)  # OpenCV uses BGR
        
        # Vẽ polygon (filled with transparency)
        overlay = frame.copy()
        cv2.fillPoly(overlay, [points_array], color_bgr)
        cv2.addWeighted(overlay, 0.3, frame, 0.7, 0, frame)
        
        # Vẽ đường viền
        cv2.polylines(frame, [points_array], isClosed=True, 
                     color=color_bgr, thickness=3)
        
        # Vẽ các điểm vertices
        for i, pt in enumerate(points):
            cv2.circle(frame, tuple(pt), 6, (0, 255, 255), -1)
            cv2.putText(frame, f"P{i+1}", (pt[0]+10, pt[1]-10),
                       cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 2)
        
        # Hiển thị tên polygon
        name = polygon.get('name', 'Unknown')
        cv2.putText(frame, name, (points[0][0], points[0][1]-25),
                   cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 255), 2)
    
    return frame

def main():
    global frame_count
    
    print("="*80)
    print("🎨 POLYGON VISUALIZER - RTSP + MQTT (Headless Mode)")
    print("="*80)
    print(f"MQTT: {MQTT_BROKER}:{MQTT_PORT}")
    print(f"RTSP: {RTSP_URL}")
    print("="*80)
    
    # Start MQTT thread
    mqtt_worker = threading.Thread(target=mqtt_thread, daemon=True)
    mqtt_worker.start()
    
    # Wait for MQTT to connect
    print("\n⏳ Đợi MQTT kết nối...")
    time.sleep(2)
    
    # Connect to RTSP stream
    print(f"\n📹 Đang kết nối RTSP stream...")
    cap = cv2.VideoCapture(RTSP_URL)
    
    if not cap.isOpened():
        print("❌ Không thể kết nối RTSP stream!")
        print("Kiểm tra:")
        print("  - IP camera có đúng không?")
        print("  - Username/password đúng chưa?")
        print("  - Mạng có kết nối được không?")
        return
    
    print("✅ Kết nối RTSP thành công!")
    
    # Get video properties
    width = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    height = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    fps = int(cap.get(cv2.CAP_PROP_FPS))
    
    print(f"\n📺 Stream info:")
    print(f"   Resolution: {width}x{height}")
    print(f"   FPS: {fps}")
    print(f"\n🎮 Mode: Headless (auto-save snapshots)")
    print("   Press Ctrl+C to quit")
    print("\n")
    
    last_save_time = time.time()
    save_interval = 5  # Lưu ảnh mỗi 5 giây nếu có polygon
    
    try:
        while True:
            ret, frame = cap.read()
            
            if not ret:
                print("⚠️  Không đọc được frame, thử reconnect...")
                cap.release()
                time.sleep(1)
                cap = cv2.VideoCapture(RTSP_URL)
                continue
            
            frame_count += 1
            
            # Vẽ polygon nếu có
            has_polygons = False
            with polygons_lock:
                if current_polygons:
                    frame = draw_polygons_on_frame(frame, current_polygons)
                    has_polygons = True
            
            # Hiển thị info
            info_text = f"Frame: {frame_count} | Polygons: {len(current_polygons)}"
            cv2.putText(frame, info_text, (10, 30),
                       cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
            
            timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
            cv2.putText(frame, timestamp, (10, height-10),
                       cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 1)
            
            # Auto-save nếu có polygon và đủ interval
            current_time = time.time()
            if has_polygons and (current_time - last_save_time) >= save_interval:
                filename = f"polygon_frame_{datetime.now().strftime('%Y%m%d_%H%M%S')}.jpg"
                cv2.imwrite(filename, frame)
                print(f"📸 Tự động lưu: {filename}")
                last_save_time = current_time
            
            # Chỉ sleep ngắn để không lag
            time.sleep(0.03)  # ~30 FPS
            
    except KeyboardInterrupt:
        print("\n👋 Đang dừng...")
    
    # Lưu frame cuối cùng
    if frame_count > 0:
        final_filename = f"polygon_final_{datetime.now().strftime('%Y%m%d_%H%M%S')}.jpg"
        cv2.imwrite(final_filename, frame)
        print(f"� Lưu frame cuối: {final_filename}")
    
    # Cleanup
    cap.release()
    print("✅ Đã dừng!")

if __name__ == "__main__":
    main()
