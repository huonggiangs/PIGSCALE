# Rà soát ứng dụng PIG WEIGH — 06/10/2026

## Việc đã khắc phục

- Bàn phím Camera Vivoo được cố định ngoài vùng cuộn, hỗ trợ mở lại sau khi đóng, nút hoàn tất/huỷ, tự cuộn trường đang nhập lên trên bàn phím và đóng khi rời tab/đăng xuất.
- Cài sẵn IP `192.168.1.10`, tài khoản `admin`, mật khẩu người dùng cung cấp, HTTPS `443`; lưu username/mật khẩu, URL RTSP chính/phụ, token snapshot và cổng nhận vào NVS. Các cổng được kiểm tra trong khoảng 1–65535.
- Mở HTTP listener, mặc định cổng `8080`, nhận `POST /api/device-data`; kiểm tra IP nguồn camera, MIME, giới hạn body 16 KiB, JSON/schema/ID/count/timestamp, bản tin cũ và bản tin lặp. Trả JSON `ok`; giữ số đếm gần nhất trong RAM.
- Sau bản tin đếm, lấy JPEG từ `/api/v1/media/snapshot` bằng Bearer token, giới hạn ảnh 512 KiB, xác nhận HTTP/MIME/JPEG và giữ ảnh gần nhất trong RAM. Tab Cài đặt cho biết endpoint, trạng thái, số đếm, kết quả tải ảnh và nút xem ảnh.
- Tab Cân hiển thị URL RTSP chính/phụ đã cấu hình và giải thích rằng xem video trực tiếp chưa được triển khai. `firmware/main` hiện không chứa RTSP transport/decoder; endpoint RTSP không tự phát video.

## Phát hiện cần xử lý trước khi triển khai vận hành

