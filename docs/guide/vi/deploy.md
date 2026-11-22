# Render và Docker

## Render Blueprint

1. Nếu muốn tự quản lý cập nhật, hãy fork repo, kết nối repo đó trong Render và tạo Blueprint từ `render.yaml` ở thư mục gốc. Bạn cũng có thể dùng nút deploy repo gốc.
2. Xem gói trả phí, vùng và ổ đĩa 1 GB trước khi xác nhận. Render build image native và gắn ổ tại `/opt/alist/data`.
3. Render tự tạo `ALIST_ADMIN_PASSWORD`. Xem giá trị trong phần environment của service và giữ bí mật. Mật khẩu chỉ được dùng khi tạo database lần đầu; đổi biến sau đó không đặt lại mật khẩu hiện có.
4. Mở URL HTTPS và đăng nhập bằng `admin`. Cấu hình các storage. Kiểm tra `/ping`, đăng nhập, thao tác tệp và khởi động lại; xác nhận ổ đã gắn và dữ liệu vẫn còn.
5. Bật xác thực hai yếu tố, sao lưu và thử khôi phục. Blueprint tắt tự động deploy; hãy kiểm tra và chủ động cập nhật.

SQLite trên một ổ đĩa dành cho **một instance**. Nhiều replica độc lập không có trạng thái nhất quán. Ổ đĩa và compute có thể phát sinh chi phí.

## Docker Compose

Trên máy Linux đã cài Docker và Compose, thay `YOUR_GITHUB_CLONE_URL` bằng URL HTTPS clone của **fork của bạn** (GitHub → repo của bạn → Code):

```sh
git clone YOUR_GITHUB_CLONE_URL kova
cd kova
docker compose up -d --build
```

Mở `http://localhost:5244` trên máy chạy Kova. Xem mật khẩu được tạo trong log đã bảo vệ, hoặc đặt `ALIST_ADMIN_PASSWORD` đủ mạnh trước lần khởi tạo đầu tiên. Không chia sẻ log chứa mật khẩu. `./data` chứa SQLite, khóa và cấu hình; hãy sao lưu. Bảo vệ truy cập mạng và dùng HTTPS trước khi công khai.

Với Railway, Fly.io và nền tảng khác, xem `docs/platform-deployment.md` trong repository và nhánh bạn chọn. Không phải mọi luồng cloud đã được thử trên tài khoản thực.
