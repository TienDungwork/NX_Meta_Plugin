import json, requests, urllib3

BASE = "https://192.168.1.215:7001"
USER = "atin"
PASS = "Atin@123#"
ENDPOINT = "/rest/v4/devices/"
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)
r = requests.post(
    f"{BASE}/rest/v2/login/sessions",
    json={"username": USER, "password": PASS},
    verify=False
)
r.raise_for_status()
token = r.json()["token"]
headers = {"Authorization": f"Bearer {token}", "Content-Type": "application/json"}
payload = {
    "source": "AI_Face",
    "caption": "smoke",
    "description": "Zone A"
}
def fetch_all_devices():
    url = f"{BASE}{ENDPOINT}"
    r = requests.get(url, headers=headers, timeout=15, verify=False)
    r.raise_for_status()
    return r.json()

def extract_camera_ids(devices):
    result = []
    for d in devices:
        cam_id = d.get("id")
        name = d.get("name")
        status = d.get("status")
        url = d.get("url")
        stream_urls = d.get("streamUrls") or (d.get("parameters", {}) or {}).get("streamUrls")

        result.append({
            "id": cam_id,
            "name": name,
            "status": status,
            "url": url,
            "streamUrls": stream_urls,
        })
    return result

if __name__ == "__main__":
    devices = fetch_all_devices()
    cams = extract_camera_ids(devices)

    for c in cams:
        print(f"{c['id']} | {c['name']} | {c['status']} | {c['url']}")
        if c["streamUrls"]:
            print(f"  streamUrls: {c['streamUrls']}")