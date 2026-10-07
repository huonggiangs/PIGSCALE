#include "camera_protocol.h"
#include "cJSON.h"
#include <limits.h>
#include <math.h>
#include <string.h>

bool camera_parse_port(const char *text, uint16_t *out)
{
    if (!text || !*text || !out) return false;
    unsigned value = 0;
    for (; *text; ++text) {
        if (*text < '0' || *text > '9') return false;
        value = value * 10 + (unsigned)(*text - '0');
        if (value > 65535) return false;
    }
    if (!value) return false;
    *out = (uint16_t)value;
    return true;
}

bool camera_parse_count(const char *json, size_t length, camera_count_event_t *out)
{
    if (!json || !length || !out) return false;
    /* Bound recursive cJSON parsing on the HTTP task's small stack. */
    int depth = 0;
    bool quoted = false, escaped = false;
    for (size_t i = 0; i < length; ++i) {
        unsigned char c = (unsigned char)json[i];
        if (!c) return false;
        if (quoted) {
            if (escaped) escaped = false;
            else if (c == '\\') escaped = true;
            else if (c == '"') quoted = false;
        } else if (c == '"') quoted = true;
        else if (c == '{' || c == '[') { if (++depth > 16) return false; }
        else if (c == '}' || c == ']') { if (--depth < 0) return false; }
    }
    if (quoted || depth) return false;
    const char *end = NULL;
    cJSON *root = cJSON_ParseWithLengthOpts(json, length, &end, false);
    if (!root) return false;
    while (end < json + length && (*end == ' ' || *end == '\r' || *end == '\n' || *end == '\t')) ++end;
    cJSON *version = cJSON_GetObjectItemCaseSensitive(root, "schema_version");
    cJSON *device = cJSON_GetObjectItemCaseSensitive(root, "device");
    cJSON *id = cJSON_GetObjectItemCaseSensitive(device, "id");
    cJSON *count = cJSON_GetObjectItemCaseSensitive(root, "count");
    cJSON *ts = cJSON_GetObjectItemCaseSensitive(root, "timestamp");
    bool ok = end == json + length && cJSON_IsObject(root) &&
        cJSON_IsNumber(version) && version->valuedouble == 1 &&
        cJSON_IsString(id) && id->valuestring[0] && strlen(id->valuestring) < sizeof(out->device_id) &&
        cJSON_IsNumber(count) && isfinite(count->valuedouble) &&
        count->valuedouble >= 0 && count->valuedouble <= INT_MAX && floor(count->valuedouble) == count->valuedouble &&
        cJSON_IsNumber(ts) && isfinite(ts->valuedouble) && ts->valuedouble > 0 &&
        ts->valuedouble <= 9007199254740991.0 && floor(ts->valuedouble) == ts->valuedouble;
    if (ok) {
        strcpy(out->device_id, id->valuestring);
        out->count = (int)count->valuedouble;
        out->timestamp = (int64_t)ts->valuedouble;
    }
    cJSON_Delete(root);
    return ok;
}

/* Baseline JPEG only: the LVGL TJPGD decoder does not support progressive JPEG. */
bool camera_jpeg_dimensions(const uint8_t *data, size_t length, uint16_t *width, uint16_t *height)
{
    if (!data || !width || !height || length < 4 || data[0] != 0xff || data[1] != 0xd8 ||
        data[length - 2] != 0xff || data[length - 1] != 0xd9) return false;
    size_t pos = 2;
    while (pos + 3 < length) {
        if (data[pos++] != 0xff) return false;
        while (pos < length && data[pos] == 0xff) ++pos;
        if (pos + 2 >= length) return false;
        uint8_t marker = data[pos++];
        if (marker == 0xda || marker == 0xd9) return false;
        size_t size = ((size_t)data[pos] << 8) | data[pos + 1];
        if (size < 2 || size > length - pos) return false;
        if (marker == 0xc0) {
            if (size < 8 || data[pos + 2] != 8) return false;
            *height = ((uint16_t)data[pos + 3] << 8) | data[pos + 4];
            *width = ((uint16_t)data[pos + 5] << 8) | data[pos + 6];
            return *width > 0 && *height > 0 && *width <= 4096 && *height <= 4096;
        }
        pos += size;
    }
    return false;
}
