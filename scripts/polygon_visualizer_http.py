#!/usr/bin/env python3
"""
Polygon visualizer with HTTP request/response
Displays RTSP stream with polygon overlay fetched via HTTP
"""

import cv2
import numpy as np
import requests
import json
import threading
import time
from datetime import datetime

# Configuration
RTSP_URL = "rtsp://192.168.1.215:7001/4af76c5ac6f541b0ae9ffbd4d56b51c3"  # Camera 1
HTTP_SERVER = "http://localhost:8090"
CAMERA_ID = "{8c8b693d-038f-53a6-518f-7a84131b023d}"  # Camera 1

# Global variables
current_polygons = []
polygons_lock = threading.Lock()

def fetch_polygons():
    """Fetch polygon data from HTTP server"""
    try:
        url = f"{HTTP_SERVER}/polygon"
        params = {"camera_id": CAMERA_ID}
        
        response = requests.get(url, params=params, timeout=3)
        
        if response.status_code == 200:
            data = response.json()
            
            if "polygons" in data and len(data["polygons"]) > 0:
                print(f"[{datetime.now().strftime('%H:%M:%S')}] Received {len(data['polygons'])} polygon(s)")
                return data["polygons"]
            else:
                print(f"[{datetime.now().strftime('%H:%M:%S')}] No polygons available")
                return []
        else:
            print(f"[{datetime.now().strftime('%H:%M:%S')}] HTTP error: {response.status_code}")
            return []
            
    except Exception as e:
        print(f"[{datetime.now().strftime('%H:%M:%S')}] HTTP request failed: {e}")
        return []

def polygon_fetch_thread():
    """Background thread to fetch polygons periodically"""
    global current_polygons
    
    while True:
        polygons = fetch_polygons()
        
        with polygons_lock:
            current_polygons = polygons
        
        time.sleep(3)  # Fetch every 3 seconds

def draw_polygons_on_frame(frame, polygons):
    """Draw polygons on video frame"""
    height, width = frame.shape[:2]
    
    for polygon in polygons:
        if "points" not in polygon:
            continue
            
        points = polygon["points"]
        if len(points) < 3:
            continue
        
        # Convert normalized coordinates to pixel coordinates
        pts = []
        for point in points:
            x = int(point[0] * width)
            y = int(point[1] * height)
            pts.append([x, y])
        
        pts_array = np.array([pts], dtype=np.int32)
        
        # Parse color
        color_str = polygon.get("color", "#00ff00")
        if color_str.startswith("#"):
            color_str = color_str[1:]
        
        try:
            r = int(color_str[0:2], 16)
            g = int(color_str[2:4], 16)
            b = int(color_str[4:6], 16)
            color = (b, g, r)  # OpenCV uses BGR
        except:
            color = (0, 255, 0)  # Default green
        
        # Draw polygon
        cv2.polylines(frame, pts_array, isClosed=True, color=color, thickness=2)
        cv2.fillPoly(frame, pts_array, color=(*color[:3], 64))  # Semi-transparent fill
        
        # Draw polygon name
        polygon_name = polygon.get("name", "polygon")
        if pts:
            text_pos = tuple(pts[0])
            cv2.putText(frame, polygon_name, text_pos, cv2.FONT_HERSHEY_SIMPLEX, 
                       0.5, color, 2)
    
    return frame

def main():
    print(f"Starting polygon visualizer...")
    print(f"RTSP URL: {RTSP_URL}")
    print(f"HTTP Server: {HTTP_SERVER}")
    print(f"Camera ID: {CAMERA_ID}")
    print(f"\nFetching polygons every 3 seconds...")
    print(f"Press 'q' to quit\n")
    
    # Start polygon fetch thread
    fetch_thread = threading.Thread(target=polygon_fetch_thread, daemon=True)
    fetch_thread.start()
    
    # Open RTSP stream
    cap = cv2.VideoCapture(RTSP_URL)
    
    if not cap.isOpened():
        print("Error: Cannot open RTSP stream")
        return
    
    print("✓ RTSP stream opened successfully\n")
    
    while True:
        ret, frame = cap.read()
        
        if not ret:
            print("Error: Cannot read frame")
            break
        
        # Get current polygons safely
        with polygons_lock:
            polygons_to_draw = current_polygons.copy()
        
        # Draw polygons on frame
        if polygons_to_draw:
            frame = draw_polygons_on_frame(frame, polygons_to_draw)
        
        # Display frame
        cv2.imshow("Polygon Visualizer (HTTP)", frame)
        
        # Check for quit
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break
    
    cap.release()
    cv2.destroyAllWindows()
    print("\nVisualizer stopped")

if __name__ == "__main__":
    main()
