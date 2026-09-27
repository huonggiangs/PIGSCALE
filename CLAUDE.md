# CÂN MÁY XÚC V3 — CLAUDE.md

## Mô tả dự án
Hệ thống **cân gàu máy xúc** (excavator bucket scale) hiển thị trên màn hình nhúng ESP32-P4.  
Giao diện dark-theme công nghiệp, ngôn ngữ tiếng Việt.

---

## Hardware Target

| Thành phần | Thông số |
|---|---|
| MCU chính (UI + logic) | ESP32-P4 (dual-core Xtensa LX7 @ 400 MHz) |
| MCU mạng | ESP32-C6 (Wi-Fi 6 / BLE — tích hợp trên mạch) |
| Board | ESP32-P4-WIFI6-POE-ETH |
| Display | 800×1280 portrait, MIPI DSI hoặc RGB |
| RAM | 32 MB PSRAM (octal) |
| Flash | 16 MB |
| Framework | **ESP-IDF v5.4+** |
| UI Library | **LVGL v9.x** |

### Cảm biến & Ngoại vi

| Module | Giao tiếp | Ghi chú |
|---|---|---|
| Cảm biến áp suất (×6 GẦU, ×2 LẬT) | ADC — GPIO34/35/36/39... | 0–5 V → voltage divider → 12-bit ADC |
| Cảm biến góc nghiêng (IMU) | I²C — địa chỉ cấu hình sẵn | VD: MPU-6050 @ 0x68, BNO055 @ 0x28 |
| USB Mass Storage (xuất báo cáo) | USB OTG FS — GPIO19/20 | ESP32-P4 hỗ trợ USB MSC host |
| Ethernet (PoE) | RMII — PHY IP101 | `esp_eth_mac_new_esp32` |

---

## Mục tiêu tích hợp phần cứng

### 1. Đọc ADC — Cảm biến áp suất
```c
// main/sensor/pressure_sensor.c
// - Đọc ADC đa kênh (ADC1_CHANNEL_x) bằng esp_adc
// - Lọc nhiễu: trung bình 16 mẫu (rolling average)
// - Chuyển đổi: raw ADC → điện áp → bar (dùng hệ số calibration)
// - Ghi nhận: lưu vào ring buffer, cập nhật LVGL label mỗi 100ms
// - Cấu trúc: pressure_reading_t { channel, raw, voltage, bar, timestamp }
```

### 2. Kết nối Wi-Fi qua ESP32-C6
```c
// main/network/wifi_manager.c  (đã có khung — cần hoàn thiện)
// - Dùng esp_wifi API, event loop FreeRTOS
// - Lưu SSID/password trong NVS
// - Auto-reconnect với exponential backoff
// - Báo trạng thái lên UI qua event queue: WIFI_CONNECTED / WIFI_DISCONNECTED
// - ESP32-C6 giao tiếp với P4 qua UART hoặc SPI (bridge)
```

### 3. Kết nối cảm biến góc (I²C)
```c
// main/sensor/angle_sensor.c
// - I²C master init: i2c_master_bus_create()
// - Địa chỉ cấu hình qua sdkconfig: CONFIG_ANGLE_SENSOR_ADDR (default 0x68)
// - Đọc pitch/roll từ register — cập nhật mỗi 50ms
// - Cấu trúc: angle_reading_t { pitch_deg, roll_deg, timestamp }
```

### 4. USB MSC — Xuất báo cáo vào USB
```c
// main/usb/usb_storage.c
// - Dùng esp_tinyusb + tinyusb MSC host (USB OTG FS)
// - Phát hiện cắm USB → mount FAT filesystem (FATFS)
// - Liệt kê thư mục gốc → UI hiển thị danh sách thư mục để chọn
// - Ghi file: report_YYYYMMDD_HHMMSS.csv vào thư mục đã chọn
// - Format CSV: timestamp, order_id, company, material, weight_kg, target_kg
```

---

## Design System

