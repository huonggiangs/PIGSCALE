/**
 * @file weight_logic.c
 * @brief Công thức tính tải trọng gàu máy xúc từ áp suất + góc nghiêng
 *
 * ════════════════════════════════════════════════════════════════
 *  CÔNG THỨC THỦY LỰC — Moment Balance quanh chốt Boom
 * ════════════════════════════════════════════════════════════════
 *
 * Nguyên lý:
 *   Xy-lanh boom phải tạo đủ moment để nâng toàn bộ:
 *   boom arm + arm cylinder + arm assembly + bucket + payload
 *
 *   τ_cyl = τ_boom_weight + τ_arm_weight + τ_bucket_weight + τ_payload
 *
 *   → τ_payload = τ_cyl - τ_tare
 *   → W_payload = τ_payload / r_bucket_horizontal
 *
 * ────────────────────────────────────────────────────────────────
 *  Bước 1: Lực xy-lanh boom (từ áp suất P1, P2)
 * ────────────────────────────────────────────────────────────────
 *
 *   F_cyl = P1_bore × A_bore - P2_rod × A_rod          [N]
 *
 *   A_bore = π/4 × D_bore²                              [m²]
 *   A_rod  = π/4 × (D_bore² - D_rod²)                  [m²]
 *
 *   1 bar = 100,000 Pa → F [N] = P [bar] × 100000 × A [m²]
 *
 * ────────────────────────────────────────────────────────────────
 *  Bước 2: Moment của xy-lanh boom quanh chốt boom (O_boom)
 * ────────────────────────────────────────────────────────────────
 *
 *   Cánh tay đòn của xy-lanh thay đổi theo góc boom θ_boom:
 *
 *   Dùng phép tính vector (pin geometry):
 *     - A = vị trí chân xy-lanh (fixed, trên khung máy)
 *     - B = vị trí đầu piston (trên boom, thay đổi theo θ_boom)
 *     - O = chốt boom pivot
 *
 *   moment_arm = |OA × AB| / |AB|   (cross product → perpendicular distance)
 *
 *   Xấp xỉ thực tế (đã linearize):
 *   r_cyl(θ) = L_cyl_moment × cos(θ_boom - θ_cyl_offset)
 *
 *   τ_cyl = F_cyl × r_cyl(θ_boom)
 *
 * ────────────────────────────────────────────────────────────────
 *  Bước 3: Moment tare (cấu trúc rỗng) theo góc
 * ────────────────────────────────────────────────────────────────
 *
 *   Khi gàu rỗng, toàn bộ moment do boom cylinder phải cân bằng
 *   với trọng lượng cấu trúc (boom arm + arm + bucket):
 *
 *   τ_tare(θ_boom, θ_arm) = τ_cyl_empty
 *
 *   Trong thực tế: đo τ_cyl ở nhiều góc khác nhau khi gàu rỗng
 *   → xây bảng lookup hoặc đa thức hồi quy
 *
 *   Xấp xỉ polynomial bậc 2:
 *   τ_tare(θ) = C0 + C1×θ_boom + C2×θ_boom² + C3×θ_arm + C4×θ_arm²
 *
 * ────────────────────────────────────────────────────────────────
 *  Bước 4: Tải trọng payload
 * ────────────────────────────────────────────────────────────────
 *
 *   τ_payload = τ_cyl_loaded - τ_tare(θ_boom, θ_arm)
 *
 *   Khoảng cách ngang từ chốt boom đến gàu:
 *   r_bucket = L_boom × cos(θ_boom)
 *            + L_arm  × cos(θ_boom + θ_arm)
 *            + L_bucket_cg × cos(θ_boom + θ_arm + θ_bucket)
 *
 *   W_payload = τ_payload / r_bucket    [N] → ÷ g → [kg]
 *
 * ────────────────────────────────────────────────────────────────
 *  Bù nghiêng máy (body roll)
 * ────────────────────────────────────────────────────────────────
 *
 *   Khi máy nghiêng góc φ (body roll), lực trọng trường chiếu:
 *   W_corrected = W_payload / cos(φ)
 *
 * ════════════════════════════════════════════════════════════════
 *  THÔNG SỐ CẦN CALIBRATE (đo trên máy thực tế)
 * ════════════════════════════════════════════════════════════════
 *
 *  Cylinder:
 *    D_BORE_BOOM   = đường kính lòng xy-lanh boom (m)
 *    D_ROD_BOOM    = đường kính cần piston boom (m)
 *
 *  Geometry (khoảng cách dọc cơ cấu):
 *    L_BOOM        = chiều dài boom arm (m) — từ chốt boom đến chốt arm
 *    L_ARM         = chiều dài arm (m) — từ chốt arm đến chốt bucket
 *    L_BUCKET_CG   = khoảng cách từ chốt bucket đến tâm gàu (m)
 *    L_CYL_MOMENT  = cánh tay đòn max của xy-lanh boom (m)
 *    THETA_CYL_OFF = góc offset của xy-lanh so với boom (rad)
 *
 *  Tare polynomial (đo khi gàu rỗng, nhiều góc):
 *    TARE_C0..C4   = hệ số đa thức
 */

