# Tài liệu Bring-up phần cứng — ESP32-P4-WIFI6-POE-ETH

> **Mục đích:** Ghi lại TOÀN BỘ lỗi/cạm bẫy phần cứng đã gặp phải khi phát triển firmware
> trên board này (dự án PIG WEIGH — trạm cân heo), để **bất kỳ ứng dụng nào sau này
> chạy trên CÙNG BOARD** đều tránh lặp lại từ đầu. Đây không phải lỗi logic ứng dụng —
> đây là đặc tính PHẦN CỨNG + phiên bản chip/thư viện cụ thể của board này.
>
> Board: **Waveshare ESP32-P4-WIFI6-POE-ETH** — ESP32-P4 (dual-core RISC-V) + ESP32-C6
> (WiFi/BLE co-processor qua SDIO) + màn MIPI-DSI JD9365 800×1280 + cảm ứng GT9271 +
> Ethernet PoE LAN8720.

---

## 0. Bẫy build system — 2 project ESP-IDF trong 1 repo

Nếu bạn copy scaffold cũ sang dự án mới (như đã xảy ra với repo PIG), **kiểm tra
KHÔNG có `CMakeLists.txt` + `main/` ở cấp gốc repo** ngoài project thật. Một project
ESP-IDF cũ (ví dụ "CÂN MÁY XÚC V3") có thể còn sót lại ở thư mục gốc cùng lúc với
project thật nằm trong thư mục con (`firmware/`). Hậu quả: nếu ai đó chạy
`idf.py build` ở **sai thư mục** (gốc repo thay vì `firmware/`), CMake sẽ âm thầm
build project CŨ (hoàn toàn khác, định danh `project(mayxuc_v3)`) mà không báo lỗi gì
rõ ràng — rất dễ nạp nhầm firmware.

**Luôn `cd` vào đúng thư mục project trước khi `idf.py build/flash`.** Nếu phát hiện
scaffold cũ không dùng tới, nên dọn hẳn (hỏi người dùng trước khi xoá).

---

## 1. Màn hình — MIPI-DSI JD9365 10.1" 800×1280

| Vấn đề | Cách khắc phục |
|---|---|
| Dùng component `espressif/esp_lcd_jd9365` (generic) → treo khi đọc Panel ID lúc khởi động | Dùng **`waveshare/esp_lcd_jd9365_10_1`** (managed component riêng của Waveshare cho đúng panel này) |
| DSI lane rate 1000 Mbps → hình không lên / treo | Đặt **1500 Mbps** (`BOARD_LCD_DSI_LANE_MBPS`) |
| `lvgl_port_add_disp()` (API chung) không hoạt động với panel DSI | Phải dùng **`lvgl_port_add_disp_dsi()`** |
| Bật `dsi_cfg.flags.avoid_tearing = true` → LVGL task treo vĩnh viễn | Đây là **bug đã xác nhận của ESP-IDF 5.5** (callback đổi tên `on_refresh_done` → `on_frame_buf_complete` nhưng `esp_lvgl_port` chưa cập nhật theo) — **PHẢI để `avoid_tearing = false`**, dùng `direct_mode = false` + buffer LVGL = 1/4 màn hình (`(h_res*v_res)/4`) |
| `dpi_cfg.num_fbs` | Đặt = 2 cho panel DPI |

---

## 2. Cảm ứng — GT9271/GT911 (I2C)

