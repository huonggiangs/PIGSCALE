#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    char device_id[64];
    int count;
    int64_t timestamp;
} camera_count_event_t;

bool camera_parse_count(const char *json, size_t length, camera_count_event_t *out);
bool camera_parse_port(const char *text, uint16_t *out);
bool camera_jpeg_dimensions(const uint8_t *data, size_t length, uint16_t *width, uint16_t *height);
