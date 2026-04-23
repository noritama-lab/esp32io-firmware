#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <ArduinoJson.h>
#include "HardwareManager.h"

class CommandHandler {
public:
    static void process(JsonVariantConst req, JsonDocument& res);
    static void buildError(JsonDocument& res, const char* cmd, const char* code, const char* detail);

private:
    static bool checkRange(int id, int max, JsonDocument& res, const char* cmd);
};
#endif