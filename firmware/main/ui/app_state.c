#include "app_state.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "wifi_manager.h"

static app_state_t s_state;

app_state_t *app_state(void) { return &s_state; }

const char *app_role_label(user_role_t role)
{
    switch (role) {
        case ROLE_OPERATOR:   return "Nhân viên cân";
        case ROLE_MANAGER:    return "Quản lý";
        case ROLE_TECHNICIAN: return "Kỹ thuật";
        default: return "";
    }
}

/* ── Khởi tạo dữ liệu demo ───────────────────────────────────────────────── */
void app_state_init(void)
{
    memset(&s_state, 0, sizeof(s_state));

    s_state.current_employee_idx = -1;
    s_state.current_tab = TAB_ORDERS;
    s_state.order_filter = ORDER_FILTER_ALL;

    /* nhân viên demo — đúng PIN nêu trong esp32p4-lvgl-handoff.md / luongcan.md */
    s_state.employee_count = 3;
    snprintf(s_state.employees[0].code, sizeof(s_state.employees[0].code), "NV001");
    snprintf(s_state.employees[0].name, sizeof(s_state.employees[0].name), "Nguyễn Văn A");
    s_state.employees[0].role = ROLE_OPERATOR;
    snprintf(s_state.employees[0].pin, sizeof(s_state.employees[0].pin), "1111");

    snprintf(s_state.employees[1].code, sizeof(s_state.employees[1].code), "QL001");
    snprintf(s_state.employees[1].name, sizeof(s_state.employees[1].name), "Trần Thị B");
    s_state.employees[1].role = ROLE_MANAGER;
    snprintf(s_state.employees[1].pin, sizeof(s_state.employees[1].pin), "2222");

    snprintf(s_state.employees[2].code, sizeof(s_state.employees[2].code), "KT001");
    snprintf(s_state.employees[2].name, sizeof(s_state.employees[2].name), "Lê Văn C");
    s_state.employees[2].role = ROLE_TECHNICIAN;
    snprintf(s_state.employees[2].pin, sizeof(s_state.employees[2].pin), "3333");

    /* trạm cân demo */
    s_state.station_count = 3;
    /* Lưu sẵn dạng IN HOA vì thanh trạng thái luôn hiển thị IN HOA
     * (vd. "TRẠM CÂN 01") — tránh phải viết-hoa runtime chuỗi UTF-8 tiếng Việt. */
    snprintf(s_state.stations[0].id, sizeof(s_state.stations[0].id), "TC01");
    snprintf(s_state.stations[0].name, sizeof(s_state.stations[0].name), "TRẠM CÂN 01");
    snprintf(s_state.stations[1].id, sizeof(s_state.stations[1].id), "TC02");
    snprintf(s_state.stations[1].name, sizeof(s_state.stations[1].name), "TRẠM CÂN 02");
    snprintf(s_state.stations[2].id, sizeof(s_state.stations[2].id), "TC03");
    snprintf(s_state.stations[2].name, sizeof(s_state.stations[2].name), "TRẠM CÂN 03");
    s_state.current_station_idx = 0;

    /* đơn hàng demo (mẫu theo luongcan.md mục 5.2) */
    struct { const char *code, *customer, *plate; int qty; ticket_type_t type; order_status_t st; } demo[] = {
        {"DH-260927-001", "Trại Chăn Nuôi An Phát",  "51C-123.45", 20, TICKET_IMPORT, ORDER_PENDING},
        {"DH-260927-002", "Trại Chăn Nuôi Xanh",     "51D-456.78", 15, TICKET_EXPORT, ORDER_PENDING},
        {"DH-260927-003", "Công ty CP Thức Ăn Miền Nam", "60A-111.22", 30, TICKET_IMPORT, ORDER_PENDING},
        {"DH-260927-004", "Trại Ba Vì",              "29H-333.44", 10, TICKET_EXPORT, ORDER_PENDING},
        {"DH-260927-005", "Trại Chăn Nuôi An Phát",  "51C-999.88", 25, TICKET_IMPORT, ORDER_PENDING},
        {"DH-260926-014", "Trại Đồng Nai",           "60B-222.33", 18, TICKET_EXPORT, ORDER_DONE},
        {"DH-260926-013", "Trại Chăn Nuôi Xanh",     "51D-777.66", 12, TICKET_IMPORT, ORDER_DONE},
    };
    int n = (int)(sizeof(demo) / sizeof(demo[0]));
    s_state.order_count = n;
    for (int i = 0; i < n; i++) {
        order_t *o = &s_state.orders[i];
        snprintf(o->id, sizeof(o->id), "ORD-%03d", i + 1);
        snprintf(o->code, sizeof(o->code), "%s", demo[i].code);
        snprintf(o->customer, sizeof(o->customer), "%s", demo[i].customer);
        snprintf(o->plate, sizeof(o->plate), "%s", demo[i].plate);
        o->planned_qty = demo[i].qty;
        o->ticket_type = demo[i].type;
        o->status = demo[i].st;
        if (o->status == ORDER_DONE) {
            o->synced = true;
            o->actual_qty = demo[i].qty;
            o->actual_weight_kg = demo[i].qty * 282.13f;
            snprintf(o->receipt_no, sizeof(o->receipt_no), "PC-20260926-%03d", i + 1);
        }
    }

    /* lịch sử demo khớp với 2 đơn ORDER_DONE ở trên */
    s_state.history_count = 2;
    snprintf(s_state.history[0].receipt_no, sizeof(s_state.history[0].receipt_no), "PC-20260926-006");
    snprintf(s_state.history[0].order_code, sizeof(s_state.history[0].order_code), "DH-260926-014");
    snprintf(s_state.history[0].customer, sizeof(s_state.history[0].customer), "Trại Đồng Nai");
    snprintf(s_state.history[0].plate, sizeof(s_state.history[0].plate), "60B-222.33");
    s_state.history[0].type = TICKET_EXPORT;
    s_state.history[0].actual_qty = 18;
    s_state.history[0].actual_weight_kg = 18 * 282.13f;

    snprintf(s_state.history[1].receipt_no, sizeof(s_state.history[1].receipt_no), "PC-20260926-007");
    snprintf(s_state.history[1].order_code, sizeof(s_state.history[1].order_code), "DH-260926-013");
    snprintf(s_state.history[1].customer, sizeof(s_state.history[1].customer), "Trại Chăn Nuôi Xanh");
    snprintf(s_state.history[1].plate, sizeof(s_state.history[1].plate), "51D-777.66");
    s_state.history[1].type = TICKET_IMPORT;
    s_state.history[1].actual_qty = 12;
    s_state.history[1].actual_weight_kg = 12 * 282.13f;
    s_state.history[1].is_manual = true;

    /* Wi-Fi: danh sách thật lấy từ wifi_manager (ESP32-C6/SDIO), không còn mô
       phỏng — xem app_state_wifi_scan()/app_state_sim_tick(). Rỗng cho tới khi
       có kết quả quét đầu tiên hoặc đã auto-connect từ NVS. */
    s_state.wifi_network_count = 0;
    s_state.wifi_connected = false;

    s_state.printer_type = PRINTER_THERMAL_58;
    s_state.bright_mode = false;

    s_state.wifi_link = LINK_OK;
    s_state.p5_link = LINK_OK;
    s_state.camera_link = LINK_OK;

    snprintf(s_state.fw_version, sizeof(s_state.fw_version), "v1.0.0-dev");
    snprintf(s_state.fw_build, sizeof(s_state.fw_build), "build 2026.09.27");

    app_state_refresh_alerts();
}

