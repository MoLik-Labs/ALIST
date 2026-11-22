# Cloudflare: triển khai bản thử nghiệm cô lập

::: danger Đọc trước khi làm
Đây là hướng dẫn **thử nghiệm**, không phải triển khai Kova hoàn chỉnh. Không sử dụng tài khoản, tệp, token hay database production. Không ghi đè Worker `kova-inc` hoặc Worker khác đang dùng. Các driver, đăng nhập, chia sẻ, bảo mật và kiểm thử cloud thật chưa hoàn thiện.
:::

## Cần chuẩn bị

- Tài khoản Cloudflare trên **Workers Free** và một D1 **trống, riêng**. Trong ví dụ, database có tên `kova-cloudflare`; thay bằng tên của bạn nếu khác. Xem hạn mức và trạng thái gói trong [bảng giá D1](https://developers.cloudflare.com/d1/platform/pricing/). Khi vượt hạn mức Free, D1 có thể ngừng xử lý truy vấn; không tự nâng cấp gói để làm theo hướng dẫn.
- Node.js và npm trên máy của bạn, cùng URL clone HTTPS của **chính fork của bạn**. Các lệnh dưới đây chạy trên máy bạn, không phải trong dashboard.
- Chọn một tên Worker mới chưa tồn tại trong tài khoản, ví dụ `kova-cloudflare-isolated-test`. Tên này phân biệt với tên database. Kiểm tra trước trên Workers & Pages: deploy trùng tên có thể thay thế Worker hiện có.
- Nhánh `CLOUDFLARE` trong fork của bạn phải chứa các thay đổi này; nếu chúng còn nằm trong PR, cần review và merge PR vào đúng nhánh trước. Không phải branch `BETA` và không phải repo gốc.

**Không cần R2** cho bước thử giao diện và D1 này. Nếu dashboard R2 yêu cầu đăng ký thanh toán, dừng lại; không cần nâng cấp hoặc cung cấp thẻ để tiếp tục bước này.

## 1. Lấy đúng mã từ fork

Trong GitHub, mở **fork của bạn → Code**, sao chép URL HTTPS. Thay `YOUR_FORK_CLONE_URL` bên dưới bằng URL đó, rồi chạy:

```sh
git clone --branch CLOUDFLARE YOUR_FORK_CLONE_URL kova-cf-test
cd kova-cf-test
npm ci --ignore-scripts --prefix deploy/cloudflare-worker
```

Nếu tên nhánh của fork khác, thay `CLOUDFLARE` bằng chính xác tên nhánh của bạn. Nếu Git báo không tìm thấy nhánh hay file mẫu ở bước sau, dừng lại và kiểm tra fork/PR; không tự chuyển sang repo gốc.

## 2. Tạo cấu hình riêng

```sh
cp deploy/cloudflare-native/wrangler.isolated.example.toml deploy/cloudflare-native/wrangler.isolated.toml
```

Mở file `deploy/cloudflare-native/wrangler.isolated.toml` vừa sao chép và thay **ba** giá trị:

1. `name`: tên Worker thử nghiệm **mới** đã kiểm tra, không phải `kova-inc`.
2. `database_name`: `kova-cloudflare` hoặc tên D1 thử nghiệm thực tế.
3. `database_id`: Database ID trong trang chi tiết của **đúng D1 thử nghiệm** trên Cloudflare Dashboard.

Không chỉnh `binding = "DB"`, `ASSETS`, `main`, `migrations_dir` hoặc thêm R2 ở bước này. Không ghi secret, password, token hay private key vào file. File cấu hình bản sao được `.gitignore` loại trừ; không xóa quy tắc đó hoặc commit file này. Nếu có nhiều tài khoản Cloudflare, xác minh tên và tài khoản chứa đúng D1 trước khi tiếp tục.

## 3. Kiểm tra mà chưa triển khai

Chạy từ thư mục gốc `kova-cf-test`:

```sh
./deploy/cloudflare-worker/node_modules/.bin/wrangler deploy --dry-run --config deploy/cloudflare-native/wrangler.isolated.toml
```

Kiểm tra log có Worker code, `env.ASSETS` và `env.DB` đúng database thử nghiệm. `--dry-run` **không** tạo Worker, không chạy migration và không chứng minh D1 thật hoặc OAuth hoạt động. Nếu lỗi, dừng và gửi log đã xóa thông tin nhạy cảm; không đăng token hay callback có authorization code.

## 4. Chuẩn bị D1 trống

Chỉ sau khi xác nhận database là D1 **trống dành riêng cho thử nghiệm**, đăng nhập Wrangler bằng `./deploy/cloudflare-worker/node_modules/.bin/wrangler login` và xác nhận đúng tài khoản. Xem danh sách migration:

```sh
./deploy/cloudflare-worker/node_modules/.bin/wrangler d1 migrations list DB --remote --config deploy/cloudflare-native/wrangler.isolated.toml
```

`--remote` thao tác với Cloudflare thật. Xem lại ID và nội dung các file trong `deploy/cloudflare-native/migrations/` trước khi áp dụng. Khi chắc chắn, chạy:

```sh
./deploy/cloudflare-worker/node_modules/.bin/wrangler d1 migrations apply DB --remote --config deploy/cloudflare-native/wrangler.isolated.toml
```

Lệnh có thể hỏi xác nhận; kiểm tra trước khi đồng ý. Đây là thay đổi thật, không phải dry-run. Không chạy vào database có dữ liệu quan trọng. Migration `0009` xóa các yêu cầu kết nối Google đang chờ từ bản thử nghiệm cũ. Sau đó chạy lại lệnh `migrations list` để kiểm tra. Nếu thất bại, dừng và giữ nguyên log đã xóa secrets; đừng chạy SQL tùy ý hoặc xóa database.

## 5. Deploy Worker thử nghiệm riêng

Sau khi kiểm tra lại tên Worker mới và binding D1 trong file cấu hình, chạy:

```sh
./deploy/cloudflare-worker/node_modules/.bin/wrangler deploy --config deploy/cloudflare-native/wrangler.isolated.toml
```

Đây là deploy **thật** lên Worker có tên trong file. Mở URL Wrangler cung cấp; kiểm tra giao diện, `GET /ping`, CSS và JS. Mẫu này chỉ bật giao diện và endpoint sống; `/ping` thành công **không** chứng minh database, đăng nhập hay provider hoạt động. Các tính năng đăng nhập/provider vẫn tắt mặc định, nên chưa thể đăng nhập; không thêm secret hoặc bật flag chỉ để bỏ qua lỗi. Không bật lại build tự động của `kova-inc` trong quá trình này.

## 6. Phần chưa thể tự động hóa an toàn

Để đăng nhập và kết nối tài khoản thử nghiệm còn cần cấu hình Google OAuth, callback HTTPS đúng host, khóa vault và ánh xạ Google `sub` đã xác minh tới user D1. Hiện chưa có giao diện cấp user an toàn, quy trình phục hồi khóa, rate limiting hay kiểm thử Google/Dropbox thật; vì vậy chưa có hướng dẫn tự bật cho người dùng thật. Không dùng email thay `sub`, không tự chèn định danh chưa xác minh và không gửi secret/token trong chat. Xem `deploy/cloudflare-native/README.md` ở **fork và nhánh của bạn** để biết các flag thử nghiệm; chờ hướng dẫn và kiểm thử an toàn tiếp theo trước khi bật.

Các nút deploy trong README gốc có thể trỏ repo upstream, không tự chọn fork của bạn. Các bước trên cố ý dùng bản clone của bạn và Worker riêng. Khi báo lỗi, chỉ gửi bước thất bại, thông báo đã xóa bí mật, tên Worker thử nghiệm và tên database; không gửi khóa, password, token hoặc URL callback chứa code.
