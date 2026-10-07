#include "camera_protocol.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static bool parse(const char *text)
{
    camera_count_event_t event;
    return camera_parse_count(text, strlen(text), &event);
}

int main(void)
{
    uint16_t port = 0;
    assert(camera_parse_port("8080", &port) && port == 8080);
    assert(camera_parse_port("65535", &port) && port == 65535);
    const char *bad_ports[] = {"", "0", "65536", "-1", "+80", "80abc", " 80", "1.2", "999999999999999999999"};
    for (unsigned i = 0; i < sizeof(bad_ports)/sizeof(*bad_ports); ++i) assert(!camera_parse_port(bad_ports[i], &port));
    camera_count_event_t event;
    const char *valid = "{\"schema_version\":1,\"device\":{\"id\":\"CAM_01\"},\"timestamp\":1757820615,\"count\":12,\"results\":[]}";
    assert(camera_parse_count(valid, strlen(valid), &event));
    assert(event.count == 12 && event.timestamp == 1757820615 && !strcmp(event.device_id, "CAM_01"));
    assert(parse("{\"schema_version\":1,\"device\":{\"id\":\"A\"},\"timestamp\":1,\"count\":0} \n"));
    const char *bad[] = {
        "", "{}", "[]", "null", "{", "{\"count\":1}",
        "{\"schema_version\":2,\"device\":{\"id\":\"A\"},\"timestamp\":1,\"count\":1}",
        "{\"schema_version\":1,\"device\":{\"id\":\"\"},\"timestamp\":1,\"count\":1}",
        "{\"schema_version\":1,\"device\":{\"id\":\"A\"},\"timestamp\":1,\"count\":-1}",
        "{\"schema_version\":1,\"device\":{\"id\":\"A\"},\"timestamp\":1,\"count\":1.5}",
        "{\"schema_version\":1,\"device\":{\"id\":\"A\"},\"timestamp\":1,\"count\":\"1\"}",
        "{\"schema_version\":1,\"device\":{\"id\":\"A\"},\"timestamp\":1,\"count\":2147483648}",
        "{\"schema_version\":1,\"device\":{\"id\":\"A\"},\"timestamp\":0,\"count\":1}",
        "{\"schema_version\":1,\"device\":{\"id\":\"A\"},\"timestamp\":1.1,\"count\":1}",
        "{\"schema_version\":1,\"device\":{\"id\":\"A\"},\"timestamp\":1e99,\"count\":1}",
        "{\"schema_version\":1,\"device\":{\"id\":\"A\"},\"timestamp\":1,\"count\":1}garbage",
        "[[[[[[[[[[[[[[[[[0]]]]]]]]]]]]]]]]]"
    };
    for (unsigned i = 0; i < sizeof(bad)/sizeof(*bad); ++i) assert(!parse(bad[i]));
    /* Each truncated prefix must be rejected, including partial JSON numbers. */
    for (size_t n = 0; n < strlen(valid); ++n) assert(!camera_parse_count(valid, n, &event));
    uint8_t jpeg[] = {0xff,0xd8,0xff,0xe0,0,4,0,0,0xff,0xc0,0,8,8,2,0,3,0,0,0xff,0xd9};
    uint16_t width, height;
    assert(camera_jpeg_dimensions(jpeg, sizeof(jpeg), &width, &height) && width == 768 && height == 512);
    for (size_t n = 0; n < sizeof(jpeg); ++n) assert(!camera_jpeg_dimensions(jpeg,n,&width,&height));
    jpeg[9] = 0xc2;
    assert(!camera_jpeg_dimensions(jpeg,sizeof(jpeg),&width,&height));
    puts("PASS: camera JSON validation, bounded nesting/truncation, ports, JPEG dimensions");
    return 0;
}
