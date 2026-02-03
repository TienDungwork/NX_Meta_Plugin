# file: send_event.py
import json, requests, urllib3

BASE = "https://192.168.1.215:7001"
USER = "admin"
PASS = "Atin@123#"     # đổi nếu khác
CAMERA_ID = "729e9f41-9c81-f825-9355-73b5b702ca4e"         # điền ID nếu muốn gán event vào camera, ví dụ "2b4f1a2e-..."

# 1) tắt cảnh báo TLS (vì verify=False dùng cert self-signed)
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

# 2) login lấy token
r = requests.post(
    f"{BASE}/rest/v4/login/sessions",
    json={"username": USER, "password": PASS},
    verify=False
)
r.raise_for_status()
token = r.json()["token"]

# 3) gửi Generic Event
headers = {"Authorization": f"Bearer {token}", "Content-Type": "application/json"}
payload = {
    "source": "face",
    "caption": "",
    "description": ""
}
if CAMERA_ID:
    payload["metadata"] = {"cameraRefs": [CAMERA_ID]}

resp = requests.post(f"{BASE}/api/createEvent",
                     headers=headers,
                     data=json.dumps(payload),
                     verify=False)
print("createEvent ->", resp.status_code, resp.text)
