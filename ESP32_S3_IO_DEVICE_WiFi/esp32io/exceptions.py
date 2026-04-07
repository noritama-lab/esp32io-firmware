class ESP32IOError(Exception):
    """ESP32 が status=error を返したときの例外"""
    pass

class ESP32IOTimeoutError(TimeoutError):
    """ESP32 からの応答がタイムアウトしたときの例外"""
    pass

class ESP32IOProtocolError(RuntimeError):
    """ESP32 から不正な JSON や予期しない応答が返ったときの例外"""
    pass
