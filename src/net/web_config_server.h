#pragma once

#include "Particle.h"
#include "softap_http.h"

#include "../app/command.h"
#include "../core/device_state.h"
#include "../core/settings_store.h"

class WebConfigServer {
public:
    typedef bool (*CommandSink)(const Command& cmd, void* ctx);
    typedef bool (*CommandBatchSink)(const Command* cmds, size_t count, void* ctx);

    explicit WebConfigServer(int port);

    void init(SettingsV2* settings, SettingsStore* store, DeviceState* state);
    void setCommandSink(CommandSink sink, void* ctx);
    void setCommandBatchSink(CommandBatchSink sink);
    void begin();
    void tick();

    static void softApHandler(const char* url,
                              ResponseCallback* cb,
                              void* cbArg,
                              Reader* body,
                              Writer* result,
                              void* reserved);

private:
    TCPServer server_;
    SettingsV2* settings_;
    SettingsStore* store_;
    DeviceState* state_;
    CommandSink sink_;
    CommandBatchSink batch_sink_;
    void* sink_ctx_;

    void handleClient(TCPClient& client);
    void handleSoftApRequest(const char* url,
                             ResponseCallback* cb,
                             void* cbArg,
                             Reader* body,
                             Writer* result,
                             void* reserved);

    size_t urlDecode(const char* src, size_t src_len, char* dst, size_t dst_size) const;
    bool getParam(const char* data, const char* key, char* out, size_t out_size) const;

    void respond(TCPClient& client, int status, const char* content_type, const char* body);
    void respondN(TCPClient& client, int status, const char* content_type, const char* body, size_t body_len);
    void handleApiSettingsGet(TCPClient& client);
    void handleApiSettingsPost(TCPClient& client, const char* form_data);
    void handleApiStateGet(TCPClient& client);
    void handleApiControlPost(TCPClient& client, const char* form_data);
    void handleApiSystemReboot(TCPClient& client);
    void handleApiSystemDfu(TCPClient& client);
    bool pushCommand(CommandType type, int value, CommandSource source);
};
