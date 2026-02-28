#include "web_config_server.h"

#include "web_ui_html.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

namespace {
const uint32_t kReadTimeoutMs = 120;
const size_t kRawRequestMax = 1536;

const char* statusReason(int status) {
    if (status == 200) {
        return "OK";
    }
    if (status == 400) {
        return "Bad Request";
    }
    if (status == 401) {
        return "Unauthorized";
    }
    if (status == 403) {
        return "Forbidden";
    }
    if (status == 404) {
        return "Not Found";
    }
    if (status == 405) {
        return "Method Not Allowed";
    }
    if (status == 503) {
        return "Service Unavailable";
    }
    if (status == 500) {
        return "Internal Server Error";
    }
    return "Error";
}

int hexToInt(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}
}  // namespace

void WebConfigServer::handleClient(TCPClient& client) {
    char raw[kRawRequestMax];
    size_t raw_len = 0;
    raw[0] = '\0';

    uint32_t start = millis();
    while (millis() - start < kReadTimeoutMs && raw_len + 1 < sizeof(raw)) {
        while (client.available() && raw_len + 1 < sizeof(raw)) {
            raw[raw_len++] = static_cast<char>(client.read());
        }
        raw[raw_len] = '\0';
        if (strstr(raw, "\r\n\r\n") != nullptr) {
            break;
        }
    }

    char* header_end = strstr(raw, "\r\n\r\n");
    if (header_end == nullptr) {
        respond(client, 400, "text/plain", "incomplete_header");
        return;
    }

    char* line_end = strstr(raw, "\r\n");
    if (line_end == nullptr) {
        respond(client, 400, "text/plain", "bad_request");
        return;
    }
    *line_end = '\0';

    char* method = raw;
    char* sp1 = strchr(method, ' ');
    if (sp1 == nullptr) {
        respond(client, 400, "text/plain", "bad_request_line");
        return;
    }
    *sp1 = '\0';

    char* path = sp1 + 1;
    char* sp2 = strchr(path, ' ');
    if (sp2 == nullptr) {
        respond(client, 400, "text/plain", "bad_request_line");
        return;
    }
    *sp2 = '\0';

    char* query = nullptr;
    char* query_start = strchr(path, '?');
    if (query_start != nullptr) {
        *query_start = '\0';
        query = query_start + 1;
    }

    size_t content_length = 0;
    bool has_content_length = false;

    char* header_line = line_end + 2;
    while (header_line < header_end) {
        char* next = strstr(header_line, "\r\n");
        if (next == nullptr || next > header_end) {
            break;
        }

        *next = '\0';
        if (header_line[0] != '\0') {
            const char* key = "content-length:";
            const size_t key_len = 15;
            bool key_match = (strlen(header_line) >= key_len);
            if (key_match) {
                for (size_t i = 0; i < key_len; ++i) {
                    if (tolower(static_cast<unsigned char>(header_line[i])) != key[i]) {
                        key_match = false;
                        break;
                    }
                }
            }
            if (key_match) {
                const char* value = header_line + key_len;
                while (*value == ' ' || *value == '\t') {
                    ++value;
                }
                char* end_ptr = nullptr;
                unsigned long parsed = strtoul(value, &end_ptr, 10);
                if (end_ptr == value || *end_ptr != '\0') {
                    respond(client, 400, "text/plain", "bad_content_length");
                    return;
                }
                content_length = static_cast<size_t>(parsed);
                has_content_length = true;
            }
        }
        *next = '\r';
        header_line = next + 2;
    }

    size_t body_offset = static_cast<size_t>((header_end + 4) - raw);
    size_t body_available = (raw_len > body_offset) ? (raw_len - body_offset) : 0;

    if (strcmp(method, "POST") == 0 && has_content_length) {
        if (body_offset + content_length + 1 > sizeof(raw)) {
            respond(client, 400, "text/plain", "body_too_large");
            return;
        }
        while (body_available < content_length && millis() - start < kReadTimeoutMs && raw_len + 1 < sizeof(raw)) {
            while (client.available() && raw_len + 1 < sizeof(raw) && body_available < content_length) {
                raw[raw_len++] = static_cast<char>(client.read());
                body_available++;
            }
            raw[raw_len] = '\0';
        }
        if (body_available < content_length) {
            respond(client, 400, "text/plain", "incomplete_body");
            return;
        }
        raw[body_offset + content_length] = '\0';
    }

    const char* body = raw + body_offset;
    const char* form = (body[0] != '\0') ? body : ((query != nullptr) ? query : "");

    if (strcmp(method, "GET") == 0 && strcmp(path, "/api/v2/settings") == 0) {
        handleApiSettingsGet(client);
        return;
    }
    if (strcmp(method, "GET") == 0 && strcmp(path, "/api/v2/state") == 0) {
        handleApiStateGet(client);
        return;
    }
    if (strcmp(method, "POST") == 0 && strcmp(path, "/api/v2/settings") == 0) {
        handleApiSettingsPost(client, form);
        return;
    }
    if (strcmp(method, "POST") == 0 && strcmp(path, "/api/v2/control") == 0) {
        handleApiControlPost(client, form);
        return;
    }
    if (strcmp(method, "POST") == 0 && strcmp(path, "/api/v2/system/reboot") == 0) {
        handleApiSystemReboot(client);
        return;
    }
    if (strcmp(method, "POST") == 0 && strcmp(path, "/api/v2/system/dfu") == 0) {
        handleApiSystemDfu(client);
        return;
    }
    if (strcmp(method, "GET") == 0 && (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0)) {
        respond(client, 200, "text/html", webUiIndexHtml());
        return;
    }

    respond(client, 404, "text/plain", "not_found");
}