| Vấn đề | Cách khắc phục |
|---|---|
| Chân RST/INT không nối ra ngoài trên board này | Set `GPIO_NUM_NC` (-1) cho cả 2 — code driver GT9271 phải có nhánh bỏ qua bước reset/chọn địa chỉ khi gặp GPIO NC (nếu không sẽ gọi `gpio_set_level` trên chân -1 → lỗi) |
| SDA/SCL | **SDA = GPIO7, SCL = GPIO8**, địa chỉ I2C mặc định **0x5D** |
| Cảm ứng đọc chập chờn / không phản hồi dù init OK | Driver đèn nền (backlight) của panel JD9365 **chiếm tạm 2 chân GPIO7/GPIO8 làm I2C nội bộ** lúc init display. Phải gọi hàm giải phóng (`gpio_reset_pin` trên 2 chân đó) **SAU khi display_init() xong, TRƯỚC KHI** gt9271_init() giành lại 2 chân này |
| GT911 báo lỗi I2C lúc boot | **KHÔNG** dùng `ESP_ERROR_CHECK` ở bước touch — lỗi phần cứng cảm ứng (dây/địa chỉ sai) sẽ làm `abort()` → reboot-loop vô hạn, cả màn hình (đã lên hình tốt) cũng không bao giờ hiển thị được. Log lỗi + cho chạy tiếp không cảm ứng |

---

## 3. Chip revision ESP32-P4 v1.3 (silicon đời trước v3.0)

Board này dùng chip rev **v1.3**. Nếu `sdkconfig` mặc định yêu cầu rev ≥ 3.0 (một số
template ESP-IDF mới đặt sẵn), app sẽ từ chối chạy hoặc báo lỗi `efuse` lúc boot.

**Bắt buộc có trong sdkconfig:**
```
CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y
CONFIG_ESP32P4_REV_MIN_100=y
```

---

## 4. WiFi qua ESP32-C6 (SDIO + esp_hosted + esp_wifi_remote)

ESP32-P4 **không có radio WiFi riêng** — mọi lệnh `esp_wifi_*` được chuyển tiếp xuống
ESP32-C6 qua SDIO (giao thức `esp_hosted`).

| Vấn đề | Cách khắc phục |
|---|---|
| `idf_component.yml` ghi `espressif/esp_hosted: "*"` → bản mới nhất (2.12.9 tại thời điểm viết) **boot-loop** trên chip rev v1.3 | **Ghim cứng phiên bản: `"==2.12.2"`** |
| Boot-loop FreeRTOS assert `app_startup.c:86` liên quan TCM | esp_hosted cần vùng heap TCM dành riêng — task stack KHÔNG được cấp phát vào TCM. Phải **reserve vùng TCM** bằng `SOC_RESERVE_MEMORY_REGION` trong `main.c` (đặt từ địa chỉ `_spm_data_end`, kích thước 0x30102000, đặt TRONG main.c để chắc chắn được linker giữ lại):<br>```c\nextern int _spm_data_end;\nSOC_RESERVE_MEMORY_REGION((intptr_t)&_spm_data_end, 0x30102000, tcm_heap_keepout);\n``` |
| sdkconfig cần thêm | `CONFIG_ESP_HOSTED_CP_TARGET_ESP32C6=y`<br>`CONFIG_ESP_HOSTED_DFLT_TASK_FROM_SPIRAM=y`<br>`CONFIG_ESP_HOSTED_SDIO_TX_Q_SIZE=4`<br>`CONFIG_ESP_HOSTED_SDIO_RX_Q_SIZE=4`<br>`CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP=y` |
| Chân SDIO (cố định trên board) | CLK=18, CMD=19, D0=14, D1=15, D2=16, D3=17, Slave Reset=GPIO54 |
| `esp_wifi_init()` có thể treo vài giây khi bắt tay với C6 | Chạy **trong task riêng** (không chặn `app_main`), mọi lỗi để **non-fatal** (UI vẫn chạy nếu C6 không phản hồi) |
| Giờ hệ thống luôn là UTC dù đã kết nối WiFi/NTP | Xem mục 7 — phải `setenv("TZ", ...)` **thủ công**, SNTP chỉ đặt UTC epoch, không tự suy ra múi giờ |

---

## 5. Ethernet PoE — LAN8720 / IP101 (RMII)

