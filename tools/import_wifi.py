"""Generate ignored local provisioning files; never print Wi-Fi credentials."""
import argparse
import json
from pathlib import Path


def read_credentials(path):
    lines = path.read_text(encoding="utf-8-sig").splitlines()
    if len(lines) != 2:
        raise ValueError("WIFI.txt must contain exactly two lines: SSID, then password")
    ssid, password = lines
    if not 1 <= len(ssid.encode("utf-8")) <= 32 or "\0" in ssid:
        raise ValueError("SSID must contain 1 to 32 UTF-8 bytes")
    size = len(password.encode("utf-8"))
    if "\0" in password or (size != 0 and not 8 <= size <= 64):
        raise ValueError("Password must be empty, 8 to 63 bytes, or a 64-digit hex PSK")
    if size == 64 and any(c not in "0123456789abcdefABCDEF" for c in password):
        raise ValueError("A 64-byte PSK must be hexadecimal")
    return ssid, password


def generate(source, directory):
    ssid, password = read_credentials(source)
    settings = {
        "network": {"wifi_enabled": True, "ssid": ssid, "password": password},
        "sync": {
            "mode": "ntp", "servers": ["ntp.aliyun.com", "ntp1.aliyun.com", "ntp2.aliyun.com"],
            "interval_seconds": 86400, "retry_seconds": 900,
            "connect_timeout_seconds": 15, "sync_timeout_seconds": 20,
        },
    }
    text = json.dumps(settings, ensure_ascii=False, separators=(",", ":"))
    directory.mkdir(parents=True, exist_ok=True)
    (directory / "wifi-provision.json").write_text(text + "\n", encoding="utf-8")
    # Escape the entire JSON document as a C string, including quotes and backslashes.
    (directory / "wifi_credentials.h").write_text(
        "#pragma once\n#define EPAPER_PRIVATE_WIFI_JSON " + json.dumps(text, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="Local two-line WIFI.txt")
    args = parser.parse_args()
    try:
        generate(args.source, Path(__file__).resolve().parents[1] / ".local")
    except (OSError, UnicodeError, ValueError) as error:
        # OS paths and validation messages contain no credential values.
        parser.exit(1, "Import failed: " + str(error) + "\n")
    print("Local provisioning files ready. Enable CONFIG_EPAPER_PROVISION_WIFI for this build.")
