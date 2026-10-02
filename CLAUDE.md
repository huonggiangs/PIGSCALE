# PIG WEIGH — TRẠM CÂN HEO — CLAUDE.md

> **Lưu ý quan trọng về repo này:** file này mô tả dự án **THẬT ĐANG PHÁT TRIỂN** —
> nằm trong thư mục **`firmware/`**. Các file khác ở cấp gốc repo (`main/`,
> `CMakeLists.txt` gốc, `idf_component.yml` gốc, `sdkconfig.defaults` gốc,
> `partitions.csv` gốc, `preview_screens.html`, `screen01.png`) là **scaffold cũ
> của một dự án khác** ("CÂN MÁY XÚC V3" — máy xúc, không phải cân heo) để lại từ
> trước — **CHỈ DÙNG THAM KHẢO**, không phải code đang chạy, không build/nạp từ đó.
> **Luôn `cd firmware` trước khi chạy `idf.py build/flash`.**
>
> Repo chuẩn (canonical): `https://github.com/huonggiangs/canheo.git`

---

## Mô tả dự án

Hệ thống **cân heo** (pig weighing station) hiển thị trên màn hình nhúng ESP32-P4 10.1".
Giao diện dark/light-theme công nghiệp, tiếng Việt, vận hành bởi 3 vai trò: **Nhân
viên cân**, **Quản lý**, **Kỹ thuật**.

## Hardware Target

| Thành phần | Thông số |
|---|---|
| Board | **Waveshare ESP32-P4-WIFI6-POE-ETH** |
| MCU chính | ESP32-P4 (dual-core RISC-V @ 360 MHz), **chip rev v1.3** |
| MCU mạng | ESP32-C6 qua **SDIO** (esp_hosted + esp_wifi_remote) — P4 không có radio WiFi riêng |
| Display | JD9365 10.1" 800×1280 MIPI-DSI |
| Cảm ứng | GT9271/GT911 (I2C, 0x5D) |
| Ethernet | PoE, LAN8720 (RMII) — hiện báo lỗi "wrong chip OUI", cần kiểm tra lại phần cứng |
| RAM | 32 MB PSRAM (octal) |
| Flash | 16 MB |
| Framework | **ESP-IDF v5.5.5** |
| UI Library | **LVGL v9.2.x** |

**Mọi gotcha/cạm bẫy phần cứng cụ thể của board này (đã xác nhận qua thực tế bring-up,
không phải lý thuyết) nằm trong [`docs/ESP32P4_HARDWARE_BRINGUP.md`](docs/ESP32P4_HARDWARE_BRINGUP.md)
— ĐỌC TRƯỚC khi đụng vào display/touch/WiFi/bộ nhớ/múi giờ.**

---

## Cấu trúc thư mục (`firmware/`)

```
firmware/
├── main/
│   ├── main.c                  ← app_main: NVS → netif → display → touch → eth → wifi → UI
│   ├── board_config.h          ← toàn bộ chân GPIO/địa chỉ I2C/tốc độ DSI
│   ├── display/display_driver.c/h
│   ├── touch/gt9271.c/h
│   ├── network/
│   │   ├── wifi_manager.c/h    ← WiFi STA qua ESP32-C6/SDIO — THẬT (không demo)
│   │   └── eth_manager.c/h     ← Ethernet PoE (LAN8720) — đang lỗi, xem bảng trên
│   └── ui/
│       ├── app_state.c/h       ← state ứng dụng — ranh giới UI/dữ liệu, nhiều phần
│       │                          còn là MÔ PHỎNG (demo), xem mục "Demo vs Thật" dưới
│       ├── ui_shell.c/h        ← khung chính: header/sub-bar/bottom-nav/tab switch,
│       │                          màn chờ (standby) khi rảnh ở tab Cài đặt
│       ├── ui_login.c/h        ← đăng nhập PIN (chọn nhân viên → nhập PIN)
│       ├── ui_orders.c/h       ← ĐƠN HÀNG
│       ├── ui_weighing.c/h     ← CÂN
│       ├── ui_alerts.c/h       ← ĐỀ XUẤT/cảnh báo
│       ├── ui_history.c/h      ← LỊCH SỬ
│       ├── ui_settings.c/h     ← CÀI ĐẶT (chỉ Kỹ thuật được vào — xem mục Bảo mật)
│       ├── ui_common.c/h       ← widget dùng chung (card, badge, keypad, pin dots...)
│       ├── ui_theme.h          ← màu/font/kích thước — không hardcode màu inline nơi khác
│       └── fonts/, icons/      ← font Inter compile riêng (có dấu tiếng Việt) + SVG icon
```