Lỗi "wrong chip OUI" lúc init PHY nghĩa là thư viện đọc được ID chip Ethernet nhưng
KHÔNG khớp loại PHY đã khai báo trong code — kiểm tra lại đúng theo board thực tế:
`MDC_GPIO` / `MDIO_GPIO` / `PHY_RST_GPIO` / `PHY_ADDR`, và đúng hàm driver
(`esp_eth_phy_new_lan87xx` vs `esp_eth_phy_new_ip101` — 2 board/revision khác nhau có
thể dùng PHY khác nhau dù cùng dòng Waveshare). **Không** `ESP_ERROR_CHECK` bước này —
để non-fatal, máy vẫn phải lên màn hình dù Ethernet lỗi.

---

## 6. LVGL v9 — bộ nhớ (PSRAM)

| Vấn đề | Cách khắc phục |
|---|---|
| Mặc định `CONFIG_LV_USE_BUILTIN_MALLOC` + `CONFIG_SPIRAM_CAPS_ALLOC` dùng pool nội bộ **cố định 64KB** → hết bộ nhớ khi UI phức tạp, `lv_malloc()` **âm thầm trả NULL**, code gọi tiếp (vd. `lv_style_init`) không kiểm tra NULL → `memset(NULL, ...)` crash sâu trong ROM, rất khó debug | Dùng **`CONFIG_LV_USE_CLIB_MALLOC=y`** + **`CONFIG_SPIRAM_USE_MALLOC=y`** để LVGL cấp phát trực tiếp từ heap PSRAM 32MB (không giới hạn 64KB) |
| Frame buffer LVGL | Đặt trong PSRAM (`CONFIG_SPIRAM=y`, octal PSRAM 32MB) |

---

## 7. Múi giờ / NTP — KHÔNG có mặc định đúng

ESP32 **không có RTC pin lâu dài** (không pin CMOS) — sau mất điện, giờ hệ thống luôn
reset về epoch 0 (1970 UTC) và **giờ hệ thống LUÔN LÀ UTC**, kể cả sau khi đồng bộ NTP
thành công (SNTP chỉ set đúng mốc UTC, không tự đổi múi giờ hiển thị).

**Bắt buộc** gọi lúc boot (hoặc lúc có xác nhận múi giờ người dùng chọn):
```c
setenv("TZ", "ICT-7", 1);   // GMT+7 Việt Nam — dấu NGƯỢC theo chuẩn POSIX!
tzset();
```
> Lưu ý dấu NGƯỢC: muốn hiển thị UTC+7 phải ghi `"ICT-7"` / `"UTC-7"` (không phải `+7`).
> Nếu quên bước này, `localtime()`/`strftime()` vẫn chạy bình thường, KHÔNG báo lỗi gì —
> chỉ là mọi đồng hồ hiển thị trên UI sẽ lệch 7 tiếng so với giờ thực tế, rất dễ bị bỏ
> sót khi review code vì không có warning/crash nào cả.

Để biết đã từng đồng bộ NTP thành công hay chưa (hiển thị trạng thái cho người dùng),
đăng ký `esp_sntp_config_t.sync_cb` (callback báo khi có mốc giờ mới), KHÔNG tự suy luận
từ việc "đã kết nối WiFi" (kết nối WiFi không đồng nghĩa đã lấy được giờ từ NTP server).

---

## 8. LVGL — 3 gotcha hay gặp nhất khi dựng UI (không phải bug board, nhưng rất tốn
   thời gian debug nếu không biết trước)

### 8.1. `lv_obj_create()` mặc định BẬT `LV_OBJ_FLAG_CLICKABLE`
`lv_obj_remove_style_all()` chỉ xoá STYLE, **không xoá FLAG**. Một container trơn
(`lv_obj_create()`) đặt ĐÈ lên một nút bấm (button đã có event handler riêng) sẽ
"cướp" điểm chạm đúng vùng nó phủ tới mà không có handler nào xử lý → **bấm vào chữ
không có tác dụng, chỉ bấm ra ngoài viền mới ăn**. Sửa bằng 1 trong 2 cách:
- `lv_obj_remove_flag(wrapper, LV_OBJ_FLAG_CLICKABLE)` nếu wrapper không cần tự bắt sự
  kiện gì (để sự kiện rơi thẳng xuống nút cha bên dưới).
