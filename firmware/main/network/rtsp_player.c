#include "rtsp_player.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "camera_receiver.h"
#include "esp_fourcc.h"
#include "esp_h264_dec_param.h"
#include "esp_h264_dec_sw.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_media_provider.h"
#include "esp_rtsp_service.h"
#include "esp_rtsp_service_ops.h"
#include "esp_service.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define VIEW_W 460
#define VIEW_H 259
#define MAX_FRAME_W 1920
#define MAX_FRAME_H 1088

static const char *TAG = "rtsp_player";
static lv_obj_t *s_view;
static lv_obj_t *s_status;
static lv_image_dsc_t s_image;
static uint16_t *s_display_rgb;
static uint16_t *s_work_rgb;
static TaskHandle_t s_task;
static volatile bool s_enabled;

static bool make_authenticated_url(const camera_config_t *cfg, const char *source, char *out, size_t cap)
{
    if (strncmp(source, "rtsp://", 7) != 0) return false;
    const char *host = source + 7;
    while (*host == '/') host++;
    char user[64], pass[128];
    size_t u = 0, p = 0;
    for (const char *s = cfg->user; *s && u + 4 < sizeof(user); ++s) {
        if ((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || (*s >= '0' && *s <= '9') || strchr("-._~", *s)) user[u++] = *s;
        else u += (size_t)snprintf(user + u, sizeof(user) - u, "%%%02X", (unsigned char)*s);
    }
    user[u] = 0;
    for (const char *s = cfg->password; *s && p + 4 < sizeof(pass); ++s) {
        if ((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || (*s >= '0' && *s <= '9') || strchr("-._~", *s)) pass[p++] = *s;
        else p += (size_t)snprintf(pass + p, sizeof(pass) - p, "%%%02X", (unsigned char)*s);
    }
    pass[p] = 0;
    return snprintf(out, cap, "rtsp://%s:%s@%s", user, pass, host) < (int)cap;
}

static uint16_t yuv_to_rgb565(int y, int u, int v)
{
    int c = y - 16, d = u - 128, e = v - 128;
    if (c < 0) c = 0;
    int r = (298 * c + 409 * e + 128) >> 8;
    int g = (298 * c - 100 * d - 208 * e + 128) >> 8;
    int b = (298 * c + 516 * d + 128) >> 8;
    if (r < 0) r = 0; else if (r > 255) r = 255;
    if (g < 0) g = 0; else if (g > 255) g = 255;
    if (b < 0) b = 0; else if (b > 255) b = 255;
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

static void publish_i420(const uint8_t *src, uint16_t width, uint16_t height)
{
    if (!src || !width || !height || width > MAX_FRAME_W || height > MAX_FRAME_H) return;
    size_t y_size = (size_t)width * height;
    size_t chroma_size = y_size / 4;
    uint32_t out_w = VIEW_W, out_h = VIEW_H;
    if ((uint64_t)width * VIEW_H > (uint64_t)height * VIEW_W) out_h = (uint32_t)height * VIEW_W / width;
    else out_w = (uint32_t)width * VIEW_H / height;
    if (!out_w || !out_h) return;
    memset(s_work_rgb, 0, VIEW_W * VIEW_H * sizeof(uint16_t));
    uint32_t x0 = (VIEW_W - out_w) / 2, y0 = (VIEW_H - out_h) / 2;
    const uint8_t *yp = src, *up = src + y_size, *vp = up + chroma_size;
    for (uint32_t y = 0; y < out_h; ++y) {
        uint32_t sy = y * height / out_h;
        for (uint32_t x = 0; x < out_w; ++x) {
            uint32_t sx = x * width / out_w;
            size_t yi = (size_t)sy * width + sx;
            size_t ci = (size_t)(sy / 2) * ((width + 1) / 2) + sx / 2;
            s_work_rgb[(y0 + y) * VIEW_W + x0 + x] = yuv_to_rgb565(yp[yi], up[ci], vp[ci]);
        }
    }
    if (!lvgl_port_lock(50)) return;
    memcpy(s_display_rgb, s_work_rgb, VIEW_W * VIEW_H * sizeof(uint16_t));
    s_image.data_size = VIEW_W * VIEW_H * sizeof(uint16_t);
    lv_image_set_src(s_view, &s_image);
    lv_obj_add_flag(s_status, LV_OBJ_FLAG_HIDDEN);
    lvgl_port_unlock();
}

static bool run_stream(void)
{
    camera_config_t cfg;
    camera_receiver_get_config(&cfg);
    char url[512];
    const char *stream = cfg.rtsp_sub[0] ? cfg.rtsp_sub : cfg.rtsp_main;
    if (!make_authenticated_url(&cfg, stream, url, sizeof(url))) return false;

    esp_rtsp_service_cfg_t service_cfg = ESP_RTSP_SERVICE_CFG_DEFAULT(ESP_RTSP_SERVICE_ROLE_SRC);
    esp_rtsp_service_t *rtsp = NULL;
    esp_h264_dec_handle_t decoder = NULL;
    esp_media_provider_t provider = {0};
    bool started = false, opened = false, got_provider = false, success = false;
    if (esp_rtsp_service_create(&service_cfg, &rtsp) != ESP_OK) goto cleanup;
    esp_rtsp_service_setup_t setup = ESP_RTSP_SERVICE_SETUP_DEFAULT();
    setup.transport = RTSP_TRANSPORT_TCP;
    setup.video_enable = true;
    setup.audio_enable = false;
    setup.video_cache_size = 1024 * 1024;
    setup.vid_frame_size = 128 * 1024;
    if (esp_rtsp_service_setup(rtsp, &setup) != ESP_OK || esp_rtsp_service_set_url(rtsp, url) != ESP_OK) goto cleanup;
    if (esp_service_start(ESP_SERVICE_BASE(rtsp)) != ESP_OK) goto cleanup;
    started = true;

    for (int i = 0; i < 100 && s_enabled; ++i) {
        if (esp_media_service_get_provider(ESP_SERVICE_BASE(rtsp), ESP_MEDIA_DEFAULT_STREAM, &provider) == ESP_OK) {
            got_provider = true;
            uint16_t count = 0;
            if (esp_media_provider_get_track_num(&provider, &count) == ESP_OK && count) break;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    if (!got_provider || !s_enabled) goto cleanup;

    uint16_t track_count = 0, video_track = UINT16_MAX;
    if (esp_media_provider_get_track_num(&provider, &track_count) != ESP_OK) goto cleanup;
    for (uint16_t i = 0; i < track_count; ++i) {
        esp_media_track_info_t info = {0};
        if (esp_media_provider_get_track_info(&provider, i, &info) == ESP_OK && info.type == ESP_MEDIA_TRACK_TYPE_VIDEO) {
            video_track = info.id;
            if (info.info.video.codec != ESP_FOURCC_H264) {
                ESP_LOGE(TAG, "Camera codec is not H.264; H.265 is unsupported by this decoder");
                goto cleanup;
            }
            break;
        }
    }
    if (video_track == UINT16_MAX) goto cleanup;

    esp_h264_dec_cfg_sw_t dec_cfg = {.pic_type = ESP_H264_RAW_FMT_I420};
    if (esp_h264_dec_sw_new(&dec_cfg, &decoder) != ESP_H264_ERR_OK || !decoder) goto cleanup;
    if (esp_h264_dec_open(decoder) != ESP_H264_ERR_OK) goto cleanup;
    opened = true;

    while (s_enabled) {
        esp_media_frame_t frame = {.type = ESP_MEDIA_TRACK_TYPE_VIDEO, .track_id = video_track};
        if (esp_media_provider_acquire_frame(&provider, &frame, 500) != ESP_OK) continue;
        esp_h264_dec_in_frame_t in = {.raw_data = {.buffer = frame.data, .len = frame.size}, .pts = (uint32_t)frame.pts};
        while (in.raw_data.len && s_enabled) {
            esp_h264_dec_out_frame_t out = {0};
            esp_h264_err_t err = esp_h264_dec_process(decoder, &in, &out);
            if (err != ESP_H264_ERR_OK || in.consume == 0) break;
            in.raw_data.buffer += in.consume;
            in.raw_data.len -= in.consume;
            if (out.out_size) {
                esp_h264_dec_param_sw_handle_t params = NULL;
                esp_h264_resolution_t res = {0};
                if (esp_h264_dec_sw_get_param_hd(decoder, &params) == ESP_H264_ERR_OK &&
                    esp_h264_dec_get_resolution(params, &res) == ESP_H264_ERR_OK)
                    publish_i420(out.outbuf, res.width, res.height);
            }
        }
        esp_media_provider_release_frame(&provider, &frame);
        success = true;
    }

cleanup:
    if (opened) esp_h264_dec_close(decoder);
    if (decoder) esp_h264_dec_del(decoder);
    if (started) esp_service_stop(ESP_SERVICE_BASE(rtsp));
    if (rtsp) {
        esp_media_service_deinit(ESP_SERVICE_BASE(rtsp));
        free(rtsp);
    }
    return success;
}

static void player_task(void *arg)
{
    (void)arg;
    while (true) {
        if (!s_enabled) { ulTaskNotifyTake(pdTRUE, portMAX_DELAY); continue; }
        ESP_LOGI(TAG, "Starting H.264 RTSP pull");
        run_stream();
        if (lvgl_port_lock(50)) {
            lv_obj_clear_flag(s_status, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(s_status, "RTSP/H.264: chờ hoặc mất kết nối");
            lvgl_port_unlock();
        }
        for (int i = 0; i < 30 && s_enabled; ++i) vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void rtsp_player_init(lv_obj_t *parent)
{
    s_display_rgb = heap_caps_malloc(VIEW_W * VIEW_H * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_work_rgb = heap_caps_malloc(VIEW_W * VIEW_H * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_display_rgb || !s_work_rgb) return;
    memset(s_display_rgb, 0, VIEW_W * VIEW_H * sizeof(uint16_t));
    s_image = (lv_image_dsc_t){
        .header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565, .w = VIEW_W, .h = VIEW_H},
        .data_size = VIEW_W * VIEW_H * sizeof(uint16_t), .data = (const uint8_t *)s_display_rgb,
    };
    s_view = lv_image_create(parent);
    lv_obj_set_size(s_view, VIEW_W, VIEW_H);
    lv_image_set_src(s_view, &s_image);
    lv_obj_set_pos(s_view, 0, 0);
    s_status = lv_label_create(parent);
    lv_label_set_text(s_status, "Đang chờ phiên RTSP");
    lv_obj_set_style_text_color(s_status, lv_color_white(), 0);
    lv_obj_center(s_status);
    xTaskCreatePinnedToCore(player_task, "rtsp_h264", 12288, NULL, 5, &s_task, 1);
}

void rtsp_player_set_enabled(bool enabled)
{
    s_enabled = enabled;
    if (s_task) xTaskNotifyGive(s_task);
}
