#pragma once
/**
 * @file app_state.h
 * @brief Mô hình dữ liệu + trạng thái ứng dụng cho "Pig Weigh".
 *
 * Đây là lớp tách biệt UI khỏi dữ liệu thật. Ở PHIÊN BẢN NÀY (chỉ dựng
 * giao diện theo esp32p4-lvgl-handoff.md), toàn bộ dữ liệu là MÔ PHỎNG
 * (mock/demo) — KHÔNG có kết nối P5 Scale/Camera/Server thật. Các hàm
 * app_state_* mô phỏng hành vi hệ thống thật để UI có thể thao tác/điều
 * hướng đầy đủ. Khi tích hợp phần cứng/mạng thật, chỉ cần thay phần triển
 * khai bên trong app_state.c — các hàm UI gọi vào layer này không cần đổi.
 *
 * Xem thêm: luongcan.md (luồng nghiệp vụ), danh-gia-thuong-mai.md (mục đã
 * hoàn thành / còn thiếu).
 */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Hằng số kích thước mảng (đủ dùng cho demo trên thiết bị) ───────────── */
#define APP_MAX_EMPLOYEES     3
#define APP_MAX_ORDERS        14
#define APP_MAX_ALERTS        8
#define APP_MAX_HISTORY       20
#define APP_MAX_AUDIT_LOG     10
#define APP_MAX_WIFI_NETWORKS 5
#define APP_MAX_STATIONS      3
#define APP_ORDERS_PER_PAGE   5

/* ── Vai trò nhân viên ───────────────────────────────────────────────────── */
typedef enum {
    ROLE_OPERATOR = 0,   /* Nhân viên cân */
    ROLE_MANAGER,        /* Quản lý */
    ROLE_TECHNICIAN,     /* Kỹ thuật */
} user_role_t;

const char *app_role_label(user_role_t role);

typedef struct {
    char code[16];        /* NV001 */
    char name[48];         /* Nguyễn Văn A */
    user_role_t role;
    char pin[8];           /* 4 số */
} employee_t;

/* ── Đơn hàng ────────────────────────────────────────────────────────────── */
typedef enum {
    TICKET_IMPORT = 0,   /* Nhập */
    TICKET_EXPORT,       /* Xuất */
} ticket_type_t;

typedef enum {
    ORDER_PENDING = 0,   /* Chờ cân */
    ORDER_WEIGHING,      /* Đang cân */
    ORDER_DONE,          /* Hoàn tất */
} order_status_t;

typedef struct {
    char id[16];
    char code[24];          /* DH-260927-001 */
    char customer[64];
    int  planned_qty;
    char plate[24];
    ticket_type_t  ticket_type;
    order_status_t status;
    bool synced;             /* đã đồng bộ lên server */
    bool pending_sync;       /* hoàn tất nhưng đang chờ đồng bộ (mất mạng lúc xác nhận) */
    bool is_manual_entry;    /* phiếu tạo từ nhập tay */
    char receipt_no[24];     /* mã phiếu cân sau khi xác nhận */
    int   actual_qty;
    float actual_weight_kg;
} order_t;

/* ── Trạng thái đối chiếu khi cân ───────────────────────────────────────── */
typedef enum {
    RECONCILE_NONE = 0,
    RECONCILE_MATCH,        /* KHỚP */
    RECONCILE_MISMATCH,     /* LỆCH */
    RECONCILE_UNSURE,       /* CHƯA CHẮC */
    RECONCILE_INVALID,      /* KHÔNG HỢP LỆ */
    RECONCILE_MANUAL,       /* NHẬP THỦ CÔNG */
} reconcile_status_t;

typedef enum {
    P5_STATE_STABLE = 0,    /* ỔN ĐỊNH */
    P5_STATE_MOVING,        /* ĐANG ĐỘNG */
    P5_STATE_LOST,          /* MẤT KẾT NỐI P5 SCALE */
    P5_STATE_MANUAL,        /* NHẬP THỦ CÔNG */
} p5_state_t;