#include "weight_logic.h"
#include "ads131m08.h"
#include "angle_sensor.h"
#include "ui_main.h"
#include "display_driver.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <string.h>

static const char *TAG = "WEIGHT_LOGIC";

/* ============================================================
 * THÔNG SỐ MÁY — THAY ĐỔI THEO MÁY THỰC TẾ
 * (Đây là giá trị ước tính cho máy xúc 20-25 tấn phổ biến)
 * ============================================================ */

/* Xy-lanh boom (mét) */
#define D_BORE_BOOM     0.140f   /* 140mm bore */
#define D_ROD_BOOM      0.095f   /* 95mm rod */

/* Xy-lanh arm (dùng P3, P4) — bổ sung nếu cần */
#define D_BORE_ARM      0.120f
#define D_ROD_ARM       0.080f

/* Geometry cơ cấu (mét) */
#define L_BOOM          5.50f    /* boom length */
#define L_ARM           2.90f    /* arm length */
#define L_BUCKET_CG     0.80f    /* boom length đến tâm gàu */
#define L_CYL_MOMENT    1.20f    /* cánh tay đòn boom cylinder (max) */
#define THETA_CYL_OFF   0.52f    /* ~30° offset (rad) */

/* Trọng lượng cấu trúc (kg) */
#define W_BOOM_KG      1800.0f   /* trọng lượng boom arm */
#define W_ARM_KG        900.0f   /* trọng lượng arm */
#define W_BUCKET_KG     650.0f   /* trọng lượng gàu rỗng */

/* Center of gravity (tỷ lệ chiều dài) */
#define CG_BOOM         0.45f    /* tâm KL boom tại 45% từ chốt */
#define CG_ARM          0.42f    /* tâm KL arm tại 42% từ chốt */

/* Tare polynomial (C0..C4) — calibrate thực tế */
/* Giá trị ban đầu = 0, sẽ được ghi sau khi chạy tare */
#define NVS_NS          "weight_cal"
#define NVS_KEY_TARE    "tare_poly"
#define NVS_KEY_SCALE   "scale_factor"

#define G_GRAVITY       9.81f

/* Diện tích tiết diện (m²) */
#define A_BORE_BOOM  (M_PI / 4.0f * D_BORE_BOOM * D_BORE_BOOM)
#define A_ROD_BOOM   (M_PI / 4.0f * (D_BORE_BOOM*D_BORE_BOOM - D_ROD_BOOM*D_ROD_BOOM))
#define A_BORE_ARM   (M_PI / 4.0f * D_BORE_ARM  * D_BORE_ARM)
#define A_ROD_ARM    (M_PI / 4.0f * (D_BORE_ARM *D_BORE_ARM  - D_ROD_ARM *D_ROD_ARM))

/* Số mẫu tính trung bình mỗi lần cân */
#define AVG_SAMPLES     10
#define TASK_PERIOD_MS  100

/* ============================================================
 * State
 * ============================================================ */
static volatile weight_state_t s_state = WEIGHT_STATE_IDLE;
static volatile float s_payload_kg    = 0.0f;
static volatile float s_total_kg      = 0.0f;
static volatile float s_target_kg     = 25000.0f;

/* Tare polynomial coefficients [C0, C1_boom, C2_boom2, C3_arm, C4_arm2] */
static float s_tare_poly[5] = {0, 0, 0, 0, 0};
/* Global scale factor (offset từ quả cân chuẩn) */
static float s_scale = 1.0f;