size_t WebConfigServer::urlDecode(const char* src, size_t src_len, char* dst, size_t dst_size) const {
    if (dst_size == 0) {
        return 0;
    }

    size_t out = 0;
    for (size_t i = 0; i < src_len && out + 1 < dst_size; ++i) {
        char c = src[i];
        if (c == '+') {
            dst[out++] = ' ';
        } else if (c == '%' && i + 2 < src_len) {
            int hi = hexToInt(src[i + 1]);
            int lo = hexToInt(src[i + 2]);
            if (hi >= 0 && lo >= 0) {
                dst[out++] = static_cast<char>((hi << 4) | lo);
                i += 2;
            } else {
                dst[out++] = c;
            }
        } else {
            dst[out++] = c;
        }
    }
    dst[out] = '\0';
    return out;
}

bool WebConfigServer::getParam(const char* data, const char* key, char* out, size_t out_size) const {
    if (data == nullptr || key == nullptr || out == nullptr || out_size == 0) {
        return false;
    }

    out[0] = '\0';
    size_t key_len = strlen(key);
    const char* p = data;
    while (*p != '\0') {
        while (*p == '&' || *p == '?' || *p == ' ') {
            ++p;
        }
        if (*p == '\0') {
            break;
        }

        if (strncmp(p, key, key_len) == 0 && p[key_len] == '=') {
            const char* value = p + key_len + 1;
            const char* end = value;
            while (*end != '\0' && *end != '&' && *end != ' ') {
                ++end;
            }
            urlDecode(value, static_cast<size_t>(end - value), out, out_size);
            return true;
        }

        const char* next_amp = strchr(p, '&');
        const char* next_q = strchr(p, '?');
        const char* next = nullptr;
        if (next_amp == nullptr) {
            next = next_q;
        } else if (next_q == nullptr) {
            next = next_amp;
        } else {
            next = (next_amp < next_q) ? next_amp : next_q;
        }
        if (next == nullptr) {
            break;
        }
        p = next + 1;
    }

    return false;
}

void WebConfigServer::respond(TCPClient& client, int status, const char* content_type, const char* body) {
    const char* text = (body == nullptr) ? "" : body;
    respondN(client, status, content_type, text, strlen(text));
}

void WebConfigServer::respondN(TCPClient& client,
                               int status,
                               const char* content_type,
                               const char* body,
                               size_t body_len) {
    client.printlnf("HTTP/1.1 %d %s", status, statusReason(status));
    client.printlnf("Content-Type: %s", content_type);
    client.printlnf("Content-Length: %u", static_cast<unsigned>(body_len));
    client.println("Connection: close");
    client.println();

    if (body_len > 0 && body != nullptr) {
        client.write(reinterpret_cast<const uint8_t*>(body), body_len);
    }
}