| Mức | Phát hiện | Căn cứ trong mã | Hướng khắc phục |
|---|---|---|---|
| P0 | Đơn hàng, lịch sử, phiếu cân, kiểm toán và xác nhận cân vẫn là dữ liệu mẫu trong RAM. Không có backend đồng bộ; `app_state_sync_now()` đổi cờ chờ thành đã đồng bộ mà không gửi mạng. | `firmware/main/ui/app_state.c`: `app_state_init`, `app_state_weighing_confirm`, `app_state_sync_now`; `firmware/main/ui/app_state.h` | Chốt API backend và định dạng nghiệp vụ; ghi giao dịch vào storage bền vững trước xác nhận; chỉ đánh dấu đồng bộ sau ACK máy chủ; định nghĩa retry, idempotency và khôi phục sau mất điện. |
| P0 | Bộ đếm camera chưa gắn sự kiện nhận được với phiên cân/đơn hàng và chưa điền `line_count`/`snapshot_count`. UI chỉ xem bản tin/ảnh gần nhất; phiếu không tự chốt. | `firmware/main/network/camera_receiver.c`; `firmware/main/ui/app_state.c:app_state_start_weighing`; `firmware/main/ui/ui_weighing.c` | Chốt khóa ghép (order/session ID, camera ID, timestamp, event ID), xử lý sự kiện đến lặp/muộn và chính sách cho phép xác nhận thiếu ảnh; sau đó mới nối count vào phiên cân. |
| P1 | Video RTSP chưa phát được dù URI đã hiển thị. Chưa có RTSP/RTP client, xử lý H.264/H.265 và renderer video trong firmware. | `firmware/main/ui/ui_weighing.c`; danh sách component trong `firmware/main/CMakeLists.txt` | Xác nhận codec, profile, độ phân giải và transport thực tế từ camera; thử decoder khả dụng trên ESP32-P4, băng thông/memory, rồi triển khai player có reconnect và teardown theo vòng đời tab. |
| P1 | User/password camera được lưu nhưng API PDF không mô tả API đăng nhập/cấp Bearer token; credential không thể thay cho token snapshot. Bộ nhận từ chối IP khác nhưng HTTP port 8080 không có chữ ký/HMAC hoặc TLS và còn có thể nhận JSON giả mạo từ host dùng IP camera trên cùng LAN. | `firmware/main/network/camera_receiver.c`; mục snapshot của tài liệu Vivoo | Xác nhận với Vivoo cơ chế phát token, TLS hỗ trợ ở thiết bị nhận, header/secret của webhook và giới hạn địa chỉ/ACL/VLAN. Không đưa cổng này ra Internet; cấp token từ UI khi hãng cung cấp. |
| P1 | Mật khẩu camera mặc định được đưa vào mã nguồn/firmware để card điền sẵn; người có firmware hoặc quyền đọc repository có thể lấy lại giá trị. NVS Encryption cũng chưa bật. | `firmware/main/network/camera_receiver.c:250`; `firmware/sdkconfig` | Đổi mật khẩu trước khi cài đặt thực địa; chuyển sang bước provisioning trên thiết bị/NVS, không giữ credential mặc định trong firmware phát hành. |
| P1 | Tắt kiểm tra chứng chỉ TLS toàn hệ thống cho kết nối camera tự ký; username/password, Wi-Fi và token lưu dạng blob NVS. Hiện chưa bật NVS Encryption/Secure Boot. | `firmware/sdkconfig.defaults`, `firmware/main/network/camera_client.c`, `firmware/main/network/camera_receiver.c`; mục bảo mật trong `CLAUDE.md` | Dùng CA/fingerprint riêng cho camera; thu hẹp ngoại lệ TLS về đúng kết nối camera nếu thiết bị bắt buộc self-signed. Thiết kế NVS Encryption/Secure Boot khi đã chốt quy trình ký/nạp/eFuse sản phẩm. |
| P1 | Ethernet PoE vẫn có ghi nhận lỗi PHY `wrong chip OUI`; firmware gọi driver LAN87xx. Không nên coi Ethernet là đường truyền hoạt động cho đến khi xác minh chip/PHY address/reset thực tế. | `CLAUDE.md`; `docs/ESP32P4_HARDWARE_BRINGUP.md`; `firmware/main/network/eth_manager.c` | Đo/đọc chip PHY và xác nhận pin, địa chỉ MDIO, clock RMII trên đúng revision board; kiểm tra IP và lưu lượng camera/backend khi cắm Ethernet. |
| P2 | IP tĩnh chỉ kiểm tra chuỗi IP chính; netmask, gateway, DNS có thể sai định dạng/không cùng subnet. Lỗi lưu IP tĩnh trong NVS bị bỏ qua nhưng UI vẫn thông báo thành công. | `firmware/main/network/wifi_manager.c:wifi_manager_set_static_ip`; `firmware/main/ui/app_state.c:app_state_wifi_set_static_ip` | Xác thực tất cả trường và subnet/gateway; trả lỗi lưu/áp dụng lên UI; kiểm tra DHCP/static reconnect và phục hồi sau reboot. |
| P2 | Tài khoản kỹ thuật/PIN demo hardcode; ứng dụng khởi động bằng dữ liệu mẫu. | `firmware/main/ui/app_state.c:app_state_init`, `app_state_try_login` | Trước khi vận hành thật, thay bằng danh tính/PIN quản lý an toàn, phân quyền phía nghiệp vụ và cấu hình thiết bị ban đầu. |
| P2 | Cổng HTTPS 443 là giá trị cấu hình mặc định tạm dùng vì chưa được cung cấp; user/password không tham gia phép kiểm tra hiện tại, vốn chỉ xác nhận phản hồi HTTPS. Cổng nhận số đếm được chọn mặc định 8080 theo yêu cầu. | `firmware/main/network/camera_receiver.c`, `firmware/main/network/camera_client.c`, `firmware/main/ui/ui_settings.c` | Xác nhận HTTPS port thật và kiểm tra `/` qua trình duyệt/serial trên camera/trạm mục tiêu trước khi coi trạng thái HTTP là đăng nhập hoặc sẵn sàng stream. |
| P2 | Project ESP-IDF trong `firmware/` còn mang tên `can_may_xuc_v3` và tạo binary `can_may_xuc_v3.bin`, dù giao diện là Pig Weigh; có thể gây nhầm khi đóng gói/nạp thiết bị. | `firmware/CMakeLists.txt` | Đổi project/artefact name sang PIG WEIGH và xác nhận boot log, OTA/build automation sau khi thống nhất tên triển khai. |

