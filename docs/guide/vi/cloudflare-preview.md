# Dùng thử giao diện Cloudflare

::: danger Chỉ dùng dữ liệu thử nghiệm
Giao diện này kết nối backend Cloudflare độc lập **đang phát triển**, không phải Kova đầy đủ. Chưa có upload, chia sẻ, phân quyền hoàn chỉnh, đầy đủ driver hay bảo vệ production. Chưa kiểm thử với Google, Dropbox hoặc tài nguyên Cloudflare thật. Không dùng tài khoản hoặc tệp nhạy cảm.
:::

## 1. Xem giao diện trên máy

Cài Node.js và chạy tại thư mục gốc repository:

```sh
npm ci --ignore-scripts
npm ci --ignore-scripts --prefix deploy/cloudflare-worker
deploy/cloudflare-worker/node_modules/.bin/wrangler dev --config deploy/cloudflare-native/wrangler.local.toml --local --ip 127.0.0.1 --port 8787 --var KOVA_NATIVE_TOKEN:local-test-secret-at-least-32-characters
```

Mở `http://127.0.0.1:8787/` trên **chính máy chạy Wrangler**. Đây là cấu hình local với bucket R2 giả lập và token ví dụ công khai. Dừng bằng Ctrl+C. Không deploy file cấu hình local hoặc dùng token mẫu trên Internet. Chế độ này chỉ cho xem giao diện: không có D1, Google OAuth hay phiên người dùng nên đăng nhập chưa hoạt động. Chạy `npm run test:cf-ui` để kiểm tra tự động bằng mô phỏng DOM và kiểm tra asset; mô phỏng không thay thế trình duyệt thật.

## 2. Các thao tác trong bản thử nghiệm

Khi backend thử nghiệm được cấu hình riêng và có tài khoản đã cấp trước, nút đăng nhập Google mở luồng thử nghiệm. Bật `KOVA_EXPERIMENTAL_BROWSER_AUTH=enabled` cùng các flag đăng nhập thử nghiệm: sau callback, trình duyệt quay về giao diện với cookie phiên Secure, HttpOnly. Nhập bearer token thủ công vẫn dành cho thử nghiệm; nếu không bật flag này, callback trả bearer token dạng JSON. **Không gửi token vào chat, URL, log hoặc ảnh chụp màn hình.** Đăng nhập này chưa phù hợp người dùng thật.

Sau đăng nhập, chọn kết nối để duyệt thư mục; nút **Xem thêm** lấy trang tiếp theo. Có thể thử thêm Dropbox bằng access token ngắn hạn của tài khoản thử nghiệm. Nút kết nối Google Drive và Dropbox mở cửa sổ riêng khi các flag OAuth tương ứng được cấu hình; sau khi callback hoàn tất, trở lại trang và bấm **Làm mới**. Chỉ bắt đầu một luồng kết nối cho mỗi nhà cung cấp tại một thời điểm trên cùng trình duyệt. Xóa kết nối chỉ xóa thông tin tại Kova, không tự thu hồi quyền ở Google/Dropbox; hãy thu hồi riêng trong tài khoản nhà cung cấp. Tải tệp qua giao diện thử nghiệm giới hạn 8 MiB; tệp Google Workspace được yêu cầu xuất PDF và có thể không được hỗ trợ cho mọi loại tài liệu.

## 3. Thiết lập backend và kiểm tra an toàn

Để thử nhiều người dùng cần Worker riêng, D1 riêng với migrations, vault key riêng và cấu hình OAuth cùng định danh được cấp trước. Xem [README Cloudflare-native](https://github.com/MoLik-Labs/Kova-INC/blob/CLOUDFLARE/deploy/cloudflare-native/README.md) về bindings, biến, giới hạn và rủi ro. Không trỏ cấu hình thử nghiệm vào Worker hoặc dữ liệu production. Chỉ bật các flag thử nghiệm trong môi trường cô lập; v1 dùng token chung có quyền trên toàn bộ dữ liệu nhà cung cấp và không phải bảo mật đa người dùng.

Trước khi dùng thật vẫn cần hoàn thiện và rà soát bảo mật đăng nhập, bảo vệ chống lạm dụng, phục hồi khóa, phân quyền/chia sẻ, các driver, kiểm thử trình duyệt và kiểm thử tài nguyên thực tế. Nếu giao diện không tải CSS/JS, kiểm tra `GET /`, `/app.css`, `/app.js`, binding `ASSETS`, cấu hình build và CSP; không đăng tải toàn bộ repository.