/* ── Đăng nhập ───────────────────────────────────────────────────────────── */
void app_state_select_employee(int idx)
{
    if (idx < 0 || idx >= s_state.employee_count) return;
    s_state.current_employee_idx = idx;
    s_state.pin_input[0] = '\0';
}

void app_state_pin_digit(char digit)
{
    size_t len = strlen(s_state.pin_input);
    if (len < 4) {
        s_state.pin_input[len] = digit;
        s_state.pin_input[len + 1] = '\0';
    }
}

void app_state_pin_backspace(void)
{
    size_t len = strlen(s_state.pin_input);
    if (len > 0) s_state.pin_input[len - 1] = '\0';
}

void app_state_pin_clear(void)
{
    s_state.pin_input[0] = '\0';
}

bool app_state_try_login(void)
{
    if (s_state.current_employee_idx < 0) return false;
    employee_t *e = &s_state.employees[s_state.current_employee_idx];
    if (strcmp(e->pin, s_state.pin_input) == 0) {
        s_state.logged_in = true;
        s_state.current_tab = TAB_ORDERS;
        s_state.settings_unlocked = (e->role != ROLE_OPERATOR);
        app_state_pin_clear();
        return true;
    }
    app_state_pin_clear();
    return false;
}

void app_state_logout(void)
{
    s_state.logged_in = false;
    s_state.current_employee_idx = -1;
    s_state.settings_unlocked = false;
    app_state_pin_clear();
}

