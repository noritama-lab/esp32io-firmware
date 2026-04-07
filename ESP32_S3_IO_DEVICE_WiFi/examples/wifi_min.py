"""Wi-Fi minimal example for ESP32 IO."""

from __future__ import annotations

import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from esp32io.client import ESP32IOWiFi


def main() -> None:
    # 必要に応じてIPを変更
    client = ESP32IOWiFi(host="172.20.10.14", port=80, timeout=3.0)

    # 最小疎通
    print("ping:", client.ping())

    # 状態取得
    state = client.get_io_state()
    print("dio_in:", state["dio_in"])
    print("adc:", state["adc"])


if __name__ == "__main__":
    main()
