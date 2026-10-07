/* Exercise production widgets and callbacks against the real LVGL 9.2 engine. */
#include <time.h>
struct tm *localtime_r(const time_t *, struct tm *);
#include <assert.h>
#include "../../main/ui/ui_settings.c"

static app_state_t state;
static camera_config_t config = {.ip="192.168.1.10", .https_port=443, .receive_port=8080, .user="admin",
    .password="a123456@", .rtsp_main="rtsp://192.168.1.10:554/live/0", .rtsp_sub="rtsp://192.168.1.10:554/live/1"};
static bool scanning;
static uint8_t pixels[800 * 1280 * 4];
app_state_t *app_state(void) { return &state; }
int64_t esp_timer_get_time(void) { return 1000000; }
struct tm *localtime_r(const time_t *t, struct tm *out)
{ struct tm *v=localtime(t); if(!v)return NULL; *out=*v; return out; }
bool app_state_wifi_is_scanning(void) { return scanning; }
void app_state_wifi_scan(void) { scanning = true; }
void app_state_wifi_connect(int i, const char *p) { (void)i; (void)p; }
bool app_state_wifi_set_static_ip(bool e,const char *a,const char *b,const char *c,const char *d)
{ (void)e;(void)a;(void)b;(void)c;(void)d; return true; }
bool app_state_time_is_synced(void) { return false; }
void app_state_time_force_sync(void) {}
bool app_state_camera_is_checking(void) { return false; }
void app_state_camera_save_config(const char *ip, uint16_t port)
{ snprintf(state.camera_ip,sizeof(state.camera_ip),"%s",ip); state.camera_port=port; }
void app_state_camera_test_connect(const char *ip,uint16_t port) { (void)ip;(void)port; }
void ui_shell_toast(const char *s) { (void)s; }
void ui_shell_refresh_chrome(void) {}
void camera_receiver_get_config(camera_config_t *out) { *out=config; }
esp_err_t camera_receiver_save_config(const camera_config_t *cfg) { config=*cfg; return ESP_OK; }
void camera_receiver_get_status(camera_receiver_status_t *out)
{ memset(out,0,sizeof(*out)); out->running=true; out->listening_port=8080; }
uint8_t *camera_receiver_copy_snapshot(size_t *s,uint16_t *w,uint16_t *h,camera_count_event_t *e)
{ (void)s;(void)w;(void)h;(void)e; return NULL; }
void camera_receiver_endpoint(char *out,size_t size)
{ snprintf(out,size,"http://192.168.1.50:8080/api/device-data"); }
bool camera_parse_port(const char *s,uint16_t *out)
{ char *end; long v=strtol(s,&end,10); if (!*s || *end || v<1 || v>65535) return false; *out=v; return true; }

static void flush(lv_display_t *display,const lv_area_t *area,uint8_t *map)
{ (void)area;(void)map; lv_display_flush_ready(display); }

static void screenshot(const char *name)
{
    lv_obj_invalidate(lv_screen_active());
    lv_refr_now(NULL);
    FILE *file=fopen(name,"wb"); assert(file);
    fwrite(pixels,1,sizeof(pixels),file); fclose(file);
}