bool app_state_try_settings_unlock(void)
{
    if (s_state.current_employee_idx < 0) return false;
    employee_t *e = &s_state.employees[s_state.current_employee_idx];
    if (strcmp(e->pin, s_state.pin_input) == 0) {
        s_state.settings_unlocked = true;
        app_state_pin_clear();
        return true;
    }
    app_state_pin_clear();
    return false;
}

/* ── Đơn hàng ────────────────────────────────────────────────────────────── */
order_t *app_state_find_order(const char *order_id)
{
    for (int i = 0; i < s_state.order_count; i++) {
        if (strcmp(s_state.orders[i].id, order_id) == 0) return &s_state.orders[i];
    }
    return NULL;
}

int app_state_get_filtered_orders(order_t *out[], int max_out)
{
    int n = 0;
    for (int i = 0; i < s_state.order_count && n < max_out; i++) {
        order_t *o = &s_state.orders[i];
        bool keep = true;
        switch (s_state.order_filter) {
            case ORDER_FILTER_PENDING:  keep = (o->status == ORDER_PENDING); break;
            case ORDER_FILTER_WEIGHING: keep = (o->status == ORDER_WEIGHING); break;
            case ORDER_FILTER_DONE:     keep = (o->status == ORDER_DONE); break;
            default: keep = true; break;
        }
        if (keep) out[n++] = o;
    }
    return n;
}

/* ── Cân ─────────────────────────────────────────────────────────────────── */
void app_state_start_weighing(const char *order_id)
{
    order_t *o = app_state_find_order(order_id);
    if (!o) return;

    /* chỉ 1 đơn được cân cùng lúc */
    for (int i = 0; i < s_state.order_count; i++) {
        if (s_state.orders[i].status == ORDER_WEIGHING) s_state.orders[i].status = ORDER_PENDING;
    }
    o->status = ORDER_WEIGHING;

    memset(&s_state.weighing, 0, sizeof(s_state.weighing));
    s_state.weighing.active = true;
    snprintf(s_state.weighing.order_id, sizeof(s_state.weighing.order_id), "%s", o->id);
    s_state.weighing.ticket_type = o->ticket_type;
    s_state.weighing.p5_state = (s_state.p5_link == LINK_LOST) ? P5_STATE_LOST : P5_STATE_MOVING;
    s_state.weighing.camera_connected = (s_state.camera_link != LINK_LOST);
    s_state.weighing.reconcile = RECONCILE_NONE;

    s_state.current_tab = TAB_WEIGHING;
}

void app_state_weighing_set_ticket_type(ticket_type_t t)
{
    if (!s_state.weighing.active) return;
    s_state.weighing.ticket_type = t;
}

void app_state_weighing_manual_input(float weight_kg, int qty)
{
    s_state.weighing.manual_mode = true;
    s_state.weighing.manual_weight_kg = weight_kg;
    s_state.weighing.manual_qty = qty;
    s_state.weighing.manual_filled = true;
    s_state.weighing.weight_kg = weight_kg;
    s_state.weighing.p5_state = P5_STATE_MANUAL;
    s_state.weighing.has_line_count = true;
    s_state.weighing.line_count = qty;
    s_state.weighing.has_snapshot_count = true;
    s_state.weighing.snapshot_count = qty;
    s_state.weighing.reconcile = RECONCILE_MANUAL;
}

