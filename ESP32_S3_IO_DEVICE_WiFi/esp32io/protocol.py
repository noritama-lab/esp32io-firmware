"""ESP32IO の JSON プロトコル定義。"""

CMD_PING = "ping"
CMD_READ_DI = "read_di"
CMD_SET_DO = "set_do"
CMD_READ_ADC = "read_adc"
CMD_SET_PWM = "set_pwm"
CMD_GET_IO_STATE = "get_io_state"
CMD_GET_PWM_CONFIG = "get_pwm_config"
CMD_SET_PWM_CONFIG = "set_pwm_config"

ALL_COMMANDS = (
    CMD_PING,
    CMD_READ_DI,
    CMD_SET_DO,
    CMD_READ_ADC,
    CMD_SET_PWM,
    CMD_GET_IO_STATE,
    CMD_GET_PWM_CONFIG,
    CMD_SET_PWM_CONFIG,
)

__all__ = [
    "CMD_PING",
    "CMD_READ_DI",
    "CMD_SET_DO",
    "CMD_READ_ADC",
    "CMD_SET_PWM",
    "CMD_GET_IO_STATE",
    "CMD_GET_PWM_CONFIG",
    "CMD_SET_PWM_CONFIG",
    "ALL_COMMANDS",
]