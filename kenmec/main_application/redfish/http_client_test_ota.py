#!/usr/bin/env python3

"""
Replicates:
curl -k -u admin:admin123 \
  -X POST http://127.0.0.1:8080/UpdateFirmwareMultipart \
  -H "Content-Type: multipart/form-data" \
  -F "UpdateFile=@OTA_TEST_FIRMWARE1.swu;type=application/octet-stream" \
  -F "UpdateParameters={\"@Redfish.OperationApplyTime\":\"Immediate\"};type=application/json"
"""

import argparse
import json
import os
import sys
import time
import requests
from requests.auth import HTTPBasicAuth
try:
    from requests_toolbelt.multipart.encoder import MultipartEncoder
except Exception:
    MultipartEncoder = None


def main():
    parser = argparse.ArgumentParser(description="Redfish multipart upload (file + UpdateParameters JSON)")
    parser.add_argument("firmware_file", help="Path to firmware file (e.g., OTA_TEST_FIRMWARE1.swu)")
    parser.add_argument("--server", "-s", default="http://127.0.0.1:8080", help="Server base URL (default: http://127.0.0.1:8080)")
    parser.add_argument("--username", "-u", default="admin", help="Basic auth username (default: admin)")
    parser.add_argument("--password", "-p", default="admin123", help="Basic auth password (default: admin123)")
    parser.add_argument("--apply-time", default="Immediate", help="@Redfish.OperationApplyTime value (default: Immediate)")
    args = parser.parse_args()

    if not os.path.exists(args.firmware_file):
        print(f"File not found: {args.firmware_file}")
        sys.exit(1)

    url = args.server.rstrip("/") + "/UpdateFirmwareMultipart"

    params_obj = {"@Redfish.OperationApplyTime": args.apply_time}
    params_json = json.dumps(params_obj, separators=(",", ":"))

    # Use a session, disable env proxies, and force Connection: close to avoid keep-alive stalls
    session = requests.Session()
    session.trust_env = False

    headers = {"Connection": "close"}

    class ProgressFile:
        def __init__(self, f, total):
            self.f = f
            self.total = total
            self.sent = 0
            self.t0 = time.time()
            self.last = self.t0
        def read(self, amt=None):
            chunk = self.f.read(amt)
            if chunk:
                self.sent += len(chunk)
                now = time.time()
                if now - self.last >= 0.5:
                    pct = (self.sent / self.total) * 100 if self.total else 0
                    rate = self.sent / max(1e-6, (now - self.t0)) / (1024*1024)
                    print(f"\rUploading: {self.sent:,}/{self.total:,} bytes ({pct:5.1f}%)  {rate:6.2f} MB/s", end="", flush=True)
                    self.last = now
            return chunk
        def __getattr__(self, name):
            return getattr(self.f, name)

    # Prefer a pre-encoded multipart with explicit Content-Length to avoid chunked TE
    if MultipartEncoder is not None:
        with open(args.firmware_file, "rb") as base:
            m = MultipartEncoder(
                fields={
                    "UpdateFile": (os.path.basename(args.firmware_file), base, "application/octet-stream"),
                    "UpdateParameters": (None, params_json, "application/json"),
                }
            )
            headers["Content-Type"] = m.content_type
            resp = session.post(
                url,
                data=m,
                headers=headers,
                auth=HTTPBasicAuth(args.username, args.password),
                timeout=(5, 300),
                allow_redirects=False,
            )
    else:
        file_size = os.path.getsize(args.firmware_file)
        with open(args.firmware_file, "rb") as base:
            pf = ProgressFile(base, file_size)
            files = {
                "UpdateFile": (os.path.basename(args.firmware_file), pf, "application/octet-stream"),
                "UpdateParameters": (None, params_json, "application/json"),
            }
            resp = session.post(
                url,
                files=files,
                headers=headers,
                auth=HTTPBasicAuth(args.username, args.password),
                timeout=(5, 300),
                allow_redirects=False,
            )
            print()  # newline after progress

    print(f"HTTP {resp.status_code} {resp.reason}")
    for k, v in resp.headers.items():
        print(f"{k}: {v}")
    print()
    print(resp.text)


if __name__ == "__main__":
    main()