void app_state_weighing_reweigh(void)
{
    weighing_session_t *w = &s_state.weighing;
    w->weight_kg = 0;
    w->has_line_count = false;
    w->has_snapshot_count = false;
    w->line_count = 0;
    w->snapshot_count = 0;
    w->reconcile = RECONCILE_NONE;
    w->manual_mode = false;
    w->manual_filled = false;
    if (s_state.p5_link != LINK_LOST) w->p5_state = P5_STATE_MOVING;
}

bool app_state_weighing_can_confirm(void)
{
    weighing_session_t *w = &s_state.weighing;
    if (!w->active) return false;
    if (!s_state.wifi_connected && s_state.wifi_link == LINK_LOST) {
        /* vẫn cho phép xác nhận khi mất mạng — phiếu sẽ vào hàng đợi đồng bộ,
         * theo luongcan.md mục 4. Chỉ chặn khi CHƯA đủ số liệu. */
    }
    if (w->manual_mode) return w->manual_filled;
    return w->has_line_count && w->has_snapshot_count && w->p5_state != P5_STATE_LOST;
}

void app_state_weighing_confirm(void)
{
    weighing_session_t *w = &s_state.weighing;
    if (!w->active) return;
    order_t *o = app_state_find_order(w->order_id);
    if (!o) return;

    o->status = ORDER_DONE;
    o->is_manual_entry = w->manual_mode;
    o->actual_qty = w->manual_mode ? w->manual_qty
                    : (w->has_line_count ? w->line_count : w->snapshot_count);
    o->actual_weight_kg = w->weight_kg;

    bool have_network = (s_state.wifi_link != LINK_LOST);
    static int s_receipt_seq = 100;
    s_receipt_seq++;
    snprintf(o->receipt_no, sizeof(o->receipt_no), "PC-20260927-%03d", s_receipt_seq);

    if (have_network) {
        o->synced = true;
        o->pending_sync = false;
    } else {
        o->synced = false;
        o->pending_sync = true;
        s_state.pending_sync_count++;
    }

    /* thêm vào lịch sử */
    if (s_state.history_count < APP_MAX_HISTORY) {
        history_ticket_t *h = &s_state.history[s_state.history_count++];
        memset(h, 0, sizeof(*h));
        snprintf(h->receipt_no, sizeof(h->receipt_no), "%s", o->receipt_no);
        snprintf(h->order_code, sizeof(h->order_code), "%s", o->code);
        snprintf(h->customer, sizeof(h->customer), "%s", o->customer);
        snprintf(h->plate, sizeof(h->plate), "%s", o->plate);
        h->type = o->ticket_type;
        h->actual_qty = o->actual_qty;
        h->actual_weight_kg = o->actual_weight_kg;
        h->is_manual = o->is_manual_entry;
    }

    w->active = false;
    memset(w, 0, sizeof(*w));
}

void app_state_sync_now(void)
{
    for (int i = 0; i < s_state.order_count; i++) {
        if (s_state.orders[i].pending_sync) {
            s_state.orders[i].pending_sync = false;
            s_state.orders[i].synced = true;
        }
    }
    s_state.pending_sync_count = 0;
}

/* ── Mô phỏng số liệu "sống" ─────────────────────────────────────────────── */
static void app_state_wifi_sync(void);

