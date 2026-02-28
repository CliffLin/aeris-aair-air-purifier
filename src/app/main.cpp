#include "app_controller.h"
#include "softap_http.h"

SYSTEM_MODE(SEMI_AUTOMATIC);
SYSTEM_THREAD(ENABLED);

namespace {
AppController g_app;
}

STARTUP(System.set(SYSTEM_CONFIG_SOFTAP_PREFIX, "Aeris"));
STARTUP(softap_set_application_page_handler(WebConfigServer::softApHandler, appController().webServer()));

void setup() {
    g_app.init();
}

void loop() {
    g_app.tick();
}

AppController& appController() {
    return g_app;
}