---

## Vai trò & quyền truy cập

| Vai trò | Mã demo | PIN demo | Quyền |
|---|---|---|---|
| Nhân viên cân | NV001 | — | Đơn hàng, Cân, Lịch sử — **không vào được Cài đặt** |
| Quản lý | QL001 | — | Như trên — **không vào được Cài đặt** |
| Kỹ thuật | KT001 | — | Toàn quyền, **duy nhất vai trò được cấu hình thiết bị** |

> PIN đang hardcode trong `app_state.c` (demo) và có khoá tạm 30 giây sau 5 lần
> nhập sai liên tiếp (`app_state_login_is_locked`). **Trước khi triển khai thật phải
> thay bằng cơ chế quản lý tài khoản thật** — xem mục Bảo mật.

---

## Demo vs Thật — ranh giới cần biết trước khi "nghiệm thu"

| Khối chức năng | Trạng thái |
|---|---|
| WiFi (kết nối, lưu NVS, quét mạng) | **THẬT** — qua ESP32-C6/SDIO |
| Đồng bộ giờ (NTP, múi giờ GMT+7) | **THẬT** — xem `wifi_manager.c`, `main.c` |
| Hiển thị, cảm ứng | **THẬT** |
| Gateway (kết nối, token) | **Demo** — ô nhập thật, nhưng "Kết nối" chỉ mô phỏng |
| Camera (tìm kiếm, tài khoản) | **Demo** — danh sách tìm được là mô phỏng |
| P5 Scale (tìm kiếm, kết nối) | **Demo** — chưa có giao thức Modbus-TCP thật |
| Máy in, lịch sử cân, đơn hàng | **Mô phỏng dữ liệu** (không có backend/server thật) |
| Bảo mật (Secure Boot/Flash/NVS Encryption) | **TẮT** — xem mục Bảo mật |

Khi thêm backend thật cho bất kỳ khối "Demo" nào, chỉ sửa bên trong file tương ứng —
các hàm `app_state_*` là ranh giới ổn định mà UI gọi vào, không cần đổi UI.

---

## Bảo mật — các việc BẮT BUỘC trước khi triển khai thật

1. **PIN hardcode trong mã nguồn** (`app_state.c`) — thay bằng cơ chế tài khoản thật
   (nhập/đổi PIN qua Kỹ thuật, lưu không phải plaintext trong mã nguồn).
2. **Secure Boot / Flash Encryption / NVS Encryption đang TẮT** (`sdkconfig`) →
   mật khẩu WiFi, token Gateway, mật khẩu camera đang lưu **plaintext trên flash**.
   Bật các cơ chế này cần **burn eFuse — KHÔNG THỂ ĐẢO NGƯỢC** và sẽ đổi hẳn quy
   trình nạp firmware (phải ký + mã hoá mỗi lần build) — **chỉ bật khi đã chốt
   thiết kế phần cứng/firmware, không bật khi còn đang phát triển lặp lại
   thường xuyên**, và cần xác nhận rõ ràng trước khi thực hiện trên thiết bị thật.
3. Ethernet PoE báo lỗi phần cứng (`wrong chip OUI`) — cần xác minh lại PHY/board
   trước khi coi mạng dây là kênh dự phòng đáng tin cậy.

---

## Quy trình build & nạp

```powershell
cd firmware
. C:\Espressif\frameworks\esp-idf-v5.5.5\export.ps1
idf.py set-target esp32p4      # chỉ 1 lần
idf.py build
idf.py -p COM6 -b 921600 flash
```

Xác minh sau mỗi lần nạp bằng log serial (115200 baud): tìm chuỗi hoàn tất boot
(`[7/7] ... OK`, `KHỞI ĐỘNG HOÀN TẤT`) và **không** có `Guru Meditation`/`assert failed`.

---

## Quy ước code

- Mỗi UI screen = 1 file (`ui_xxx.c/h`), style lấy từ `ui_theme.h`, không hardcode màu/font inline.
- `lv_obj_create()` mặc định BẬT `LV_OBJ_FLAG_CLICKABLE` và KHÔNG tự có flex layout —
  xem mục 8 trong `docs/ESP32P4_HARDWARE_BRINGUP.md` trước khi dựng container mới.
- Mọi commit message bằng **tiếng Việt có dấu**.
- Workflow: `Plan → Code → Verify (idf.py build, nạp thật, đọc log serial) → Commit → Push`.
