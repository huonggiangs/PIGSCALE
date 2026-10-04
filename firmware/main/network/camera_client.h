#pragma once
/**
 * @file camera_client.h
 * @brief Kiểm tra kết nối mạng TỚI camera IP (Vivoo Web IP Camera).
 *
 * Theo tài liệu "WEB IP CAMERA — Configuration & User Guide" của Vivoo: đây là
 * camera có Web UI (đăng nhập qua form HTML trong trình duyệt), HTTP port mặc
 * định 80, RTSP port mặc định 554. Tài liệu TỰ GHI RÕ ở mục "17. Information
 * Still Needed" rằng chưa có URL RTSP/ONVIF thật hay tài liệu API/SDK nào được
 * xác nhận — nghĩa là KHÔNG có đặc tả nào để gọi "đăng nhập" hay "lấy luồng
 * video" một cách đúng đắn từ firmware này.
 *
 * Vì vậy module này CHỈ làm một việc THẬT và kiểm chứng được: xác nhận camera
 * có đang hiện diện + đáp ứng HTTP trên mạng hay không (TCP connect + 1 HTTP
 * GET, không đăng nhập) — dùng làm trạng thái "kết nối" hiển thị trên UI.
 * KHÔNG xem đây là đã "đăng nhập" hay "có luồng video" — xem video trực tiếp
 * cần URL RTSP/ONVIF thật từ Vivoo, hiện chưa có.
 */
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CAMERA_CHECK_UNKNOWN = 0,   /* chưa kiểm tra lần nào */
    CAMERA_CHECK_REACHABLE,     /* có phản hồi HTTP (bất kể mã trạng thái) */
    CAMERA_CHECK_UNREACHABLE,   /* lỗi kết nối / timeout */
} camera_check_result_t;

/** Bắt đầu kiểm tra bất đồng bộ (task nền, không chặn LVGL). No-op nếu đang
 *  có một lần kiểm tra khác chạy dở. */
void camera_client_test_async(const char *ip, uint16_t port);

/** True khi đang có một lần kiểm tra đang chạy. */
bool camera_client_is_busy(void);

/** Kết quả LẦN KIỂM TRA GẦN NHẤT đã hoàn tất. */
camera_check_result_t camera_client_get_result(void);

#ifdef __cplusplus
}
#endif
