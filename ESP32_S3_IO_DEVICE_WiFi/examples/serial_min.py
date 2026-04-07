"""Serial minimal example for ESP32 IO."""

from __future__ import annotations

import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from esp32io.client import ESP32IO


def main() -> None:
    # 必要に応じてCOM番号を変更
    client = ESP32IO(port="COM4", baud=115200, timeout=2.0)

    try:
        # 最小疎通
        print("ping:", client.ping())

        # 状態取得
        state = client.get_io_state()
        print("dio_in:", state["dio_in"])
        print("adc:", state["adc"])
    finally:
        client.close()


if __name__ == "__main__":
    main()