int main(void)
{
    lv_init();
    lv_display_t *display=lv_display_create(800,1280);
    lv_display_set_buffers(display,pixels,NULL,sizeof(pixels),LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(display,flush);
    state.current_employee_idx=0;
    state.settings_unlocked=true;
    state.employees[0].role=ROLE_TECHNICIAN;
    strcpy(state.camera_ip,config.ip); state.camera_port=443;
    lv_obj_t *viewport=lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(viewport);
    lv_obj_set_size(viewport,UI_HOR_RES,UI_CONTENT_H);
    lv_obj_set_pos(viewport,0,UI_CONTENT_Y);
    lv_obj_set_scroll_dir(viewport,LV_DIR_VER);
    ui_settings_create(viewport);
    ui_settings_refresh();
    assert(!strcmp(lv_textarea_get_text(s_cam_ip_ta),"192.168.1.10"));
    assert(!strcmp(lv_textarea_get_text(s_cam_user_ta),"admin"));
    assert(!strcmp(lv_textarea_get_text(s_cam_pass_ta),"a123456@"));
    assert(!strcmp(lv_textarea_get_text(s_rtsp_main_ta),config.rtsp_main));
    assert(!strcmp(lv_textarea_get_text(s_rtsp_sub_ta),config.rtsp_sub));
    lv_textarea_set_text(s_cam_token_ta,"test-bearer-token");
    lv_textarea_set_text(s_cam_user_ta,"draft-user");
    lv_textarea_set_text(s_cam_pass_ta,"draft-pass");
    assert(cam_save_fields());
    assert(!strcmp(config.ip,"192.168.1.10") && config.https_port==443 && config.receive_port==8080);
    assert(!strcmp(config.user,"draft-user") && !strcmp(config.password,"draft-pass"));
    assert(!strcmp(config.token,"test-bearer-token"));
    lv_textarea_set_text(s_cam_user_ta,"admin");
    lv_textarea_set_text(s_cam_pass_ta,"a123456@");
    lv_textarea_set_text(s_cam_token_ta,"");
        lv_obj_t *fields[]={s_cam_ip_ta,s_cam_port_ta,s_cam_user_ta,s_cam_pass_ta,s_rtsp_main_ta,s_rtsp_sub_ta,s_rx_port_ta,s_cam_token_ta};
    for (unsigned i=0;i<sizeof(fields)/sizeof(*fields);++i) {
        lv_obj_send_event(fields[i],LV_EVENT_FOCUSED,NULL);
        lv_obj_update_layout(lv_screen_active());
        lv_obj_update_layout(lv_layer_top());
        assert(!lv_obj_has_flag(s_kb,LV_OBJ_FLAG_HIDDEN));
        assert(lv_keyboard_get_textarea(s_kb)==fields[i]);
        lv_area_t kb,field;
        lv_obj_get_coords(s_kb,&kb); lv_obj_get_coords(fields[i],&field);
        assert(kb.y1==UI_VER_RES-UI_BOTTOMNAV_H-320);
        assert(kb.y2==UI_VER_RES-UI_BOTTOMNAV_H-1);
        assert(field.y1>=UI_CONTENT_Y && field.y2<kb.y1);
        lv_obj_send_event(s_kb,LV_EVENT_READY,NULL);
        assert(lv_obj_has_flag(s_kb,LV_OBJ_FLAG_HIDDEN));
        assert(!lv_keyboard_get_textarea(s_kb));
        /* Re-tapping an already focused field must reopen the keyboard. */
        lv_obj_send_event(fields[i],LV_EVENT_CLICKED,NULL);
        assert(lv_keyboard_get_textarea(s_kb)==fields[i]);
        lv_obj_send_event(s_kb,LV_EVENT_CANCEL,NULL);
        assert(lv_obj_has_flag(s_kb,LV_OBJ_FLAG_HIDDEN));
    }
    lv_textarea_set_text(s_cam_user_ta,"draft-user");
    lv_obj_send_event(s_cam_user_ta,LV_EVENT_CLICKED,NULL);
    screenshot("settings-keyboard.bgra");
    /* Deferred scan refresh must not remove the textarea being edited. */
    s_wifi_was_scanning=true; scanning=false;
    lv_obj_t *editing=s_cam_user_ta;
    ui_settings_tick();
    assert(s_cam_user_ta==editing && lv_keyboard_get_textarea(s_kb)==editing);
    ui_settings_close_keyboard();
    ui_settings_tick();
    assert(!strcmp(lv_textarea_get_text(s_cam_user_ta),"draft-user"));
    assert(!lv_keyboard_get_textarea(s_kb));
    lv_obj_update_layout(viewport);
    assert(lv_obj_get_height(viewport)==UI_CONTENT_H);
    lv_obj_scroll_to_view_recursive(s_rx_port_ta,LV_ANIM_OFF);
    screenshot("settings-receiver.bgra");
    /* A rebuild must detach the keyboard before the old textarea is freed. */
    lv_obj_send_event(s_cam_pass_ta,LV_EVENT_CLICKED,NULL);
    ui_settings_refresh();
    assert(!lv_keyboard_get_textarea(s_kb) && lv_obj_has_flag(s_kb,LV_OBJ_FLAG_HIDDEN));
    puts("PASS: eight camera fields, viewport geometry, reopen/ready/cancel, refresh safety, draft preservation");
    return 0;
}