### Màu sắc
```c
#define COLOR_BG          0x1A1A1A   // nền chính
#define COLOR_CARD        0x242424   // nền card/panel
#define COLOR_ACCENT      0xF5C800   // vàng chủ đạo
#define COLOR_TEXT_PRI    0xFFFFFF   // chữ trắng chính
#define COLOR_TEXT_SEC    0x888888   // chữ xám phụ
#define COLOR_BORDER      0x333333   // border/divider
#define COLOR_OK          0x22C55E   // xanh lá (OK / SẴN SÀNG)
#define COLOR_WARN        0xEAB308   // vàng cảnh báo
#define COLOR_ERR         0xEF4444   // đỏ lỗi
```

### Font (LVGL)
- Display lớn: `lv_font_montserrat_48` hoặc `font_gilroy_80.c` (custom compile)
- Header: 20px bold uppercase
- Label: 12–14px uppercase, letter-spacing
- Cần compile font với Unicode Vietnamese Extended (ắ, ổ, ử…) bằng `lv_font_conv`

### Layout (800×1280 portrait)
```
┌─────────────────────────────┐  h=96   Header (giờ, tiêu đề, icons)
├─────────────────────────────┤  ~130   Order Card (compact)
├─────────────────────────────┤  ~490   Weight Display (số lớn + progress)
├─────────────────────────────┤  ~192   Stats Row (tổng / mục tiêu)
├─────────────────────────────┤  ~100   CTA Button "BẮT ĐẦU CÂN"
└─────────────────────────────┘  h=128  Bottom Navigation (5 tabs)
```

---

## Cấu trúc thư mục (đầy đủ)

```
V3/
├── CLAUDE.md
├── CMakeLists.txt
├── sdkconfig.defaults
├── partitions.csv             ← ota_0, ota_1, nvs, storage(FAT)
├── main/
│   ├── CMakeLists.txt
│   ├── main.c                 ← app_main: init display, sensors, network, USB, LVGL tasks
│   │
│   ├── display/
│   │   ├── display_driver.c   ← init MIPI/RGB panel, LVGL flush callback
│   │   └── display_driver.h
│   │
│   ├── ui/
│   │   ├── ui_theme.h         ← màu, font, lv_style_t constants
│   │   ├── ui_main.c/h        ← CÂN (state machine IDLE→WEIGHING→COMPLETE)
│   │   ├── ui_orders.c/h      ← ĐƠN HÀNG (danh sách, tạo mới)
│   │   ├── ui_history.c/h     ← LỊCH SỬ
│   │   ├── ui_report.c/h      ← BÁO CÁO (chart, USB export modal)
│   │   ├── ui_settings.c/h    ← CÀI ĐẶT (menu + PIN gate)
│   │   ├── ui_calibration.c/h ← HIỆU CHUẨN (PIN, machine tabs, sensor matrix)
│   │   ├── ui_config.c/h      ← THIẾT LẬP THIẾT BỊ (brightness, volume, time, timezone)
│   │   ├── ui_network.c/h     ← CÀI ĐẶT MẠNG (WiFi, 4G, cloud sync)
│   │   ├── ui_about.c/h       ← THÔNG TIN
│   │   └── ui_nav.c/h         ← bottom navigation bar (5 tabs, SVG icons)
│   │
│   ├── sensor/
│   │   ├── pressure_sensor.c/h  ← ADC đọc 6 kênh áp suất, rolling avg, bar conversion
│   │   ├── angle_sensor.c/h     ← I²C góc nghiêng (pitch/roll), địa chỉ cấu hình NVS
│   │   └── sensor_hub.c/h       ← tổng hợp readings, cung cấp cho weighing logic
│   │
│   ├── weighing/
│   │   ├── weight_logic.c/h   ← state machine cân, tính khối lượng từ áp suất
│   │   └── weight_record.c/h  ← lưu lịch sử vào NVS / FATFS
│   │
│   ├── network/
│   │   ├── wifi_manager.c/h   ← ESP32-C6 bridge, scan/connect, NVS credentials
│   │   ├── eth_manager.c/h    ← Ethernet PoE (PHY IP101)
│   │   └── cloud_sync.c/h     ← REST API upload (HTTPS, TLS 1.3)
│   │
│   └── usb/
│       ├── usb_storage.c/h    ← TinyUSB MSC host, FAT mount, file write
│       └── report_export.c/h  ← tạo CSV/report từ weight_record, ghi vào USB path
│
├── components/
│   └── lvgl/                  ← LVGL v9.x (git submodule)
└── managed_components/        ← idf-component-manager cache
```