- `lv_obj_add_flag(wrapper, LV_OBJ_FLAG_EVENT_BUBBLE)` nếu wrapper (và toàn bộ con của
  nó) CẦN nổi bọt sự kiện lên cha (ví dụ: 1 màn phủ toàn màn hình có handler "chạm để
  tắt", nhưng bên trong có nhiều label trang trí không cần tự xử lý gì).

### 8.2. Flex container `LV_SIZE_CONTENT` + `LV_FLEX_ALIGN_END` (hoặc CENTER) trên trục
chính → tính SAI bề rộng tự động
Container tự co theo nội dung (`LV_SIZE_CONTENT`) kết hợp căn **END** trên trục chính
khiến LVGL tính nhầm kích thước (chỉ tính theo phần tử cuối cùng), đẩy các phần tử
thêm trước đó ra toạ độ ÂM rồi bị cắt (clip) mất khỏi màn hình — trông như
"biến mất" dù đã add vào cây UI đúng cách. **Luôn dùng `LV_FLEX_ALIGN_START`** cho
container tự co nội dung; nếu cần cả KHỐI nằm bên phải/trái, căn bằng
`lv_obj_align()` trên CHÍNH container đó (không phải căn các con bên trong).

### 8.3. Card/container trơn không tự có flex layout
`lv_obj_create()` (container trơn, không phải layout có sẵn) **không** tự đặt
`flex_flow`. Thêm 2+ label con mà quên gọi `lv_obj_set_flex_flow(...)` → mọi label con
mặc định nằm cùng toạ độ (0,0) → **chữ chồng khít lên nhau**. Luôn gọi
`lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN)` (hoặc ROW) ngay sau khi tạo bất kỳ
card/container nào dự kiến chứa từ 2 phần tử con trở lên.

### 8.4. `lv_obj_move_to_index()` gọi SAU khi cha đã tính xong `LV_SIZE_CONTENT`
Để cha tự co kích thước đúng, hãy **dựng các con theo đúng thứ tự cuối cùng ngay từ
đầu** thay vì dựng rồi sắp xếp lại bằng `move_to_index()` — kích thước cha đã cache
trước đó sẽ không được tính lại, khiến phần tử mới thêm/sắp xếp lại hiển thị sai vị trí.

---

## 9. Font tiếng Việt

Font LVGL mặc định (Montserrat) **không có dấu tiếng Việt**. Phải compile font riêng
bằng `lv_font_conv` (qua `npx`) với dải Unicode **Latin Extended (0x0000–0x024F)**.
Nhớ set **font tường minh** cho MỌI label/textarea (kể cả placeholder text của ô
nhập) — nếu không set, một số widget âm thầm dùng font mặc định không có dấu → chữ
hiện thành ô vuông (tofu box) dù phần còn lại của app hiện đúng.

---

## 10. Quy trình build & nạp chuẩn cho board này

```powershell
cd <thư_mục_project>          # LUÔN đúng project con, xem mục 0
. C:\Espressif\frameworks\esp-idf-v5.5.5\export.ps1
idf.py set-target esp32p4     # chỉ cần 1 lần
idf.py build
idf.py -p COM6 -b 921600 flash
```

Xác minh sau mỗi lần nạp bằng cách đọc log serial (115200 baud) trong ~15s, tìm:
- Chuỗi hoàn tất boot (ví dụ `[7/7] ... OK` + `KHỞI ĐỘNG HOÀN TẤT`)
- **Không** có `Guru Meditation` / `assert failed` → nếu có, đây là crash thật, không
  phải log bình thường.

---

*Tài liệu này tổng hợp từ quá trình bring-up thực tế dự án PIG WEIGH trên board
ESP32-P4-WIFI6-POE-ETH — áp dụng cho MỌI ứng dụng khác chạy trên cùng phần cứng này.*
