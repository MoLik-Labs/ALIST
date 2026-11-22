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

## Render Free: thử từ public repo, không cần Blueprint

::: danger Chỉ thử nghiệm, không phải lưu trữ ổn định
Render Free **không có ổ đĩa bền vững**. SQLite, tài khoản, khóa ký, cấu hình storage và tệp lưu trong container có thể mất khi ngủ, restart hoặc deploy lại; dữ liệu trên cloud drive riêng không tự mất nhưng kết nối có thể mất. Không kết nối tài khoản hoặc dùng dữ liệu thật. Free ngủ sau khoảng 15 phút không có truy cập, lần mở sau có thể chờ khoảng một phút; giới hạn giờ chạy, build và băng thông có thể khiến dịch vụ bị dừng. Xem [giới hạn Render Free](https://render.com/docs/free). Không có cách nào cấu hình repo hiện tại thành Render Free **vừa ổn định vừa lưu dữ liệu bền vững**: Kova hiện dùng SQLite và chưa hỗ trợ PostgreSQL cho metadata.
:::

1. Đảm bảo mã mới đã được merge vào nhánh trong **public repo của bạn**. Render chỉ nhìn thấy commit đã push, không nhìn thấy bản local hay PR chưa merge.
2. Render Dashboard → **New → Web Service** → chọn phương thức dùng public Git repository và nhập URL repo của bạn (không dùng nút README trỏ repo gốc). Nếu giao diện yêu cầu kết nối GitHub, cấp quyền theo hướng dẫn Render. Chọn đúng branch; để Root Directory trống.
3. Chọn runtime **Docker**, Dockerfile `./Dockerfile`, context `.` nếu được hỏi, compute **Free**. Không thêm disk; đừng chọn Starter. Tắt auto-deploy nếu muốn tự kiểm soát. Trong Advanced, đặt health check `/ping` và biến `KOVA_DISPOSABLE_DEMO=true` để log cảnh báo mất dữ liệu.
4. Đặt `ALIST_ADMIN_PASSWORD` thành mật khẩu ngẫu nhiên mạnh trong Environment **trước lần deploy đầu**. Không commit hoặc gửi mật khẩu. Mật khẩu chỉ khởi tạo khi database mới; sau lần mất database nó lại khởi tạo từ biến này. Nếu bỏ trống, mật khẩu mới được in trong log khởi tạo: log này là bí mật.
5. Kiểm tra màn hình xác nhận: chỉ gói Free, không disk, không dịch vụ trả phí; không xác nhận nếu yêu cầu phương thức thanh toán mà bạn không muốn cung cấp. Việc Render có yêu cầu xác minh tài khoản/thẻ tùy chính sách của họ, dự án không thể vượt qua yêu cầu đó. Sau deploy, mở URL HTTPS, kiểm tra `/ping`, đăng nhập `admin` và thử với dữ liệu giả. Đây không phải kiểm thử độ bền.

Nếu vẫn muốn Blueprint, tạo **Blueprint mới riêng** và chọn Blueprint Path `deploy/render/render.free-demo.yaml`; nó tạo mật khẩu tự động trong Environment và không tạo disk. Không chuyển Blueprint/Service có dữ liệu đang chạy sang mẫu này: việc thay đổi cấu hình hoặc xóa tài nguyên có thể gây mất dữ liệu. Blueprint gốc `render.yaml` vẫn là gói trả phí có disk; mặc định không dùng cho Free. Chưa xác minh các bước này trên tài khoản Render thực.

## Gắn liên kết Catbox công khai (chỉ đọc)

::: danger Catbox không phải kho riêng tư
Tệp Catbox là **công khai** với người có URL; đặt quyền hoặc mật khẩu trong Kova không bảo vệ URL gốc. Không dùng cho dữ liệu nhạy cảm. Đây chỉ là danh sách thủ công các liên kết đã có, **không đăng nhập hoặc liệt kê tài khoản**, không tải lên/xóa/đổi tên. Xóa mount không xóa tệp trên Catbox.
:::

Sau khi phiên bản có driver Catbox đã được merge và deploy, vào **Console → Storages → Add storage**, chọn `Catbox public links (read-only)` và đặt Mount path riêng, ví dụ `/catbox`. Nhập các liên kết **của bạn**, mỗi dòng một tệp theo mẫu (đây chỉ là ví dụ, không phải liên kết thật):

```text
/anh/vi-du.jpg|https://files.catbox.moe/abc123.jpg|12345
/tai-lieu.txt|https://files.catbox.moe/def456.txt|2048
```

Số cuối là kích thước thật theo byte; cung cấp sai có thể ảnh hưởng tải xuống. Chỉ nhận HTTPS trực tiếp tại `files.catbox.moe`, không có query, fragment hoặc đường dẫn con. Thư mục là ảo, cấu hình danh sách trong mount và lưu lại khi thêm/bớt liên kết. Kiểm tra trạng thái mount và mở tệp công khai thử; môi trường kiểm thử chưa kết nối Catbox thật. Catbox có chính sách dung lượng, nội dung và lưu trữ riêng, có thể thay đổi: xem [API](https://catbox.moe/tools.php), [FAQ](https://catbox.moe/faq.php), và [điều khoản](https://catbox.moe/legal.php). Cần storage bền vững cho cấu hình mount; Render Free có thể mất cấu hình khi ngủ hoặc khởi động lại.