void app_state_sim_tick(void)
{
    app_state_wifi_sync();

    weighing_session_t *w = &s_state.weighing;
    if (w->active && !w->manual_mode && w->p5_state != P5_STATE_LOST) {
        /* tăng dần khối lượng mô phỏng tới khi ổn định quanh kế hoạch */
        order_t *o = app_state_find_order(w->order_id);
        float target = o ? (o->planned_qty * 282.13f) : 1000.0f;
        float step = target * 0.08f;
        if (w->weight_kg < target) {
            w->weight_kg += step;
            w->p5_state = P5_STATE_MOVING;
            if (w->weight_kg >= target) w->weight_kg = target;
        } else {
            w->p5_state = P5_STATE_STABLE;
            if (!w->has_line_count && o) {
                w->has_line_count = true;
                w->line_count = o->planned_qty;
            }
            if (!w->has_snapshot_count && o) {
                w->has_snapshot_count = true;
                /* mô phỏng lệch nhẹ đôi khi để minh hoạ trạng thái LỆCH/CHƯA CHẮC */
                w->snapshot_count = o->planned_qty;
            }
            if (w->has_line_count && w->has_snapshot_count) {
                w->reconcile = (w->line_count == w->snapshot_count) ? RECONCILE_MATCH : RECONCILE_MISMATCH;
            }
        }
    }
}

/* ── Cảnh báo ────────────────────────────────────────────────────────────── */
void app_state_ack_alert(const char *alert_id)
{
    for (int i = 0; i < s_state.alert_count; i++) {
        if (strcmp(s_state.alerts[i].id, alert_id) == 0) {
            s_state.alerts[i].acknowledged = true;
            return;
        }
    }
}

static bool find_old_acknowledged(const alert_t *old, int old_count, const char *id)
{
    for (int i = 0; i < old_count; i++) {
        if (strcmp(old[i].id, id) == 0) return old[i].acknowledged;
    }
    return false;
}

void app_state_refresh_alerts(void)
{
    /* giữ danh sách cũ để bảo toàn cờ "đã xem" cho cảnh báo còn hiệu lực —
     * nếu không, mỗi lần tick mô phỏng (500ms) sẽ xoá mất trạng thái vừa bấm */
    alert_t old[APP_MAX_ALERTS];
    int old_count = s_state.alert_count;
    memcpy(old, s_state.alerts, sizeof(old));

    int n = 0;
    alert_t *a;

    /* cảnh báo cố định — luôn có mặt (theo esp32p4-lvgl-handoff.md mục 4.7) */
    a = &s_state.alerts[n++];
    memset(a, 0, sizeof(*a));
    snprintf(a->id, sizeof(a->id), "fixed-qty");
    a->level = ALERT_INFO;
    snprintf(a->title, sizeof(a->title), "Đơn thiếu số con");
    snprintf(a->desc, sizeof(a->desc), "Một số đơn hàng chưa nhập đủ kế hoạch số con trước khi cân.");
    snprintf(a->action, sizeof(a->action), "Kiểm tra lại kế hoạch số con ở tab Đơn hàng");
    a->active = true;
    a->acknowledged = find_old_acknowledged(old, old_count, a->id);

    if (s_state.p5_link == LINK_LOST) {
        a = &s_state.alerts[n++];
        memset(a, 0, sizeof(*a));
        snprintf(a->id, sizeof(a->id), "p5-lost");
        a->level = ALERT_CRITICAL;
        snprintf(a->title, sizeof(a->title), "Mất kết nối P5 Scale");
        snprintf(a->desc, sizeof(a->desc), "Đầu cân P5 Scale không phản hồi. Không thể đọc khối lượng tự động.");
        snprintf(a->action, sizeof(a->action), "Kiểm tra nguồn/điện IP 192.168.1.61 hoặc dùng Nhập tay");
        a->active = true;
        a->acknowledged = find_old_acknowledged(old, old_count, a->id);
    }
    if (s_state.camera_link == LINK_LOST) {
        a = &s_state.alerts[n++];
        memset(a, 0, sizeof(*a));
        snprintf(a->id, sizeof(a->id), "camera-lost");
        a->level = ALERT_CRITICAL;
        snprintf(a->title, sizeof(a->title), "Mất kết nối Camera");
        snprintf(a->desc, sizeof(a->desc), "Camera 01 không cấp được khung hình nhận diện.");
        snprintf(a->action, sizeof(a->action), "Kiểm tra Camera 01 (192.168.1.50) hoặc dùng Nhập tay");
        a->active = true;
        a->acknowledged = find_old_acknowledged(old, old_count, a->id);
    }
    if (s_state.wifi_link == LINK_WEAK) {
        a = &s_state.alerts[n++];
        memset(a, 0, sizeof(*a));
        snprintf(a->id, sizeof(a->id), "wifi-weak");
        a->level = ALERT_WARNING;
        snprintf(a->title, sizeof(a->title), "Wi-Fi yếu");
        snprintf(a->desc, sizeof(a->desc), "Tín hiệu Wi-Fi hiện tại yếu, có thể ảnh hưởng tới đồng bộ phiếu.");
        snprintf(a->action, sizeof(a->action), "Kiểm tra vị trí gateway hoặc đổi mạng ở Cài đặt");
        a->active = true;
        a->acknowledged = find_old_acknowledged(old, old_count, a->id);
    }
    if (s_state.weighing.reconcile == RECONCILE_MISMATCH) {
        a = &s_state.alerts[n++];
        memset(a, 0, sizeof(*a));
        snprintf(a->id, sizeof(a->id), "count-mismatch");
        a->level = ALERT_WARNING;
        snprintf(a->title, sizeof(a->title), "Lệch số con");
        snprintf(a->desc, sizeof(a->desc), "Số con qua vạch và số con ảnh tĩnh không khớp cho phiên đang cân.");
        snprintf(a->action, sizeof(a->action), "Cân lại hoặc kiểm tra camera ở tab Cân");
        a->active = true;
        a->acknowledged = find_old_acknowledged(old, old_count, a->id);
    }
    if (s_state.pending_sync_count > 0) {
        a = &s_state.alerts[n++];
        memset(a, 0, sizeof(*a));
        snprintf(a->id, sizeof(a->id), "pending-sync");
        a->level = ALERT_WARNING;
        snprintf(a->title, sizeof(a->title), "Phiếu chờ đồng bộ");
        snprintf(a->desc, sizeof(a->desc), "%d phiếu cân đang chờ đồng bộ lên server nội bộ do mất mạng lúc xác nhận.", s_state.pending_sync_count);
        snprintf(a->action, sizeof(a->action), "Bấm Đồng bộ lại ở thanh phụ khi có mạng");
        a->active = true;
        a->acknowledged = find_old_acknowledged(old, old_count, a->id);
    }

    s_state.alert_count = n;
}

