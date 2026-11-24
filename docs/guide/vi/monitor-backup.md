# Uptime và sao lưu

## Trước khi bật

`/ping` chỉ kiểm tra máy chủ trả `pong`. Nó **không** xác nhận đăng nhập, database, cloud drive hay bản sao lưu đang hoạt động. Console → Overview có nút kiểm tra tức thời từ trình duyệt. Sau khi deploy monitor riêng, nhập origin `https://<worker>.<subdomain>.workers.dev` trong mục giám sát và nhấn kết nối: Kova hiển thị mẫu từ Worker và cập nhật mỗi phút khi trang đang mở. Origin lưu trong trình duyệt đó, không phải cấu hình dùng chung; xóa ô và nhấn kết nối để ngừng. Kova chỉ truy cập đường dẫn status cố định qua HTTPS, không chuyển tiếp mật khẩu. Không hỗ trợ domain riêng cho liên kết này.

Bản theo dõi thử nghiệm dưới đây là một Worker **riêng**, lấy ý tưởng từ UptimeFlare; không phải bản sao đầy đủ, không có cảnh báo, nhiều monitor hay tự nâng cấp. Trang trạng thái là **công khai**. Không nhập URL hoặc bí mật vào trang; không dùng URL người khác mà chưa được phép. Lịch kiểm tra năm phút có thể giúp giảm ngủ do không truy cập, **không đảm bảo Render luôn bật**. Render vẫn có thể restart hoặc ngừng service vì hạn mức; 750 giờ miễn phí/tháng dùng chung workspace, nhiều service có thể hết hạn mức. Ping không làm filesystem bền vững. Xem [Render Free](https://render.com/docs/free) trước khi bật.

Tự ping URL công khai từ cron **bên trong chính service** chỉ chạy khi tiến trình còn sống; nếu service đã ngủ, dừng hoặc restart thì cron đó cũng dừng và không thể tự đánh thức. Ping có thể giảm khả năng ngủ nhưng không đảm bảo luôn hoạt động, và không thay thế lưu trữ bền vững. Vì vậy monitor bên ngoài dưới đây là lựa chọn độc lập; không tự bật hay yêu cầu thay đổi service hiện tại.

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

Một monitor dự kiến khoảng 288 lần ghi KV/ngày; quota Free tính chung tài khoản, trang công khai và Console đang mở cũng tiêu thụ lượt đọc/request (khoảng một lần mỗi phút cho mỗi trình duyệt). Theo dõi quota trước khi dùng. Dữ liệu KV có độ trễ đồng bộ; lần chạy chồng nhau hoặc thiếu lịch có thể mất mẫu. `thành công / số mẫu` không phải SLA hay uptime chính xác; trạng thái cũ hơn 15 phút được đánh dấu stale. Cold start có thể lâu, kiểm tra timeout 75 giây. Mẫu chỉ giữ khoảng 24 giờ, không có thông báo. Chưa thử Cron, KV và Render thực nên không cam kết hoạt động hoặc luôn miễn phí.

## NodeWarden và backup

NodeWarden có chức năng backup tới WebDAV/S3 riêng. Kova có WebDAV, nhưng tương thích **end-to-end với NodeWarden thật chưa được xác minh**. Đã thử cục bộ client upload WebDAV thật của NodeWarden tại commit `954bd05c10535e5828e43103caa821514c333f7c` với dữ liệu giả: upload, liệt kê, tải, kiểm tra và xóa. Thử nghiệm chuyển hostname giả sang loopback, không kiểm tra TLS, Worker thực hay khôi phục vault. Muốn thử tiếp, dùng tài khoản Kova **riêng**, giới hạn đường dẫn và quyền WebDAV vào một mount bền vững riêng, kết nối HTTPS `/dav` theo cấu hình NodeWarden, rồi thử backup và khôi phục dữ liệu giả trong môi trường tách biệt trước. Kova yêu cầu `X-OTP-Code` cho mỗi request WebDAV Basic nếu tài khoản bật 2FA; client NodeWarden đã kiểm tra chỉ gửi Basic, nên tài khoản đó chưa kết nối trực tiếp được. Dùng tài khoản riêng không bật TOTP là đánh đổi bảo mật: giới hạn đường dẫn và bảo vệ mật khẩu; không hạ bảo mật tài khoản admin. Không gửi mật khẩu vault, master key hay backup lên chat. Không dùng Catbox cho backup vault vì URL Catbox công khai.

Công cụ thử nghiệm bên dưới chỉ sao lưu metadata Kova; NodeWarden tự backup không đồng nghĩa backup Kova. Render Free không giữ SQLite hay dữ liệu mount local sau spin-down/restart; không dùng nó làm đích backup. Chọn nơi lưu bền vững ngoài host, bảo vệ khóa và mã hóa bản sao, lưu cả metadata và attachments, kiểm tra khôi phục định kỳ. Sao chép `data.db` khi SQLite đang chạy có thể thiếu WAL; xem [bảo mật và khôi phục](./security). Không tự động xóa backup cũ trước khi có bản mới đã xác minh.


## Sao lưu metadata Kova được mã hóa (thử nghiệm trên máy có storage bền vững)

Công cụ tùy chọn `tools/backup/kova_backup.py` chụp SQLite bằng SQLite Backup API (bao gồm WAL), mã hóa AES-256-GCM và kiểm tra giải mã/tính toàn vẹn SQLite sau mỗi lần chạy lịch. Nó **chỉ** lưu `data.db` và `config.json`, tối đa 64 MiB database và 1 MiB config. Không sao lưu tệp Local, tệp cloud, NodeWarden, attachments, biến môi trường, backup key hay `ALIST_JWT_SECRET` đặt qua môi trường. Nếu server dùng biến đó, phải giữ đúng secret riêng biệt để khôi phục credential; không gửi hoặc commit secret. `verify` không thay thế kiểm thử khôi phục và đăng nhập.

Công cụ chạy độc lập bằng Python 3.11+ có `sqlite3` và thư viện `cryptography`; **không có sẵn trong image Render**, có thể gửi bản mã hóa tới WebDAV khi bạn cấu hình, không chạy khi host ngủ và không giúp Render Free giữ dữ liệu. Chỉ dùng trên host có quyền đọc cùng database và nơi lưu bền vững riêng. Lưu bản mã hóa sang nơi khác và lưu key ở **nơi khác nữa**; mất key sẽ không giải mã được. Không bỏ key vào Git, Catbox hay cùng thư mục backup. Bảo vệ các thư mục khỏi người dùng khác; tool yêu cầu thư mục backup và thư mục cha khôi phục thuộc user đang chạy và mode 0700. Trong lúc kiểm tra/khôi phục, một bản SQLite **plaintext tạm** được ghi vào `TMPDIR` của máy chạy, sau đó xóa; đặt `TMPDIR` trên filesystem riêng tư/mã hóa và hiểu rằng xóa không bảo đảm xóa vật lý.

Ví dụ chạy từ clone có thay đổi mới, thay `/duong-dan-data` bằng đường dẫn thực trên host và dùng shell an toàn (không nhập mật khẩu vault):

```sh
python3 -m venv .venv
.venv/bin/pip install -r tools/backup/requirements.txt
mkdir -p "$HOME/kova-keys" "$HOME/kova-backups" "$HOME/kova-temp"
chmod 700 "$HOME/kova-keys" "$HOME/kova-backups" "$HOME/kova-temp"
export TMPDIR="$HOME/kova-temp"
.venv/bin/python tools/backup/kova_backup.py create-key "$HOME/kova-keys/backup.key"
.venv/bin/python tools/backup/kova_backup.py create /duong-dan-data "$HOME/kova-backups/first.kvbak" "$HOME/kova-keys/backup.key"
.venv/bin/python tools/backup/kova_backup.py verify "$HOME/kova-backups/first.kvbak" "$HOME/kova-keys/backup.key"
.venv/bin/python tools/backup/kova_backup.py restore "$HOME/kova-backups/first.kvbak" "$HOME/kova-keys/backup.key" "$HOME/kova-temp/restore-test"
```

`create-key`, `create` và `restore` từ chối ghi đè; đừng chạy `create-key` lần hai để thay key. `restore-test` phải chưa tồn tại. Khôi phục cấu hình đổi đường dẫn DB/temp/log về thư mục mới; mount có thể vẫn chỉ tới dữ liệu thật: **cô lập mạng và storage trước khi khởi động thử**. Kiểm tra đăng nhập, mount và phục hồi các tệp/secret ngoài backup. Sao chép backup ra nơi khác an toàn, và xác nhận có bản key khôi phục ở vị trí riêng.

Muốn chạy tự động mỗi ngày trên host còn hoạt động, chạy tiến trình sau dưới systemd/supervisor của host (không tự cài hay tự khởi động lại):

```sh
.venv/bin/python tools/backup/kova_backup.py schedule /duong-dan-data "$HOME/kova-backups" "$HOME/kova-keys/backup.key" --interval 86400
```

Lần đầu chạy ngay, sau đó theo khoảng giây (tối thiểu 3600); lỗi được báo log, không có cảnh báo bên ngoài, không tự xóa backup cũ. Giám sát dung lượng và log, lập chính sách retention thủ công sau khi xác minh bản mới. Nếu database cấu hình đường dẫn khác `data.db` trong thư mục data, hoặc config thay đổi khi chụp, tool từ chối. Đường dẫn database tương đối được tính từ thư mục làm việc của tool: chạy cùng working directory với server hoặc sửa đường dẫn theo hướng dẫn quản trị. Đây là bản thử nghiệm, chưa kiểm thử restore với dữ liệu người dùng hay dịch vụ thực.

### Tự gửi bản mã hóa tới WebDAV của bạn

Cần một thư mục WebDAV **đã tồn tại**, HTTPS với chứng chỉ hợp lệ và tài khoản Basic riêng có quyền HEAD, PUT, GET. Tool không tạo thư mục, không chuyển hướng, không hỗ trợ URL chứa user/password, query hoặc fragment, không tắt kiểm tra TLS. Chỉ cấu hình đích do bạn tin cậy; đây là CLI chạy trên host của bạn, không phải API công cộng và không phải SSRF sandbox. Không dùng tài khoản admin nếu tạo được tài khoản giới hạn đường dẫn. Tài khoản WebDAV bật TOTP của Kova cần mã mỗi request nên không dùng được với tool này.

Tạo file credential cục bộ ngoài repository (không gửi username/password trong chat). Lệnh dưới hỏi trong terminal; mật khẩu không hiện khi nhập và file mới được tạo với mode 0600, không ghi đè:

```sh
.venv/bin/python - <<'PY'
import getpass, json, os
path = os.path.expanduser('~/kova-keys/webdav.json')
user = input('WebDAV username: ')
password = getpass.getpass('WebDAV password: ')
fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600)
with os.fdopen(fd, 'w') as f:
    json.dump({'username': user, 'password': password}, f)
PY
```

Thay URL bên dưới bằng URL thư mục WebDAV **của bạn** (không gồm mật khẩu hay query), giữ nguyên tên file credential. Tạo backup mới với tên duy nhất, rồi thử tải bản mã hóa đó:

```sh
.venv/bin/python tools/backup/kova_backup.py upload-webdav "$HOME/kova-backups/first.kvbak" "$HOME/kova-keys/backup.key" 'https://webdav-cua-ban.example/thu-muc-backup' "$HOME/kova-keys/webdav.json"
```

Muốn chạy lịch với WebDAV, dùng lệnh `schedule` phía trên và thêm **trên cùng dòng**:

```sh
--webdav-url 'https://webdav-cua-ban.example/thu-muc-backup' --webdav-credentials "$HOME/kova-keys/webdav.json"
```

Mỗi lần chụp dùng tên ngẫu nhiên mới; tool xác thực bản mã hóa local, HEAD kiểm tra tên chưa tồn tại, PUT với `If-None-Match: *`, sau đó GET và so sánh SHA-256 byte-for-byte. Một số WebDAV có thể bỏ qua điều kiện PUT; HEAD không loại được race nên dùng thư mục riêng và không để nhiều client ghi cùng tên. Khi lỗi upload/verify, bản local vẫn còn; tool không xóa bản cũ hay tự tải lại bản remote. Để khôi phục, tải ciphertext về bằng client WebDAV an toàn, chạy `verify` và `restore` với key riêng. Luồng này đã thử TLS cục bộ và dữ liệu giả, **chưa thử với WebDAV của bạn**. Không cung cấp mật khẩu, credential file hay key cho tôi.
