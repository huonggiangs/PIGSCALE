#pragma once
/**
 * @file ui_shell.h
 * @brief Khung điều hướng: màn đăng nhập, thanh trạng thái, thanh phụ,
 *        vùng nội dung theo tab, thanh điều hướng dưới, nút Trợ giúp nổi.
 */

#include "lvgl.h"
#include "app_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Dựng toàn bộ giao diện (gọi 1 lần từ ui_main_start, trong lvgl_port_lock) */
void ui_shell_build(void);

/* Chuyển tab (được gọi bởi thanh điều hướng dưới, hoặc từ nơi khác trong app
 * — ví dụ "Bắt đầu cân" ở tab Đơn hàng chuyển thẳng sang tab Cân). */
void ui_shell_switch_tab(app_tab_t tab);

/* Cập nhật thanh trạng thái + thanh phụ (chấm kết nối, giờ, badge đồng bộ...) */
void ui_shell_refresh_chrome(void);

/* Hiện toast trong vùng nội dung hiện tại */
void ui_shell_toast(const char *text);

/* Sau khi đăng nhập thành công (gọi từ ui_login) */
void ui_shell_on_login_success(void);
/* Đăng xuất — quay lại màn hình đăng nhập */
void ui_shell_logout(void);

#ifdef __cplusplus
}
#endif