## Xác minh

- `firmware/tests/host`: hai bài kiểm thử chạy qua. `camera_protocol` kiểm tra port, schema, số đếm, timestamp, JSON cắt cụt/lồng sâu và JPEG. `settings_keyboard` chạy production widget trên LVGL 9.2 để kiểm tra 8 trường, hình học/scroll, focus, mở lại, Ready/Cancel, bảo toàn draft và refresh.
- `idf.py -C firmware build`: thành công với ESP-IDF 5.5.5; ảnh firmware `0x2ca6e0`, còn 44% phân vùng app nhỏ nhất.
- Chưa kiểm thử trên board/camera thật: không có kết quả xác nhận HTTP POST, TLS/token/JPEG, port 8080 xuyên mạng, hoặc phát RTSP; Ethernet đang có cảnh báo phần cứng nêu trên.

## Cập nhật xử lý — 06/10/2026

### Đã xử lý trong phiên này

- Count chỉ được gắn vào phiên cân đang mở nếu nhận được sequence mới sau lúc phiên (hoặc lần cân lại) bắt đầu. Không lấy lại sự kiện camera cũ khi mở đơn mới. Số lượng từ camera được giữ riêng theo nguồn và dùng làm số lượng phiếu; giao diện nhập tay chỉ yêu cầu khối lượng và khóa số con do camera cung cấp.
- Phiếu mới luôn ở trạng thái chờ đồng bộ cho tới khi có ACK backend. Nút đồng bộ không còn tự đổi cờ thành công khi chưa gửi request tới máy chủ.
- Đã thêm `espressif/esp_rtsp_service` vào dependency firmware để cung cấp RTSP pull client H.264/MJPEG trên ESP-IDF.

### Còn tồn tại / chưa thể xác nhận

- RTSP service mới được kéo vào build; chưa có sink/decoder/render pipeline chạy trong giao diện. Camera mở TCP/554 nhưng đóng kết nối ngay với OPTIONS và DESCRIBE từ máy phát triển, nên chưa xác nhận được SDP, auth scheme, RTP transport, codec/profile hoặc độ phân giải. H.264 decoder của ESP32-P4 là software decode; tài liệu Espressif không liệt kê H.265 decoder cho P4. Cần cấu hình camera ra H.264 để hoàn tất player. Không tuyên bố video đã phát.
- API token snapshot, webhook auth, TLS CA pinning, provisioning credential, dữ liệu bền vững/backend ACK, xác minh Ethernet PHY, và tài khoản/PIN demo vẫn cần xử lý. Không đốt eFuse Secure Boot/NVS Encryption trong lượt này.
- Cổng nhận 8080 là listener trên firmware, không phải dịch vụ đang mở tại `192.168.1.10`; chỉ camera mới có thể gửi POST tới IP của thiết bị PIG sau khi firmware chạy trên board và hai thiết bị cùng subnet.

### Kiểm thử cập nhật