typedef struct {
    bool   active;              /* đang có phiên cân hay chưa (đã chọn đơn) */
    char   order_id[16];
    ticket_type_t ticket_type;  /* loại phiếu — có thể đổi trong lúc cân */

    float  weight_kg;
    p5_state_t p5_state;

    bool   has_line_count;
    int    line_count;          /* số con qua vạch */
    bool   has_snapshot_count;
    int    snapshot_count;      /* số con ảnh tĩnh */

    bool   camera_connected;
    reconcile_status_t reconcile;

    bool   manual_mode;         /* đang ở chế độ nhập tay (mất P5/Camera) */
    float  manual_weight_kg;
    int    manual_qty;
    bool   manual_filled;       /* đã nhập đủ 2 giá trị tay chưa */
} weighing_session_t;

/* ── Cảnh báo / đề xuất ──────────────────────────────────────────────────── */
typedef enum {
    ALERT_INFO = 0,
    ALERT_WARNING,
    ALERT_CRITICAL,
} alert_level_t;

typedef struct {
    char id[16];
    alert_level_t level;
    char title[96];
    char desc[200];
    char action[160];   /* bắt đầu bằng "→ " khi hiển thị */
    bool acknowledged;
    bool active;         /* còn hiệu lực hay đã bị thu hồi vì tình huống hết */
} alert_t;

/* ── Lịch sử phiếu cân ───────────────────────────────────────────────────── */
typedef struct {
    char receipt_no[24];
    char order_code[24];
    char customer[64];
    char plate[24];
    ticket_type_t type;
    int   actual_qty;
    float actual_weight_kg;
    bool  is_manual;
    bool  cancelled;
} history_ticket_t;

typedef struct {
    char order_code[24];
    char action[40];      /* "Huỷ phiếu", "Sửa phiếu" ... */
    char actor[64];
    char timestamp[24];   /* dd/mm/yyyy HH:MM:SS */
    char reason[200];
} audit_log_t;

/* ── Wi-Fi (Cài đặt) ─────────────────────────────────────────────────────── */
typedef struct {
    char ssid[32];
    int  bars;          /* 1-4 (theo RSSI thật từ wifi_manager) */
    int  dbm;
    bool secured;
    bool selected;
} wifi_network_t;

/* ── Trạm cân ────────────────────────────────────────────────────────────── */
typedef struct {
    char id[8];
    char name[32];
} station_t;

/* ── Máy in ──────────────────────────────────────────────────────────────── */
typedef enum {
    PRINTER_THERMAL_58 = 0,
    PRINTER_A5,
} printer_type_t;

/* ── Trạng thái thiết bị ngoại vi (mô phỏng) ─────────────────────────────── */
typedef enum {
    LINK_OK = 0,     /* xanh */
    LINK_WEAK,       /* cam  */
    LINK_LOST,       /* đỏ   */
} link_state_t;

typedef enum {
    TAB_ORDERS = 0,
    TAB_WEIGHING,
    TAB_SUGGESTIONS,
    TAB_HISTORY,
    TAB_SETTINGS,
    TAB_COUNT,
} app_tab_t;

typedef enum {
    ORDER_FILTER_ALL = 0,
    ORDER_FILTER_PENDING,
    ORDER_FILTER_WEIGHING,
    ORDER_FILTER_DONE,
} order_filter_t;

