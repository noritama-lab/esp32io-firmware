import serial
import json
import time
from typing import Any, Dict, Optional
import urllib.error
import urllib.request

from .exceptions import (
    ESP32IOError,
    ESP32IOTimeoutError,
    ESP32IOProtocolError,
)

class ESP32IO:
    """
    ESP32 と JSON ベースのシリアルプロトコルで通信するクライアント。
    コマンド（read_di, set_do, read_adc, set_pwm, get_pwm_config,
    set_pwm_config, ping, get_io_state）をメソッドとして提供。
    """

    def __init__(
        self,
        port: str,
        baud: int = 115200,
        timeout: float = 2.0,
        debug: bool = False,
        recv_timeout: Optional[float] = None,
    ) -> None:
        """
        :param port: 例: "COM5" や "/dev/ttyUSB0"
        :param baud: ボーレート (デフォルト: 115200)
        :param timeout: pyserial の read タイムアウト秒数
        :param debug: True にすると送受信ログを表示
        :param recv_timeout: _safe_recv 内での最大待ち時間（秒）。
                             None の場合は無制限ループ（pyserial の timeout に依存）
        """
        self.debug = debug
        # DTR=False でオープンして ESP32 のオートリセットを防ぐ
        self.ser = serial.Serial()
        self.ser.port = port
        self.ser.baudrate = baud
        self.ser.timeout = timeout
        self.ser.dtr = False
        self.ser.rts = False
        self.ser.open()
        self.recv_timeout = recv_timeout

        # リセットが起きないので短い待機で OK
        time.sleep(0.3)
        self.ser.reset_input_buffer()

        # ウォームアップ: ping を投げて応答を捨てる（失敗しても無視）
        try:
            self._send({"cmd": "ping"})
            self._safe_recv()
        except Exception:
            pass

    # ------------------------------
    # 内部ユーティリティ
    # ------------------------------
    def _log(self, *args: Any) -> None:
        if self.debug:
            print("[ESP32IO]", *args)

    def _send(self, obj: Dict[str, Any]) -> None:
        """JSON オブジェクトを 1 行の JSON として送信する"""
        line = json.dumps(obj, separators=(",", ":")) + "\n"
        self._log("SEND:", line.strip())
        data = line.encode("utf-8")

        total = 0
        while total < len(data):
            written = self.ser.write(data[total:])
            if written == 0:
                raise ESP32IOProtocolError("Failed to write to serial port")
            total += written

    def _safe_recv(self) -> Dict[str, Any]:
        """
        空行を無視し、JSON を返すまで待つ。
        - status=error の場合は ESP32IOError を送出
        - recv_timeout を超えた場合は ESP32IOTimeoutError を送出
        - 不正 JSON の場合は ESP32IOProtocolError を送出
        """
        start = time.perf_counter()

        while True:
            if self.recv_timeout is not None:
                if (time.perf_counter() - start) > self.recv_timeout:
                    raise ESP32IOTimeoutError("Timeout waiting for response from ESP32")

            line_bytes = self.ser.readline()
            if not line_bytes:
                # pyserial の timeout による空読み。recv_timeout が None なら継続。
                if self.recv_timeout is None:
                    continue
                # recv_timeout 管理は上の if でやっているのでここでは単に continue
                continue

            try:
                line = line_bytes.decode("utf-8", errors="replace").strip()
            except UnicodeDecodeError as e:
                raise ESP32IOProtocolError(f"Invalid UTF-8 from ESP32: {line_bytes!r}") from e

            if not line:
                # 空行は無視
                continue

            self._log("RECV:", line)

            # JSON パース
            try:
                data = json.loads(line)
            except json.JSONDecodeError as e:
                raise ESP32IOProtocolError(f"Invalid JSON from ESP32: {line}") from e

            # エラー応答なら例外
            if data.get("status") == "error":
                code = data.get("code", "UNKNOWN")
                detail = data.get("detail", "")
                raise ESP32IOError(f"{code}: {detail}")

            # 正常応答
            return data

    def command(self, cmd: str, **kwargs: Any) -> Dict[str, Any]:
        """
        任意のコマンドを送信し、JSON 応答を返す低レベル API。

        :param cmd: "read_di" など
        :param kwargs: 追加パラメータ
        :return: ESP32 からの JSON 応答
        """
        req: Dict[str, Any] = {"cmd": cmd}
        req.update(kwargs)
        self._send(req)
        return self._safe_recv()

    # ------------------------------
    # 基本コマンド（公開 API）
    # ------------------------------
    def read_di(self, pin_id: int) -> int:
        """
        デジタル入力を読む。

        :param pin_id: ピン番号（ESP32 側の定義に依存）
        :return: 0 or 1
        """
        res = self.command("read_di", pin_id=pin_id)
        value = res.get("value")
        if not isinstance(value, int):
            raise ESP32IOProtocolError(f"Invalid read_di response: {res}")
        return value

    def set_do(self, pin_id: int, value: int) -> Dict[str, Any]:
        """
        デジタル出力を書き込む。

        :param pin_id: ピン番号
        :param value: 0 or 1
        :return: ESP32 の生 JSON 応答
        """
        return self.command("set_do", pin_id=pin_id, value=value)

    def read_adc(self, pin_id: int) -> int:
        """
        ADC を読む。

        :param pin_id: ピン番号
        :return: ADC 値（0〜4095 など、ESP32 側の仕様に依存）
        """
        res = self.command("read_adc", pin_id=pin_id)
        value = res.get("value")
        if not isinstance(value, int):
            raise ESP32IOProtocolError(f"Invalid read_adc response: {res}")
        return value

    def set_pwm(self, pin_id: int, duty: int) -> Dict[str, Any]:
        """
        PWM デューティを設定する。

        :param pin_id: ピン番号
        :param duty: デューティ値（0〜255 など、ESP32 側の仕様に依存）
        :return: ESP32 の生 JSON 応答
        """
        return self.command("set_pwm", pin_id=pin_id, duty=duty)

    def get_pwm_config(self) -> Dict[str, int]:
        """
        PWM の周波数と分解能を取得する。

        :return: {"freq": 周波数, "res": 分解能}
        """
        response = self.command("get_pwm_config")
        freq = response.get("freq")
        resolution = response.get("res")
        if not isinstance(freq, int) or not isinstance(resolution, int):
            raise ESP32IOProtocolError(f"Invalid get_pwm_config response: {response}")
        return {"freq": freq, "res": resolution}

    def set_pwm_config(self, freq: int, res: int) -> Dict[str, int]:
        """
        PWM の周波数と分解能を設定する。

        :param freq: 周波数。ファームウェア仕様では 1〜20000。
        :param res: 分解能ビット数。ファームウェア仕様では 1〜16。
        :return: {"freq": 周波数, "res": 分解能}
        """
        response = self.command("set_pwm_config", freq=freq, res=res)
        actual_freq = response.get("freq")
        actual_res = response.get("res")
        if not isinstance(actual_freq, int) or not isinstance(actual_res, int):
            raise ESP32IOProtocolError(f"Invalid set_pwm_config response: {response}")
        return {"freq": actual_freq, "res": actual_res}

    def ping(self) -> bool:
        """
        ESP32 との疎通確認を行う。

        :return: pong 応答を受け取れた場合は True
        """
        res = self.command("ping")
        if res.get("message") != "pong":
            raise ESP32IOProtocolError(f"Invalid ping response: {res}")
        return True
    
    # ------------------------------
    # 全データ取得コマンド（公開 API）
    # ------------------------------
    def get_io_state(self):
        """
        ESP32 の全 I/O 状態をまとめて取得する。
        戻り値:
            dio_in  : list[int] (len=6)
            dio_out : list[int] (len=6)
            adc     : list[int] (len=2)
            pwm     : list[int] (len=2)
        """
        res = self.command("get_io_state")

        return {
            "dio_in":  list(res["dio_in"]),
            "dio_out": list(res["dio_out"]),
            "adc":     list(res["adc"]),
            "pwm":     list(res["pwm"]),
        }

    # ------------------------------
    # 後片付け
    # ------------------------------
    def close(self) -> None:
        """シリアルポートを閉じる"""
        if self.ser and self.ser.is_open:
            self.ser.close()

    def __del__(self) -> None:
        try:
            self.close()
        except Exception:
            pass


