#ifndef WEB_HANDLER_H
#define WEB_HANDLER_H

#include <WebServer.h>
#include "Config.h"

class WebHandler {
public:
    void begin();
    void handle();

private:
    void handleRoot();
    void handleSave();
    void handleApi();
    String makeConfigPage();
};

extern WebHandler Web;
#endif