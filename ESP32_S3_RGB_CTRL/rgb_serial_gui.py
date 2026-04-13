from __future__ import annotations

import json
import sys
import threading
import time
from datetime import datetime

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    serial = None
    list_ports = None

from PySide6.QtCore import Qt, Signal
from PySide6.QtGui import QCloseEvent, QColor
from PySide6.QtWidgets import (
    QApplication,
    QComboBox,
    QColorDialog,
    QGridLayout,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QMainWindow,
    QMessageBox,
    QPlainTextEdit,
    QPushButton,
    QSpinBox,
    QVBoxLayout,
    QWidget,
)

DEFAULT_PORT = "COM4"
DEFAULT_BAUD = 115200
DEFAULT_TIMEOUT = 1.0
DEFAULT_HEX = "#FF0000"
DEFAULT_BRIGHTNESS = 64


def hex_to_rgb(hex_value: str) -> tuple[int, int, int]:
    text = hex_value.strip()
    if text.startswith("#"):
        text = text[1:]

    if len(text) != 6 or any(ch not in "0123456789ABCDEFabcdef" for ch in text):
        raise ValueError("カラーコードは #RRGGBB 形式で入力してください。")

    return int(text[0:2], 16), int(text[2:4], 16), int(text[4:6], 16)


class Esp32RgbWindow(QMainWindow):
    log_message = Signal(str)
    error_message = Signal(str)

    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle("ESP32 RGB Serial Sender")
        self.resize(720, 540)

        self.serial_conn = None
        self.serial_lock = threading.Lock()

        self.port_combo = QComboBox()
        self.port_combo.setEditable(True)
        self.port_combo.setMinimumWidth(140)
        self.port_combo.setEditText(DEFAULT_PORT)

        self.baud_edit = QLineEdit(str(DEFAULT_BAUD))
        self.timeout_edit = QLineEdit(str(DEFAULT_TIMEOUT))
        self.hex_edit = QLineEdit(DEFAULT_HEX)

        self.brightness_spin = QSpinBox()
        self.brightness_spin.setRange(0, 255)
        self.brightness_spin.setValue(DEFAULT_BRIGHTNESS)

        self.connect_button = QPushButton("Connect")
        self.preview = QLabel(DEFAULT_HEX)
        self.log_text = QPlainTextEdit()

        self.log_message.connect(self.append_log)
        self.error_message.connect(self.show_error)

        self.build_ui()
        self.refresh_ports()
        self.hex_edit.textChanged.connect(self.update_preview)
        self.connect_button.clicked.connect(self.toggle_connection)
        self.update_preview()

        if serial is None:
            self.append_log("pyserial が見つかりません。`pip install pyserial` を実行してください。")

    def build_ui(self) -> None:
        central = QWidget()
        self.setCentralWidget(central)

        root = QVBoxLayout(central)

        serial_group = QGroupBox("Serial settings")
        serial_layout = QGridLayout(serial_group)
        serial_layout.addWidget(QLabel("COM port"), 0, 0)
        serial_layout.addWidget(QLabel("Baud"), 0, 1)
        serial_layout.addWidget(QLabel("Timeout (sec)"), 0, 2)
        serial_layout.addWidget(self.port_combo, 1, 0)
        serial_layout.addWidget(self.baud_edit, 1, 1)
        serial_layout.addWidget(self.timeout_edit, 1, 2)

        refresh_button = QPushButton("Refresh")
        refresh_button.clicked.connect(self.refresh_ports)
        serial_layout.addWidget(refresh_button, 1, 3)
        serial_layout.addWidget(self.connect_button, 1, 4)

        color_group = QGroupBox("Color sender")
        color_layout = QGridLayout(color_group)
        color_layout.addWidget(QLabel("Hex code"), 0, 0)
        color_layout.addWidget(QLabel("Brightness (0-255)"), 0, 2)
        color_layout.addWidget(self.hex_edit, 1, 0)

        choose_button = QPushButton("Choose...")
        choose_button.clicked.connect(self.choose_color)
        color_layout.addWidget(choose_button, 1, 1)
        color_layout.addWidget(self.brightness_spin, 1, 2)

        self.preview.setMinimumSize(150, 80)
        self.preview.setStyleSheet("border: 1px solid #999; border-radius: 4px; padding: 8px;")
        self.preview.setAlignment(Qt.AlignmentFlag.AlignCenter)
        color_layout.addWidget(self.preview, 0, 3, 2, 1)

        action_row = QHBoxLayout()
        send_button = QPushButton("Send color")
        send_button.clicked.connect(self.send_color)
        action_row.addWidget(send_button)

        led_off_button = QPushButton("LED off")
        led_off_button.clicked.connect(self.send_led_off)
        action_row.addWidget(led_off_button)

        ping_button = QPushButton("Ping")
        ping_button.clicked.connect(self.send_ping)
        action_row.addWidget(ping_button)
        action_row.addStretch(1)

        note = QLabel(
            'ESP32 へ JSON コマンドをシリアル送信します。例: {"cmd":"set_rgb","r":255,"g":0,"b":0}'
        )
        note.setStyleSheet("color: #555555;")

        log_group = QGroupBox("Log")
        log_layout = QVBoxLayout(log_group)
        self.log_text.setReadOnly(True)
        log_layout.addWidget(self.log_text)

        root.addWidget(serial_group)
        root.addWidget(color_group)
        root.addLayout(action_row)
        root.addWidget(note)
        root.addWidget(log_group)

    def refresh_ports(self) -> None:
        current_port = self.port_combo.currentText().strip()

        self.port_combo.blockSignals(True)
        self.port_combo.clear()

        if list_ports is not None:
            ports = [port.device for port in list_ports.comports()]
            self.port_combo.addItems(ports)
        else:
            ports = []

        if current_port:
            index = self.port_combo.findText(current_port)
            if index >= 0:
                self.port_combo.setCurrentIndex(index)
            else:
                self.port_combo.setEditText(current_port)
        elif ports:
            self.port_combo.setCurrentIndex(0)
        else:
            self.port_combo.setEditText(DEFAULT_PORT)

        self.port_combo.blockSignals(False)

    def toggle_connection(self) -> None:
        if self.serial_conn is not None and getattr(self.serial_conn, "is_open", False):
            self.disconnect_serial()
        else:
            self.connect_serial()

    def connect_serial(self) -> bool:
        if serial is None:
            QMessageBox.critical(self, "pyserial not found", "先に `pip install pyserial` を実行してください。")
            return False

        if self.serial_conn is not None and getattr(self.serial_conn, "is_open", False):
            return True

        port = self.port_combo.currentText().strip()
        if not port:
            QMessageBox.critical(self, "入力エラー", "COMポートを指定してください。")
            return False

        try:
            baud = int(self.baud_edit.text().strip())
            timeout = float(self.timeout_edit.text().strip())
        except ValueError:
            QMessageBox.critical(self, "入力エラー", "Baud と Timeout は数値で入力してください。")
            return False

        try:
            conn = serial.Serial()
            conn.port = port
            conn.baudrate = baud
            conn.timeout = timeout
            conn.write_timeout = timeout
            conn.rtscts = False
            conn.dsrdtr = False
            conn.dtr = False
            conn.rts = False
            conn.open()
            time.sleep(0.2)
            conn.reset_input_buffer()

            self.serial_conn = conn
            self.connect_button.setText("Disconnect")
            self.append_log(f"Connected: {port} @ {baud}")
            return True
        except Exception as exc:
            self.serial_conn = None
            self.append_log(f"Connect error: {exc}")
            QMessageBox.critical(self, "接続エラー", str(exc))
            return False

    def disconnect_serial(self) -> None:
        if self.serial_conn is not None:
            try:
                if self.serial_conn.is_open:
                    self.serial_conn.close()
            finally:
                self.serial_conn = None

        self.connect_button.setText("Connect")
        self.append_log("Disconnected")

    def choose_color(self) -> None:
        selected = QColorDialog.getColor(QColor(self.hex_edit.text()), self, "色を選択")
        if selected.isValid():
            self.hex_edit.setText(selected.name().upper())

    def update_preview(self) -> None:
        try:
            r, g, b = hex_to_rgb(self.hex_edit.text())
            color = f"#{r:02X}{g:02X}{b:02X}"
            luminance = (0.299 * r) + (0.587 * g) + (0.114 * b)
            text_color = "#000000" if luminance > 140 else "#FFFFFF"
            self.preview.setText(color)
            self.preview.setStyleSheet(
                f"background-color: {color}; color: {text_color}; border: 1px solid #999; border-radius: 4px; padding: 8px;"
            )
        except ValueError:
            self.preview.setText("INVALID")
            self.preview.setStyleSheet(
                "background-color: #A0A0A0; color: #000000; border: 1px solid #999; border-radius: 4px; padding: 8px;"
            )

    def send_color(self) -> None:
        try:
            r, g, b = hex_to_rgb(self.hex_edit.text())
        except ValueError as exc:
            QMessageBox.critical(self, "入力エラー", str(exc))
            return

        brightness = self.brightness_spin.value()
        payload = {
            "cmd": "set_rgb",
            "r": r,
            "g": g,
            "b": b,
            "brightness": brightness,
        }
        self.send_payload(payload)

    def send_led_off(self) -> None:
        self.send_payload({"cmd": "led_off"})

    def send_ping(self) -> None:
        self.send_payload({"cmd": "ping"})

    def send_payload(self, payload: dict) -> None:
        if not self.connect_serial():
            return

        worker = threading.Thread(target=self._send_payload_worker, args=(payload,), daemon=True)
        worker.start()

    def _send_payload_worker(self, payload: dict) -> None:
        text = json.dumps(payload, separators=(",", ":"))
        self.log_message.emit(f"SEND {text}")

        try:
            with self.serial_lock:
                if self.serial_conn is None or not self.serial_conn.is_open:
                    raise RuntimeError("COMポートが未接続です。")

                self.serial_conn.reset_input_buffer()
                self.serial_conn.write((text + "\n").encode("utf-8"))
                self.serial_conn.flush()
                response = self.serial_conn.readline().decode("utf-8", errors="replace").strip()

            if response:
                self.log_message.emit(f"RECV {response}")
            else:
                self.log_message.emit("RECV <no response>")
        except Exception as exc:
            self.log_message.emit(f"ERROR {exc}")
            self.error_message.emit(str(exc))

    def append_log(self, message: str) -> None:
        timestamp = datetime.now().strftime("%H:%M:%S")
        self.log_text.appendPlainText(f"[{timestamp}] {message}")

    def show_error(self, message: str) -> None:
        QMessageBox.critical(self, "送信エラー", message)

    def closeEvent(self, event: QCloseEvent) -> None:
        self.disconnect_serial()
        event.accept()


if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = Esp32RgbWindow()
    window.show()
    sys.exit(app.exec())