/* Dữ liệu đầy đủ cho UI */
static weight_data_t s_data;
static SemaphoreHandle_t s_data_mutex = NULL;

/* Buffer tare */
#define TARE_SAMPLES 30
static float s_tare_buf[TARE_SAMPLES];
static int   s_tare_idx = 0;
static bool  s_tare_collecting = false;

/* ============================================================
 * Hàm tính lực xy-lanh (N)
 * ============================================================ */
static float calc_cylinder_force(float p_bore_bar, float p_rod_bar,
                                  float a_bore, float a_rod)
{
    /* F = P_bore × A_bore − P_rod × A_rod (1 bar = 100000 Pa) */
    return (p_bore_bar * 100000.0f * a_bore)
         - (p_rod_bar  * 100000.0f * a_rod);
}

/* ============================================================
 * Cánh tay đòn xy-lanh boom theo góc θ_boom (rad)
 * r_cyl(θ) = L_CYL_MOMENT × cos(θ − THETA_CYL_OFF)
 * ============================================================ */
static float boom_cyl_moment_arm(float theta_boom_rad)
{
    float arm = L_CYL_MOMENT * cosf(theta_boom_rad - THETA_CYL_OFF);
    if (arm < 0.05f) arm = 0.05f;  /* tránh chia cho 0 */
    return arm;
}

/* ============================================================
 * Khoảng cách ngang từ chốt boom đến tâm tải (m)
 * Dùng để quy đổi moment → lực (kg)
 * ============================================================ */
static float bucket_horizontal_reach(float theta_boom_rad,
                                     float theta_arm_rad,
                                     float theta_bucket_rad)
{
    float r = L_BOOM   * cosf(theta_boom_rad)
            + L_ARM    * cosf(theta_boom_rad + theta_arm_rad)
            + L_BUCKET_CG * cosf(theta_boom_rad + theta_arm_rad + theta_bucket_rad);
    if (r < 0.3f) r = 0.3f;
    return r;
}

/* ============================================================
 * Moment tare (cấu trúc rỗng) quanh chốt boom
 * Dùng đa thức + trọng lượng tĩnh đã biết
 * ============================================================ */
static float calc_tare_torque(float theta_boom_rad, float theta_arm_rad,
                               float theta_bucket_rad)
{
    /* 1. Moment do trọng lượng cấu trúc (tính từ geometry) */
    float r_boom_cg = L_BOOM * CG_BOOM * cosf(theta_boom_rad);
    float r_arm_cg  = L_BOOM * cosf(theta_boom_rad)
                    + L_ARM  * CG_ARM * cosf(theta_boom_rad + theta_arm_rad);
    float r_bucket  = bucket_horizontal_reach(theta_boom_rad, theta_arm_rad,
                                              theta_bucket_rad);

    float tau_struct = (W_BOOM_KG   * G_GRAVITY * r_boom_cg)
                     + (W_ARM_KG    * G_GRAVITY * r_arm_cg)
                     + (W_BUCKET_KG * G_GRAVITY * r_bucket);

    /* 2. Correction polynomial từ calibration */
    float boom_deg = theta_boom_rad * 180.0f / (float)M_PI;
    float arm_deg  = theta_arm_rad  * 180.0f / (float)M_PI;
    float tau_corr = s_tare_poly[0]
                   + s_tare_poly[1] * boom_deg
                   + s_tare_poly[2] * boom_deg * boom_deg
                   + s_tare_poly[3] * arm_deg
                   + s_tare_poly[4] * arm_deg  * arm_deg;

    return tau_struct + tau_corr;
}

/* ============================================================
 *  CÔNG THỨC CHÍNH — tính tải trọng gàu (kg)
 * ============================================================ */