/* ── Lịch sử ─────────────────────────────────────────────────────────────── */
int app_state_search_history(const char *keyword, history_ticket_t *out[], int max_out)
{
    int n = 0;
    bool has_kw = (keyword && keyword[0] != '\0');
    for (int i = 0; i < s_state.history_count && n < max_out; i++) {
        history_ticket_t *h = &s_state.history[i];
        if (!has_kw ||
            strstr(h->order_code, keyword) ||
            strstr(h->customer, keyword) ||
            strstr(h->plate, keyword)) {
            out[n++] = h;
        }
    }
    return n;
}

void app_state_cancel_ticket(const char *receipt_no, const char *reason, const char *actor)
{
    for (int i = 0; i < s_state.history_count; i++) {
        if (strcmp(s_state.history[i].receipt_no, receipt_no) == 0) {
            s_state.history[i].cancelled = true;

            /* trả đơn về PENDING */
            for (int j = 0; j < s_state.order_count; j++) {
                if (strcmp(s_state.orders[j].code, s_state.history[i].order_code) == 0) {
                    s_state.orders[j].status = ORDER_PENDING;
                    s_state.orders[j].synced = false;
                    s_state.orders[j].pending_sync = false;
                    s_state.orders[j].receipt_no[0] = '\0';
                    break;
                }
            }

            if (s_state.audit_log_count < APP_MAX_AUDIT_LOG) {
                audit_log_t *log = &s_state.audit_log[s_state.audit_log_count++];
                /* Sao chep qua buffer rieng tren stack truoc khi gan vao log:
                 * log va history cung nam trong s_state (mot struct global lon),
                 * nen GCC -Wrestrict khong the chung minh hai field khong
                 * chong lan neu truyen thang field-to-field cho snprintf; dung
                 * mot mang tren stack (khong the alias voi s_state) de cat dut
                 * phan tich bi hai con tro nay. */
                char order_code_buf[sizeof(log->order_code)];
                strncpy(order_code_buf, s_state.history[i].order_code, sizeof(order_code_buf) - 1);
                order_code_buf[sizeof(order_code_buf) - 1] = '\0';
                snprintf(log->order_code, sizeof(log->order_code), "%s", order_code_buf);
                snprintf(log->action, sizeof(log->action), "Huỷ phiếu");
                snprintf(log->actor, sizeof(log->actor), "%s", actor);
                snprintf(log->timestamp, sizeof(log->timestamp), "27/09/2026 %s", "10:02:00");
                snprintf(log->reason, sizeof(log->reason), "%s", reason);
            }
            return;
        }
    }
}

