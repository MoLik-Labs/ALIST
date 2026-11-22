# Chọn cách triển khai

Kova gồm giao diện web và server xử lý tài khoản, tệp, kết nối lưu trữ và API. Chỉ đưa HTML tĩnh lên mạng **không** tạo ra server đó.

| Nhu cầu | Lựa chọn hiện tại | Dữ liệu |
| --- | --- | --- |
| Server quản lý sẵn, dễ bắt đầu | [Render Blueprint](./deploy) | Cần ổ đĩa bền vững trả phí |
| Máy của bạn | [Docker Compose](./deploy) | Giữ và sao lưu `./data` |
| Tên miền Cloudflare | [Proxy Worker hoặc Pages](./cloudflare) | Cần server riêng với lưu trữ bền vững |
| Heroku dyno thông thường | Chỉ dùng demo tạm | Tệp dyno bị xóa; tài khoản và khóa có thể mất |
| GitHub Pages | Trang tài liệu này | Chỉ trang tĩnh, không chạy API Kova |

Nút Render trong repo trỏ tới dự án gốc. Nếu đã fork, hãy chọn fork của bạn trong dashboard nhà cung cấp để triển khai đúng mã của bạn. Xem chi phí và quyền truy cập trước khi xác nhận.

::: danger Sao lưu trước khi nâng cấp
Sao lưu nhất quán database, khóa ký và cấu hình; kiểm tra khôi phục. Ổ đĩa không phải bản sao lưu. Object storage từ xa không tự lưu tài khoản và cài đặt Kova.
:::