static float calculate_payload(const ads_reading_t *adc,
                                float theta_boom_deg,
                                float theta_arm_deg,
                                float theta_bucket_deg,
                                float body_roll_deg)
{
    /* Chuyển sang radian */
    float tb = theta_boom_deg   * (float)M_PI / 180.0f;
    float ta = theta_arm_deg    * (float)M_PI / 180.0f;
    float tk = theta_bucket_deg * (float)M_PI / 180.0f;
    float roll = body_roll_deg  * (float)M_PI / 180.0f;

    /* --- Bước 1: Lực xy-lanh boom --- */
    float F_boom = calc_cylinder_force(
        adc->pressure_bar[ADS_CH_P1_BOOM_BORE],
        adc->pressure_bar[ADS_CH_P2_BOOM_ROD],
        A_BORE_BOOM, A_ROD_BOOM);

    if (F_boom < 0) F_boom = 0;

    /* --- Bước 2: Moment xy-lanh boom quanh chốt --- */
    float r_cyl = boom_cyl_moment_arm(tb);
    float tau_cyl = F_boom * r_cyl;

    /* --- Bước 3: Trừ moment tare --- */
    float tau_tare   = calc_tare_torque(tb, ta, tk);
    float tau_payload = tau_cyl - tau_tare;

    if (tau_payload < 0) tau_payload = 0;

    /* --- Bước 4: Quy moment → kg --- */
    float r_bucket = bucket_horizontal_reach(tb, ta, tk);
    float weight_n = tau_payload / r_bucket;    /* Newton */
    float weight_kg = weight_n / G_GRAVITY;     /* kg */

    /* --- Bước 5: Bù nghiêng máy --- */
    if (fabsf(body_roll_deg) > 0.5f) {
        float cos_roll = cosf(roll);
        if (cos_roll > 0.1f) weight_kg /= cos_roll;
    }

    /* --- Bước 6: Scale factor từ calibration --- */
    weight_kg *= s_scale;

    if (weight_kg < 0) weight_kg = 0;
    return weight_kg;
}

/* ============================================================
 * Stable averaging — lọc nhiễu, chỉ lấy khi ổn định
 * ============================================================ */
#define STABLE_WINDOW    5
#define STABLE_THRESH_KG 50.0f  /* biến động < 50kg/100ms = ổn định */

static float s_avg_buf[STABLE_WINDOW];
static int   s_avg_head = 0;
static float s_avg_sum  = 0;
static bool  s_buf_full = false;

static float moving_avg(float new_val)
{
    s_avg_sum -= s_avg_buf[s_avg_head];
    s_avg_buf[s_avg_head] = new_val;
    s_avg_sum += new_val;
    s_avg_head = (s_avg_head + 1) % STABLE_WINDOW;
    if (!s_buf_full && s_avg_head == 0) s_buf_full = true;
    int n = s_buf_full ? STABLE_WINDOW : s_avg_head;
    return (n > 0) ? (s_avg_sum / n) : new_val;
}

/* ============================================================
 * FreeRTOS task — đọc sensor + tính tải + cập nhật UI
 * ============================================================ */
static void weight_task(void *arg)
{
    (void)arg;
    ads_reading_t adc;
    weight_data_t snap;
    float prev_payload = 0;
    int dump_stable_cnt = 0;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(TASK_PERIOD_MS));

        /* Đọc ADC */
        bool adc_ok = (ads131m08_read(&adc) == ESP_OK);

        /* Đọc góc */
        float boom_pitch   = angle_get_boom_pitch();
        float arm_pitch    = angle_get_arm_pitch();
        float bucket_pitch = angle_get_bucket_pitch();
        float body_roll    = angle_get_body_roll();

        /* Tính tải */
        float payload = 0;
        if (adc_ok) {
            payload = calculate_payload(&adc, boom_pitch, arm_pitch,
                                         bucket_pitch, body_roll);
            payload = moving_avg(payload);
        }
        s_payload_kg = payload;

        /* State machine — phát hiện dump (gàu hạ xuống đổ tải) */
        if (s_state == WEIGHT_STATE_WEIGHING) {
            /* Phát hiện dump: tải giảm đột ngột > 200kg */
            float delta = prev_payload - payload;
            if (payload < 100.0f && delta > 200.0f) {
                dump_stable_cnt++;
                if (dump_stable_cnt >= 3) {
                    s_total_kg += prev_payload;
                    ESP_LOGI(TAG, "DUMP detected: +%.0f kg, total=%.0f",
                             prev_payload, s_total_kg);
                    dump_stable_cnt = 0;
                    if (s_total_kg >= s_target_kg) {
                        s_state = WEIGHT_STATE_COMPLETE;
                    }
                }
            } else {
                dump_stable_cnt = 0;
            }

            /* Tare collection */
            if (s_tare_collecting && payload < 50.0f && adc_ok) {
                s_tare_buf[s_tare_idx++] = payload;
                if (s_tare_idx >= TARE_SAMPLES) {
                    s_tare_collecting = false;
                    /* Tính mean tare error */
                    float mean = 0;
                    for (int i = 0; i < TARE_SAMPLES; i++) mean += s_tare_buf[i];
                    mean /= TARE_SAMPLES;
                    s_tare_poly[0] -= mean * G_GRAVITY * 1.0f; /* adjust C0 */
                    ESP_LOGI(TAG, "Tare done, offset=%.1f kg", mean);
                }
            }
        }
        prev_payload = payload;

        /* Update shared data */
        xSemaphoreTake(s_data_mutex, portMAX_DELAY);
        snap.payload_kg  = payload;
        snap.total_kg    = s_total_kg;
        snap.target_kg   = s_target_kg;
        snap.boom_angle  = boom_pitch;
        snap.arm_angle   = arm_pitch;
        snap.body_roll   = body_roll;
        snap.state       = s_state;
        for (int i = 0; i < 6; i++)
            snap.sensor_ok[i] = adc_ok && !adc.wire_break[i];
        for (int i = 0; i < 4; i++) {
            angle_data_t a; angle_sensor_get(i, &a);
            snap.angle_ok[i] = a.connected;
        }
        s_data = snap;
        xSemaphoreGive(s_data_mutex);

        /* Update UI (LVGL thread-safe) */
        lvgl_lock();
        ui_main_update_weight(payload, s_total_kg, s_target_kg);
        lvgl_unlock();
    }
}

