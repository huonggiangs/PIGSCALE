#pragma once
/**
 * @file board_config.h
 * @brief GPIO pin map cho ESP32-P4-WIFI6-POE-ETH (Waveshare)
 *
 * Xác minh lại với schematic thực tế của bo mạch nếu cần điều chỉnh.
 * Tài liệu: https://www.waveshare.com/esp32-p4-wifi6-poe-eth.htm
 */

#include "driver/gpio.h"

/* ── MIPI-DSI Display (JD9365 10.1" 800×1280 portrait) ─────────────────── */
#define BOARD_LCD_H_RES          800
#define BOARD_LCD_V_RES          1280
#define BOARD_LCD_DSI_LANES      2          /* 2-lane MIPI-DSI */
/* 1500 Mbps/lane — xác nhận qua schematic + dự án tham chiếu
 * E:\Project\ESP32P4YOLO26n (cùng board, cùng panel 10.1", đã chạy ổn định).
 * Giá trị 1000 trước đây là SAI: panel đọc ID (lệnh DCS 0x04) lúc init bị
 * treo (không timeout, "task_wdt" treo cứng CPU) vì tốc độ lane không khớp
 * yêu cầu thực của panel — đây chính là nguyên nhân màn hình tối đen /
 * treo máy quan sát được. */
#define BOARD_LCD_DSI_LANE_MBPS  1500
#define BOARD_LCD_RST_GPIO       GPIO_NUM_27
/* Board Waveshare ESP32-P4-WIFI6-POE-ETH KHÔNG có chân GPIO nào điều khiển
 * đèn nền panel 10.1" — đèn nền được chỉnh qua IC I2C riêng, địa chỉ 0x45
 * (thanh ghi 0x96), nằm trên cùng bus I2C dùng chung (xem BOARD_TOUCH_I2C_*).
 * Component waveshare/esp_lcd_jd9365_10_1 tự bật đèn nền lên mức tối đa
 * trong lúc esp_lcd_panel_init(). BOARD_LCD_BL_GPIO/LEDC_CH giữ lại chỉ để
 * tương thích chữ ký display_config_t — không có tác dụng vật lý. */
#define BOARD_LCD_BL_GPIO        GPIO_NUM_26
#define BOARD_LCD_BL_LEDC_CH     0
/* Ghi chú: DPI timing (HSYNC/VSYNC porch) do macro
 * JD9365_800_1280_PANEL_60HZ_DPI_CONFIG() cung cấp.
 * Không cần khai báo thủ công ở đây. */

/* ── GT9271 Touch ────────────────────────────────────────────────────────── */
/* Bus I2C dùng chung: ES8311 (audio), GT911/GT9271 (cảm ứng), OV5647 (SCCB),
 * IC đèn nền panel 10.1" (0x45) — xác nhận qua schematic
 * ESP32-P4-WIFI6-POE-ETH-Schematic.pdf + dự án tham chiếu ESP32P4YOLO26n.
 * SDA=GPIO7, SCL=GPIO8 — giá trị cũ (SDA=8, SCL=9) SAI, gây "i2c transaction
 * failed" mỗi lần đọc GT911. */
#define BOARD_TOUCH_I2C_NUM      I2C_NUM_0
#define BOARD_TOUCH_I2C_SDA      GPIO_NUM_7
#define BOARD_TOUCH_I2C_SCL      GPIO_NUM_8
#define BOARD_TOUCH_I2C_FREQ     100000     /* 100 kHz — bus dài, nhiều thiết bị */
/* RST/INT của GT911/GT9271 KHÔNG nối tới ESP32-P4 trên board này (xác nhận
 * qua dự án tham chiếu ESP32P4YOLO26n) — đặt -1 để gt9271_init() bỏ qua
 * chuỗi reset/chọn địa chỉ và đọc cảm ứng theo chu kỳ (polling qua
 * esp_lcd_touch, không cần ngắt). Ngoài ra GPIO3 còn nghi ngờ nối tới khối
 * ESP32-C6 trên board — không nên dùng làm GPIO tùy ý. */
#define BOARD_TOUCH_INT_GPIO     GPIO_NUM_NC
#define BOARD_TOUCH_RST_GPIO     GPIO_NUM_NC
#define BOARD_TOUCH_I2C_ADDR     0x5D       /* 0x5D hoặc 0x14 tùy INT pull */
#define BOARD_TOUCH_MAX_POINTS   10

/* ── Ethernet RMII (LAN8720 PHY) ────────────────────────────────────────── */
#define BOARD_ETH_MDC_GPIO       GPIO_NUM_31
#define BOARD_ETH_MDIO_GPIO      GPIO_NUM_52
#define BOARD_ETH_TXD0_GPIO      GPIO_NUM_34
#define BOARD_ETH_TXD1_GPIO      GPIO_NUM_35
#define BOARD_ETH_TX_EN_GPIO     GPIO_NUM_36
#define BOARD_ETH_RXD0_GPIO      GPIO_NUM_39
#define BOARD_ETH_RXD1_GPIO      GPIO_NUM_40
#define BOARD_ETH_CRS_DV_GPIO    GPIO_NUM_41
#define BOARD_ETH_CLK_GPIO       GPIO_NUM_50 /* RMII CLK input */
#define BOARD_ETH_PHY_RST_GPIO   GPIO_NUM_51
#define BOARD_ETH_PHY_ADDR       1

/* ── NVS ─────────────────────────────────────────────────────────────────── */
#define BOARD_NVS_NAMESPACE      "pig_weigh_v1"

/* ── LVGL task config ────────────────────────────────────────────────────── */
#define LVGL_TASK_PRIORITY       5
/* 12 KB (chỉ gấp đôi mặc định 6KB của esp_lvgl_port) không đủ khi LVGL thực
   sự đo/dựng chữ (text shaping, kerning, style cascade nhiều lớp flexbox) —
   xác nhận qua thực nghiệm: lúc font rỗng (hiển thị toàn ô vuông) máy chạy
   ổn định, ngay khi font thật render được thì "taskLVGL" (core 1) tràn stack,
   phá hỏng heap kế bên, gây "Guru Meditation Store access fault" ngẫu nhiên
   trong get_local_style/lv_obj_allocate_spec_attr (task watchdog xác nhận
   đúng taskLVGL đang chạy lúc treo). Tăng lên 32 KB, dư dả với ~300KB+ SRAM
   nội bộ còn trống theo log heap_init lúc boot. */
#define LVGL_TASK_STACK_KB       32         /* KB */
#define LVGL_TASK_CORE           1          /* Pin vào core 1 */
#define LVGL_TICK_PERIOD_MS      2
#define LVGL_BUF_LINES           40         /* Số dòng trong 1 draw buffer */