/* ── Cài đặt — Wi-Fi thật qua ESP32-C6 (SDIO/esp_hosted) ────────────────────
 * wifi_manager chạy nền (task riêng), API không chặn. app_state chỉ đọc kết
 * quả quét mới nhất (theo "gen") và trạng thái kết nối mỗi tick — xem
 * app_state_wifi_sync(), gọi từ app_state_sim_tick(). */
static uint32_t s_wifi_scan_gen = 0;

void app_state_wifi_scan(void)
{
    wifi_manager_scan_start();
}

bool app_state_wifi_is_scanning(void)
{
    return wifi_manager_is_scanning();
}

static void app_state_wifi_sync(void)
{
    uint32_t gen = 0;
    wifi_ap_info_t aps[APP_MAX_WIFI_NETWORKS];
    int n = wifi_manager_get_scan(aps, APP_MAX_WIFI_NETWORKS, &gen);
    if (gen != s_wifi_scan_gen) {
        s_wifi_scan_gen = gen;
        const char *connected_ssid = wifi_manager_get_ssid();
        bool connected = wifi_manager_is_connected();
        s_state.wifi_network_count = n;
        for (int i = 0; i < n; i++) {
            /* memcpy có giới hạn thay vì snprintf: aps[i].ssid (33 byte) có thể
               dài hơn wifi_networks[].ssid (32 byte) — GCC -Wformat-truncation
               không chấp nhận cắt bớt có chủ đích qua snprintf ở đây. */
            size_t ssid_len = strlen(aps[i].ssid);
            size_t ssid_cap = sizeof(s_state.wifi_networks[i].ssid) - 1;
            if (ssid_len > ssid_cap) ssid_len = ssid_cap;
            memcpy(s_state.wifi_networks[i].ssid, aps[i].ssid, ssid_len);
            s_state.wifi_networks[i].ssid[ssid_len] = '\0';
            /* wifi_manager báo 1..4 vạch (theo RSSI thật); UI hiển thị "/4". */
            s_state.wifi_networks[i].bars = aps[i].bars;
            s_state.wifi_networks[i].dbm = aps[i].rssi;
            s_state.wifi_networks[i].secured = !aps[i].open;
            s_state.wifi_networks[i].selected = connected && strcmp(aps[i].ssid, connected_ssid) == 0;
        }
    }
    s_state.wifi_connected = wifi_manager_is_connected();
    if (s_state.wifi_connected) {
        /* Icon header phản ánh CƯỜNG ĐỘ tín hiệu thật (RSSI), không chỉ
           on/off: 3-4 vạch = tốt, 1-2 vạch = yếu. */
        s_state.wifi_link = (wifi_manager_get_bars() >= 3) ? LINK_OK : LINK_WEAK;
    } else {
        s_state.wifi_link = wifi_manager_is_connecting() ? LINK_WEAK : LINK_LOST;
    }
}

void app_state_wifi_connect(int idx, const char *pass)
{
    if (idx < 0 || idx >= s_state.wifi_network_count) return;
    for (int i = 0; i < s_state.wifi_network_count; i++) s_state.wifi_networks[i].selected = false;
    s_state.wifi_networks[idx].selected = true;
    wifi_manager_connect(s_state.wifi_networks[idx].ssid, pass);
}

void app_state_select_station(int idx)
{
    if (idx < 0 || idx >= s_state.station_count) return;
    s_state.current_station_idx = idx;
}