/* ── Toàn bộ trạng thái ứng dụng ─────────────────────────────────────────── */
typedef struct {
    /* đăng nhập */
    bool logged_in;
    int  current_employee_idx;    /* -1 nếu chưa chọn nhân viên trên màn login */
    char pin_input[8];

    /* điều hướng */
    app_tab_t current_tab;
    bool settings_unlocked;       /* đã mở khoá Cài đặt (cho operator) trong phiên này */

    /* dữ liệu tĩnh / demo */
    employee_t employees[APP_MAX_EMPLOYEES];
    int employee_count;

    order_t orders[APP_MAX_ORDERS];
    int order_count;
    order_filter_t order_filter;
    int order_page;               /* 0-based */

    weighing_session_t weighing;

    alert_t alerts[APP_MAX_ALERTS];
    int alert_count;

    history_ticket_t history[APP_MAX_HISTORY];
    int history_count;
    audit_log_t audit_log[APP_MAX_AUDIT_LOG];
    int audit_log_count;

    wifi_network_t wifi_networks[APP_MAX_WIFI_NETWORKS];
    int wifi_network_count;
    bool wifi_connected;

    station_t stations[APP_MAX_STATIONS];
    int station_count;
    int current_station_idx;

    printer_type_t printer_type;
    bool bright_mode;             /* Chế độ ánh sáng mạnh */

    /* liên kết thiết bị (mô phỏng) */
    link_state_t wifi_link;
    link_state_t p5_link;
    link_state_t camera_link;

    int pending_sync_count;       /* số phiếu đang chờ đồng bộ */

    char fw_version[24];
    char fw_build[24];
} app_state_t;

/* Truy cập trạng thái toàn cục (singleton) */
app_state_t *app_state(void);

/* Khởi tạo dữ liệu demo — gọi 1 lần khi khởi động */
void app_state_init(void);

/* ── Đăng nhập / phân quyền ──────────────────────────────────────────────── */
void app_state_select_employee(int idx);
void app_state_pin_digit(char digit);
void app_state_pin_backspace(void);
void app_state_pin_clear(void);
/* Trả về true nếu đăng nhập thành công (PIN đủ 4 số & đúng) */
bool app_state_try_login(void);
void app_state_logout(void);

/* PIN gate riêng cho tab Cài đặt (operator) — dùng chung state pin_input */
bool app_state_try_settings_unlock(void);

/* ── Đơn hàng ────────────────────────────────────────────────────────────── */
/* Lọc theo order_filter hiện tại + phân trang, trả về mảng con trỏ & số lượng trên trang */
int app_state_get_filtered_orders(order_t *out[], int max_out);
order_t *app_state_find_order(const char *order_id);

/* ── Cân ─────────────────────────────────────────────────────────────────── */
void app_state_start_weighing(const char *order_id);
void app_state_weighing_set_ticket_type(ticket_type_t t);
void app_state_weighing_manual_input(float weight_kg, int qty);
void app_state_weighing_reweigh(void);       /* "Cân lại" */
bool app_state_weighing_can_confirm(void);
void app_state_weighing_confirm(void);       /* chốt phiếu cân */
void app_state_sync_now(void);               /* "Đồng bộ lại" toàn bộ hàng đợi */

/* Tick mô phỏng — gọi định kỳ (vd. mỗi 200-500ms) từ 1 lv_timer để số liệu
 * "sống" trên demo (KHÔNG phải dữ liệu thật). */
void app_state_sim_tick(void);

/* ── Đề xuất / cảnh báo ──────────────────────────────────────────────────── */
void app_state_ack_alert(const char *alert_id);
void app_state_refresh_alerts(void);   /* tính lại danh sách theo trạng thái thiết bị */

/* ── Lịch sử ─────────────────────────────────────────────────────────────── */
int app_state_search_history(const char *keyword, history_ticket_t *out[], int max_out);
void app_state_cancel_ticket(const char *receipt_no, const char *reason, const char *actor);

/* ── Cài đặt ─────────────────────────────────────────────────────────────── */
void app_state_wifi_scan(void);   /* wifi_manager_scan_start() thật qua ESP32-C6 */
bool app_state_wifi_is_scanning(void);   /* để UI hiện "Đang quét..." + khoá nút */
void app_state_wifi_connect(int idx, const char *pass);
void app_state_select_station(int idx);

/* ── Thời gian hệ thống (GMT+7 Việt Nam — xem main.c setenv TZ) ──────────── */
bool app_state_time_is_synced(void);   /* đã có mốc giờ từ NTP lần nào chưa */
void app_state_time_force_sync(void);  /* ép đồng bộ lại qua NTP (cần có WiFi) */

#ifdef __cplusplus
}
#endif
