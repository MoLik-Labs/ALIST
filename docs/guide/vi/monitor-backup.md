# Uptime và sao lưu

## Trước khi bật

`/ping` chỉ kiểm tra máy chủ trả `pong`. Nó **không** xác nhận đăng nhập, database, cloud drive hay bản sao lưu đang hoạt động. Console → Overview có nút kiểm tra tức thời từ trình duyệt, không chạy nền và không lưu lịch sử.

Bản theo dõi thử nghiệm dưới đây là một Worker **riêng**, lấy ý tưởng từ UptimeFlare; không phải bản sao đầy đủ, không có cảnh báo, nhiều monitor hay tự nâng cấp. Trang trạng thái là **công khai**. Không nhập URL hoặc bí mật vào trang; không dùng URL người khác mà chưa được phép. Lịch kiểm tra năm phút có thể giúp giảm ngủ do không truy cập, **không đảm bảo Render luôn bật**. Render vẫn có thể restart hoặc ngừng service vì hạn mức; 750 giờ miễn phí/tháng dùng chung workspace, nhiều service có thể hết hạn mức. Ping không làm filesystem bền vững. Xem [Render Free](https://render.com/docs/free) trước khi bật.

## Worker kiểm tra Render (tùy chọn, chưa thử trên tài khoản thật)

1. Sau khi thay đổi được merge và push, clone **fork của bạn** đúng nhánh như hướng dẫn Render, cài Node.js/npm, vào thư mục repo và chạy `npm ci --ignore-scripts --prefix deploy/cloudflare-worker`.
2. Trong Cloudflare Workers & Pages, kiểm tra gói Free, bật workers.dev nếu chưa có và chọn tên Worker **mới chưa tồn tại**. Không dùng `kova-inc`, Worker ứng dụng hoặc Worker chứa secrets: deploy có thể thay thế cấu hình của Worker cùng tên.
3. Tạo một **KV namespace mới riêng**, không dùng D1 `kova-cloudflare` hay KV chứa dữ liệu khác. Trong Dashboard tìm Workers KV → Create namespace, lưu Namespace ID. Xem [giới hạn KV](https://developers.cloudflare.com/kv/platform/limits/) và [giới hạn Workers](https://developers.cloudflare.com/workers/platform/limits/); không nâng cấp hoặc thêm thẻ nếu bạn không muốn.
4. Sao chép cấu hình, sau đó sửa bản sao:

```sh
cp deploy/uptime-monitor/wrangler.example.toml deploy/uptime-monitor/wrangler.local.toml
```

Sửa `name` thành tên Worker mới, `id` thành ID KV mới, `RENDER_ORIGIN` thành URL HTTPS **onrender.com** của chính bạn, ví dụ `https://ten-cua-ban.onrender.com` (không có `/ping`, dấu `/` cuối, đường dẫn hay query). Template cố ý không hoạt động khi chưa sửa. Không nhập mật khẩu. Bản sao đã được gitignore; đừng commit nó. Monitor không hỗ trợ custom domain ở bản này.
5. Kiểm tra account bằng `./deploy/cloudflare-worker/node_modules/.bin/wrangler login`, rồi chạy dry-run từ thư mục repo:

```sh
./deploy/cloudflare-worker/node_modules/.bin/wrangler deploy --dry-run --config deploy/uptime-monitor/wrangler.local.toml
```

Kiểm tra tên, URL, binding và tài khoản. Dry-run không tạo tài nguyên. Sau khi chắc chắn đây là Worker **mới** và KV riêng, chạy cùng lệnh nhưng bỏ `--dry-run` để deploy thực. Không thay đổi `kova-inc`. Mở URL Worker được in ra để xem trang trạng thái. Cron có thể mất tới 15 phút để áp dụng; đợi thêm một chu kỳ 5 phút. Dừng Worker/xóa cron khi không cần.

Một monitor dự kiến khoảng 288 lần ghi KV/ngày; quota Free tính chung tài khoản, trang công khai cũng tiêu thụ lượt đọc/request. Theo dõi quota trước khi dùng. Dữ liệu KV có độ trễ đồng bộ; lần chạy chồng nhau hoặc thiếu lịch có thể mất mẫu. `thành công / số mẫu` không phải SLA hay uptime chính xác; trạng thái cũ hơn 15 phút được đánh dấu stale. Cold start có thể lâu, kiểm tra timeout 75 giây. Mẫu chỉ giữ khoảng 24 giờ, không có thông báo. Chưa thử Cron, KV và Render thực nên không cam kết hoạt động hoặc luôn miễn phí.

## NodeWarden và backup

NodeWarden có chức năng backup tới WebDAV/S3 riêng. Kova có WebDAV, nhưng tương thích **end-to-end với NodeWarden thật chưa được xác minh**. Đã thử cục bộ chuỗi MKCOL, PUT, HEAD, PROPFIND, GET, DELETE bằng dữ liệu giả. Muốn thử tiếp, dùng tài khoản Kova **riêng**, giới hạn đường dẫn và quyền WebDAV vào một mount bền vững riêng, kết nối HTTPS `/dav` theo cấu hình NodeWarden, rồi thử backup và khôi phục dữ liệu giả trong môi trường tách biệt trước. Kova yêu cầu `X-OTP-Code` cho mỗi request WebDAV Basic nếu tài khoản bật 2FA; client NodeWarden đã kiểm tra chỉ gửi Basic, nên tài khoản đó chưa kết nối trực tiếp được. Dùng tài khoản riêng không bật TOTP là đánh đổi bảo mật: giới hạn đường dẫn và bảo vệ mật khẩu; không hạ bảo mật tài khoản admin. Không gửi mật khẩu vault, master key hay backup lên chat. Không dùng Catbox cho backup vault vì URL Catbox công khai.

Kova **chưa có tự động sao lưu/khôi phục an toàn** trong bản này. NodeWarden tự backup không đồng nghĩa backup Kova. Render Free không giữ SQLite hay dữ liệu mount local sau spin-down/restart; không dùng nó làm đích backup. Chọn nơi lưu bền vững ngoài host, bảo vệ khóa và mã hóa bản sao, lưu cả metadata và attachments, kiểm tra khôi phục định kỳ. Sao chép `data.db` khi SQLite đang chạy có thể thiếu WAL; xem [bảo mật và khôi phục](./security). Không tự động xóa backup cũ trước khi có bản mới đã xác minh.
