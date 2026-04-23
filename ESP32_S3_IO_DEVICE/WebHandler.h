#ifndef WEB_HANDLER_H
#define WEB_HANDLER_H

#include <WebServer.h>
#include "Config.h"

class WebHandler {
public:
    void begin();
    void handle();
    bool shouldRestart() const { return _pendingRestart; }

private:
    void handleRoot();
    void handleSave();
    void handleApi();
    String makeConfigPage();
    bool _pendingRestart = false;
};

extern WebHandler Web;
#endif