class ESP32IOWiFi:
    """
    ESP32 と HTTP(JSON) で通信するクライアント。
    /api/cmd と各HTTP APIに対応。
    """

    def __init__(self, host: str, port: int = 80, timeout: float = 3.0, debug: bool = False) -> None:
        self.host = host
        self.port = port
        self.timeout = timeout
        self.debug = debug
        self.base_url = f"http://{host}:{port}"

    def _log(self, *args: Any) -> None:
        if self.debug:
            print("[ESP32IOWiFi]", *args)

    def _request_json(self, method: str, path: str, payload: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
        url = f"{self.base_url}{path}"
        data = None
        headers = {}

        if payload is not None:
            data = json.dumps(payload, separators=(",", ":")).encode("utf-8")
            headers["Content-Type"] = "application/json"

        req = urllib.request.Request(url, data=data, headers=headers, method=method)
        self._log("REQ:", method, url, payload if payload is not None else "")

        try:
            with urllib.request.urlopen(req, timeout=self.timeout) as resp:
                body = resp.read().decode("utf-8", errors="replace")
        except urllib.error.URLError as e:
            raise ESP32IOTimeoutError(f"HTTP request failed: {e}") from e

        if not body:
            parsed: Dict[str, Any] = {}
        else:
            try:
                parsed = json.loads(body)
            except json.JSONDecodeError as e:
                raise ESP32IOProtocolError(f"Invalid JSON from ESP32 HTTP API: {body}") from e

        if isinstance(parsed, dict) and parsed.get("status") == "error":
            code = parsed.get("code", "UNKNOWN")
            detail = parsed.get("detail", "")
            raise ESP32IOError(f"{code}: {detail}")

        self._log("RES:", parsed)
        return parsed

    # ------------------------------
    # 汎用コマンド
    # ------------------------------
    def command(self, cmd: str, **kwargs: Any) -> Dict[str, Any]:
        payload: Dict[str, Any] = {"cmd": cmd}
        payload.update(kwargs)
        return self._request_json("POST", "/api/cmd", payload)

    # ------------------------------
    # /api/cmd 経由コマンド
    # ------------------------------
    def ping(self) -> bool:
        res = self.command("ping")
        if res.get("message") != "pong":
            raise ESP32IOProtocolError(f"Invalid ping response: {res}")
        return True

    def help(self) -> Dict[str, Any]:
        return self.command("help")

    def get_status(self) -> Dict[str, Any]:
        return self.command("get_status")

    def get_io_state(self) -> Dict[str, Any]:
        res = self.command("get_io_state")
        return {
            "dio_in": list(res["dio_in"]),
            "dio_out": list(res["dio_out"]),
            "adc": list(res["adc"]),
            "pwm": list(res["pwm"]),
        }

    def read_di(self, pin_id: int) -> int:
        res = self.command("read_di", pin_id=pin_id)
        value = res.get("value")
        if not isinstance(value, int):
            raise ESP32IOProtocolError(f"Invalid read_di response: {res}")
        return value

    def set_do(self, pin_id: int, value: int) -> Dict[str, Any]:
        return self.command("set_do", pin_id=pin_id, value=value)

    def read_adc(self, pin_id: int) -> int:
        res = self.command("read_adc", pin_id=pin_id)
        value = res.get("value")
        if not isinstance(value, int):
            raise ESP32IOProtocolError(f"Invalid read_adc response: {res}")
        return value

    def set_pwm(self, pin_id: int, duty: int) -> Dict[str, Any]:
        return self.command("set_pwm", pin_id=pin_id, duty=duty)

    def get_pwm_config(self) -> Dict[str, int]:
        response = self.command("get_pwm_config")
        freq = response.get("freq")
        resolution = response.get("res")
        if not isinstance(freq, int) or not isinstance(resolution, int):
            raise ESP32IOProtocolError(f"Invalid get_pwm_config response: {response}")
        return {"freq": freq, "res": resolution}

    def set_pwm_config(self, freq: int, res: int) -> Dict[str, int]:
        response = self.command("set_pwm_config", freq=freq, res=res)
        actual_freq = response.get("freq")
        actual_res = response.get("res")
        if not isinstance(actual_freq, int) or not isinstance(actual_res, int):
            raise ESP32IOProtocolError(f"Invalid set_pwm_config response: {response}")
        return {"freq": actual_freq, "res": actual_res}