- ESP-IDF 5.5.5: build nền trước thay đổi RTSP thành công. Build có RTSP service đang được chạy lại sau khi component manager giải quyết dependency; cần ghi kết quả cuối sau khi hoàn tất.
- Máy phát triển kết nối TCP được tới camera `192.168.1.10:554`; RTSP OPTIONS/DESCRIBE hiện bị camera reset.
- Chưa nạp firmware lên thiết bị và chưa xác minh bằng luồng video thật.
- IP tĩnh Wi-Fi: đã bổ sung xác thực địa chỉ IPv4, netmask liên tục, host hợp lệ, gateway cùng subnet; lỗi ghi NVS được trả về thay vì báo thành công. (Bản sửa này được đưa vào build cuối.)
- Đã thêm `espressif/esp_h264` 1.4.1 (software decoder) bên cạnh RTSP pull service. Chúng là dependency firmware; giải mã chưa được nối vào frame sink/LVGL renderer nên video trực tiếp vẫn chưa hoàn tất.
- Build cuối ESP-IDF 5.5.5 thành công: `0x2cb000` byte, còn `0x235000` byte (44%) trong phân vùng app nhỏ nhất. `wifi_manager.c` đã được biên dịch lại sau bản sửa IP tĩnh.
- `ctest --test-dir firmware/build/host-tests`: 2/2 qua (`camera_protocol`, `settings_keyboard`). `git diff --check` không phát hiện lỗi whitespace.

## Cập nhật xử lý — 06/10/2026 (phiên 2)

### Đã xử lý

- Bàn phím ảo không hiện khi bấm ô nhập trong Cài đặt: `lv_obj_move_foreground(s_kb)` trong `kb_focus_event_cb` (LVGL v9, thực chất gọi `lv_obj_move_to_index()`) — đúng nhóm hàm đã 2 lần gây lỗi hiển thị/layout trước đây trong dự án (header, badge Đơn hàng). Bàn phím chỉ tạo 1 lần, sau cùng, nên đã ở trên cùng theo đúng thứ tự dựng — gọi lại là thừa và rơi vào đúng lớp lỗi đã biết. Đã gỡ bỏ; xác nhận bằng ảnh chụp màn hình thật: bàn phím hiện đúng vị trí, không che bottom-nav.
- `network/rtsp_player.c` (pipeline RTSP pull → giải mã H.264 phần mềm → I420→RGB565 → `lv_image`) đã được viết từ phiên trước nhưng **chưa từng được thêm vào `firmware/main/CMakeLists.txt`** nên chưa từng biên dịch được — đây là lý do thật của "giải mã chưa nối vào renderer" ghi ở mục trên. Đã thêm vào SRCS; sửa lỗi biên dịch `ESP_MEDIA_CODEC_H264` (không tồn tại trong `esp_media_service_types.h`) → `ESP_FOURCC_H264` (đúng theo `esp_fourcc.h`, dùng khắp ví dụ chính thức của `esp_rtsp_service`). Build qua.
- Nối khung video vào tab Cân: `rtsp_player_init()` gọi trong `ui_weighing_create()`, chấm trạng thái + nhãn URL RTSP chuyển thành lớp phủ góc nhỏ trên ảnh video (trước đây là chữ to chiếm giữa khung — không còn đúng vì khung giờ có ảnh thật). Thêm `ui_weighing_set_visible()`: bật giải mã khi có phiên cân đang mở VÀ đang ở tab Cân, tắt khi rời tab/đăng xuất/hết phiên — tránh tốn CPU/băng thông khi không hiển thị.
- Đổi tên project ESP-IDF `can_may_xuc_v3` → `pig_weigh` (mục P2 trong bảng trên). Build lại xác nhận: `pig_weigh.bin`, cùng kích thước, còn 41% phân vùng app.
- Đã build + nạp thật qua COM6 ba lần (sau mỗi thay đổi), log serial xác nhận `[7/7] UI LVGL (Pig Weigh) OK`, không Guru Meditation/assert.

### Phát hiện mới trong phiên này

