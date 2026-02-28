#include "web_config_server.h"

#include "../util/string_safety.h"

#include <string.h>

namespace {
WebConfigServer* g_softap_fallback_ctx = nullptr;
}  // namespace

WebConfigServer::WebConfigServer(int port)
    : server_(port),
      settings_(nullptr),
      store_(nullptr),
      state_(nullptr),
      sink_(nullptr),
      batch_sink_(nullptr),
      sink_ctx_(nullptr) {
    g_softap_fallback_ctx = this;
}

void WebConfigServer::init(SettingsV2* settings, SettingsStore* store, DeviceState* state) {
    settings_ = settings;
    store_ = store;
    state_ = state;
}

void WebConfigServer::setCommandSink(CommandSink sink, void* ctx) {
    sink_ = sink;
    sink_ctx_ = ctx;
}

void WebConfigServer::setCommandBatchSink(CommandBatchSink sink) {
    batch_sink_ = sink;
}

void WebConfigServer::begin() {
    server_.begin();
}

void WebConfigServer::tick() {
    TCPClient client = server_.available();
    if (!client) {
        return;
    }
    handleClient(client);
    client.stop();
}

void WebConfigServer::softApHandler(const char* url,
                                    ResponseCallback* cb,
                                    void* cbArg,
                                    Reader* body,
                                    Writer* result,
                                    void* reserved) {
    WebConfigServer* server = static_cast<WebConfigServer*>(reserved);
    if (server == nullptr) {
        server = g_softap_fallback_ctx;
    }
    if (server == nullptr) {
        cb(cbArg, 0, 500, "text/plain", nullptr);
        result->write("server_not_ready");
        return;
    }

    server->handleSoftApRequest(url, cb, cbArg, body, result, reserved);
}

void WebConfigServer::handleSoftApRequest(const char* url,
                                          ResponseCallback* cb,
                                          void* cbArg,
                                          Reader* body,
                                          Writer* result,
                                          void* reserved) {
    (void) body;
    (void) reserved;

    if (settings_ == nullptr || store_ == nullptr) {
        cb(cbArg, 0, 500, "text/plain", nullptr);
        result->write("server_not_ready");
        return;
    }

    const char* req = (url == nullptr) ? "" : url;
    if (strncmp(req, "/save", 5) == 0) {
        char ssid[sizeof(settings_->wifi_ssid)] = {0};
        char pass[sizeof(settings_->wifi_pass)] = {0};
        const char* form_data = req;
        const char* query = strchr(req, '?');
        if (query != nullptr && *(query + 1) != '\0') {
            form_data = query + 1;
        }

        bool has_ssid = getParam(form_data, "s", ssid, sizeof(ssid));
        getParam(form_data, "p", pass, sizeof(pass));

        if (!has_ssid || ssid[0] == '\0') {
            cb(cbArg, 0, 400, "text/plain", nullptr);
            result->write("missing_ssid");
            return;
        }

        safeCopy(settings_->wifi_ssid, sizeof(settings_->wifi_ssid), ssid);
        safeCopy(settings_->wifi_pass, sizeof(settings_->wifi_pass), pass);
        store_->sanitize(*settings_);
        store_->save(*settings_);

        pushCommand(CommandType::Reboot, 0, CommandSource::Web);

        cb(cbArg, 0, 200, "text/html", nullptr);
        result->write(
            "<!doctype html><html><head><meta charset='utf-8'>"
            "<meta name='viewport' content='width=device-width, initial-scale=1'>"
            "<style>"
            "body{margin:0;font-family:sans-serif;background:#f4f7ff;color:#122138;}"
            ".shell{min-height:100vh;display:flex;align-items:center;justify-content:center;padding:20px;box-sizing:border-box;}"
            ".card{width:min(92vw,420px);background:#fff;border:1px solid #d9e3f8;border-radius:16px;padding:20px;box-sizing:border-box;"
            "box-shadow:0 8px 24px rgba(19,33,58,.08);text-align:center;}"
            "h1{margin:0 0 10px;font-size:24px;}"
            "p{margin:0;color:#5c6f91;font-size:15px;line-height:1.5;}"
            "</style></head><body><main class='shell'><section class='card'>"
            "<h1>Saved</h1><p>Rebooting now. Reconnect to your home Wi-Fi in a moment.</p>"
            "</section></main></body></html>");
        return;
    }

    cb(cbArg, 0, 200, "text/html", nullptr);
    result->write(
        "<!doctype html><html><head><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<style>"
        "*{box-sizing:border-box;}"
        "body{margin:0;font-family:sans-serif;background:linear-gradient(160deg,#edf3ff,#f6fbff);color:#122138;}"
        ".shell{min-height:100vh;display:flex;align-items:center;justify-content:center;padding:16px;}"
        ".card{width:min(94vw,430px);background:#fff;border:1px solid #d9e3f8;border-radius:18px;padding:20px;"
        "box-shadow:0 10px 28px rgba(20,36,62,.10);}"
        "h2{margin:0 0 8px;font-size:24px;line-height:1.2;}"
        ".hint{margin:0 0 14px;color:#5c6f91;font-size:14px;line-height:1.45;}"
        "label{display:block;margin:10px 0 6px;text-align:left;font-size:13px;color:#445979;font-weight:600;}"
        "input{width:100%;height:46px;padding:11px 12px;border:1px solid #cfdcf4;border-radius:12px;font-size:16px;"
        "background:#fdfefe;color:#122138;}"
        "input:focus{outline:none;border-color:#4d8cff;box-shadow:0 0 0 3px rgba(77,140,255,.18);}"
        "button{width:100%;height:48px;margin-top:14px;border:0;border-radius:12px;background:#216df2;color:#fff;"
        "font-size:16px;font-weight:700;}"
        "button:active{transform:translateY(1px);}"
        "@media (max-width:360px){.card{padding:16px;}h2{font-size:22px;}}"
        "</style></head><body><main class='shell'><section class='card'>"
        "<h2>Aeris Wi-Fi Setup</h2>"
        "<p class='hint'>Enter your home Wi-Fi credentials, then tap Connect.</p>"
        "<form action='/save' method='GET'>"
        "<label for='ssid'>Wi-Fi name (SSID)</label>"
        "<input id='ssid' name='s' placeholder='Wi-Fi SSID' autocomplete='off' required>"
        "<label for='pass'>Wi-Fi password</label>"
        "<input id='pass' name='p' placeholder='Wi-Fi password' type='password' autocomplete='off'>"
        "<button type='submit'>Connect</button>"
        "</form></section></main></body></html>");
}