---

## Màn hình CÂN — State Machine

```
IDLE ──[BẮT ĐẦU CÂN]──► WEIGHING ──[đủ target]──► COMPLETE
  ▲                          │                          │
  └────────[RESET]───────────┘◄──────────[RESET]───────┘
```

| State | Mô tả |
|---|---|
| `IDLE` | Hiển thị 0, progress 0%, nút "BẮT ĐẦU CÂN" |
| `WEIGHING` | Đọc sensor_hub realtime → cập nhật LVGL mỗi 100ms |
| `COMPLETE` | Nhấp nháy "HOÀN THÀNH", gọi weight_record_save(), nút "RESET" |

---

## LVGL Task Setup (FreeRTOS)

```c
#define LVGL_TICK_PERIOD_MS  2
#define LVGL_TASK_STACK_KB   8
#define LVGL_TASK_PRIORITY   5

// lvgl_tick_task:    lv_tick_inc(2) mỗi 2ms (timer ISR hoặc FreeRTOS task)
// lvgl_handler_task: lv_timer_handler() trong loop, bảo vệ bằng mutex
// sensor_task:       đọc ADC + I²C mỗi 50ms, gửi vào sensor_hub queue
// network_task:      WiFi events + cloud sync mỗi 60s
// usb_task:          TinyUSB device task (nếu dùng USB MSC host thì là host task)
```

---

## Workflow phát triển

```
Plan → Code → Verify → Deploy
```

1. **Plan** — Đọc CLAUDE.md, xác định module cần làm, kiểm tra spec hardware
2. **Code** — Mỗi UI screen = 1 file, style từ `ui_theme.h`, không hardcode màu inline
3. **Verify** — `idf.py build && idf.py size` (kiểm tra RAM/Flash)
4. **Deploy** — `idf.py -p COMx flash monitor` hoặc OTA qua Ethernet

```bash
# Build & flash
cd E:\Project\Candientu\mayxuc\V3
idf.py set-target esp32p4
idf.py build
idf.py -p COM3 flash monitor

# OTA
idf.py build && python ota_upload.py --host 192.168.1.x
```

---

## Lưu ý quan trọng

1. **Vietnamese font** — compile bằng `lv_font_conv` với Unicode range 0x0000–0x024F (Latin Extended).
2. **PSRAM** — `CONFIG_SPIRAM=y`, LVGL frame buffer đặt trong PSRAM.
3. **Thread safety** — mọi `lv_*` call phải bọc trong `lvgl_port_lock()` / `lvgl_port_unlock()`.
4. **ADC accuracy** — dùng `adc_cali_scheme_line_fitting` để bù nhiệt độ và điện áp.
5. **I²C angle sensor** — địa chỉ lưu trong NVS key `"angle_addr"`, default `0x68` (MPU-6050).
6. **USB MSC host** — cần `CONFIG_TINYUSB_MSC_ENABLED=y` và partition FAT `storage` trong `partitions.csv`.
7. **ESP32-C6 bridge** — P4 giao tiếp C6 qua UART2 (AT commands hoặc custom protocol); C6 xử lý Wi-Fi stack.
8. **PoE ETH** — driver `esp_eth_mac_new_esp32` + PHY `esp_eth_phy_new_ip101`.