- **Sự cố bảo mật tự gây ra, đã sửa trước khi đẩy lên remote**: khi cập nhật thông tin đăng nhập camera thật do người dùng cung cấp, mật khẩu thật đã bị gán làm giá trị mặc định trong mã nguồn (`camera_receiver_init()`) và được đưa vào một commit cục bộ. Công cụ chặn tự động ("Credential Leakage") đã từ chối lệnh `git push` trước khi commit này rời máy — **chưa từng lên GitHub**. Đã sửa: mật khẩu mặc định trong mã nguồn đổi về chuỗi rỗng (bắt buộc nhập qua Cài đặt > Camera, chỉ lưu NVS). **Còn tồn tại cần người dùng quyết định**: commit chứa mật khẩu thật vẫn còn trong lịch sử Git cục bộ (chưa push) — việc xoá sạch đòi hỏi viết lại lịch sử (rebase/filter), là thao tác huỷ dữ liệu cần được xác nhận rõ ràng trước khi thực hiện.
- Xác minh lại kết nối RTSP `192.168.1.10:554` từ máy phát triển với thông tin đăng nhập thật (`admin`/mật khẩu thật, cổng 554): OPTIONS và DESCRIBE đều bị camera RST ngay sau khi gửi — đã thử 4 biến thể header/User-Agent khác nhau (tối giản, giả lập ffmpeg, giả lập VLC, bỏ qua OPTIONS vào thẳng DESCRIBE), kết quả giống hệt nhau. Kết nối TCP để KHÔNG gửi gì (idle 1s) thì không bị reset — loại trừ khả năng do ACL/giới hạn kết nối ở tầng TCP thuần. Việc reset xảy ra NGAY khi gửi bất kỳ request RTSP nào, bất kể nội dung — nghiêng về khả năng có thiết bị tầng mạng (firewall/router có RTSP ALG) giữa máy phát triển và camera can thiệp, hơn là do bản thân parser RTSP của camera kén chọn cú pháp. **Chưa kết luận được** vì đường mạng của ESP32 (qua ESP32-C6/WiFi) có thể khác đường mạng của máy phát triển — cần phiên cân thật trên thiết bị để xác nhận.
- Kiểm tra lại HTTPS cổng 443 (camera_client periodic check): vẫn lỗi bắt tay TLS nhưng mã lỗi mbedtls đổi từ `-0x7200` (lần trước) sang `-0x7780` (lần này) — hai lỗi khác nhau ở các lần thử khác nhau, chưa đủ dữ kiện để kết luận nguyên nhân cụ thể (cipher suite/TLS version/something else); cần bật `CONFIG_MBEDTLS_DEBUG` để lấy alert chi tiết từ phía server nếu cần điều tra tiếp.
- Đã theo dõi log serial thiết bị thật 2 lần (90s + 150s) sau khi nạp — **chưa quan sát được `rtsp_player` chạy thật** vì chưa có phiên cân nào được mở trên thiết bị trong các cửa sổ theo dõi đó (`rtsp_player_set_enabled(true)` chỉ kích hoạt khi `weighing_session_t.active == true`). **Chưa thể tuyên bố video đã phát hay chưa phát** — cần người vận hành mở một phiên cân thật trên thiết bị trong lúc theo dõi log để xác nhận.

### Còn tồn tại / chưa thể xác nhận (không đổi so với trước, trừ khi nêu trên)

- Toàn bộ các mục P0/P1/P2 còn lại trong bảng phát hiện ở trên (backend đồng bộ thật, khoá ghép sự kiện camera-phiên cân, token/TLS CA pinning camera, Secure Boot/NVS Encryption, xác minh Ethernet PHY, tài khoản/PIN demo) **chưa được xử lý trong phiên này** — đây là các quyết định chính sách/phối hợp nhà cung cấp/đốt eFuse cần xác nhận rõ ràng của người vận hành trước khi làm, không tự ý thực hiện.
- Codec/profile/độ phân giải thật của luồng RTSP camera vẫn chưa xác nhận được (xem mục RTSP reset ở trên) — decoder hiện giả định H.264; nếu camera trả H.265 hoặc codec khác, `rtsp_player.c` sẽ dừng với log lỗi thay vì hiển thị video (đã code chủ động từ chối thay vì hiển thị sai).