/* ============================================================
 * Calibration — NVS
 * ============================================================ */
void weight_logic_save_calibration(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_blob(h, NVS_KEY_TARE,  s_tare_poly, sizeof(s_tare_poly));
    nvs_set_blob(h, NVS_KEY_SCALE, &s_scale,    sizeof(s_scale));
    nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "Calibration saved to NVS");
}

void weight_logic_load_calibration(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return;
    size_t sz = sizeof(s_tare_poly);
    nvs_get_blob(h, NVS_KEY_TARE, s_tare_poly, &sz);
    sz = sizeof(s_scale);
    nvs_get_blob(h, NVS_KEY_SCALE, &s_scale, &sz);
    nvs_close(h);
    ESP_LOGI(TAG, "Calibration loaded: scale=%.4f, C0=%.1f", s_scale, s_tare_poly[0]);
}

/* ============================================================
 * Public API
 * ============================================================ */
void weight_logic_init(void)
{
    s_data_mutex = xSemaphoreCreateMutex();
    memset(&s_data, 0, sizeof(s_data));
    memset(s_avg_buf, 0, sizeof(s_avg_buf));

    ads131m08_init();
    angle_sensor_init();
    weight_logic_load_calibration();

    xTaskCreate(weight_task, "weight_task", 6144, NULL, 5, NULL);
    ESP_LOGI(TAG, "Weight logic started (target=%.0f kg)", s_target_kg);
    ESP_LOGI(TAG, "A_bore_boom=%.4f m², A_rod_boom=%.4f m²",
             (float)A_BORE_BOOM, (float)A_ROD_BOOM);
}

void weight_logic_start(void)
{
    if (s_state == WEIGHT_STATE_IDLE) {
        s_state = WEIGHT_STATE_WEIGHING;
        s_avg_head = 0; s_avg_sum = 0; s_buf_full = false;
        ESP_LOGI(TAG, "START weighing");
    }
}

void weight_logic_stop(void)
{
    if (s_state == WEIGHT_STATE_WEIGHING) {
        s_state = WEIGHT_STATE_IDLE;
        ESP_LOGI(TAG, "STOP");
    }
}

void weight_logic_reset(void)
{
    s_state    = WEIGHT_STATE_IDLE;
    s_total_kg = 0;
    ESP_LOGI(TAG, "RESET");
}

void weight_logic_tare(void)
{
    s_tare_idx        = 0;
    s_tare_collecting = true;
    ESP_LOGI(TAG, "Tare collecting...");
}

bool weight_logic_is_running(void)
{
    return s_state == WEIGHT_STATE_WEIGHING;
}

weight_state_t weight_logic_get_state(void) { return s_state; }

void weight_logic_get_data(weight_data_t *out)
{
    xSemaphoreTake(s_data_mutex, portMAX_DELAY);
    *out = s_data;
    xSemaphoreGive(s_data_mutex);
}

void weight_logic_set_target(float t) { s_target_kg = t; }
