# Bảo mật và khôi phục

1. Dùng HTTPS. Bảo vệ tài khoản quản trị bằng mật khẩu mạnh, riêng biệt và 2FA. Không đưa mật khẩu hoặc API token vào repo, ảnh chụp hay build log.
2. Giữ quyền guest tắt trừ khi chủ động cấu hình. Chỉ cấp đường dẫn và thao tác cần thiết; kiểm tra các link chia sẻ và thời hạn.
3. Hạn chế truy cập server và thư mục dữ liệu. Chỉ bật kết nối outbound vào mạng riêng khi cần và đáng tin cậy. Không tin forwarded headers nếu client có thể bỏ qua proxy.
4. Sao lưu database nhất quán, bao gồm trạng thái SQLite WAL, cùng cấu hình và khóa ký. Cất bản sao an toàn ngoài host. Khôi phục thử vào môi trường riêng và xác nhận đăng nhập, dữ liệu trước khi tin cậy bản sao.
5. Tách secret và storage của staging, preview và production. Theo dõi log triển khai và cập nhật có kiểm soát. Giới hạn Cloudflare và sự cố nhà cung cấp có thể ảnh hưởng upload; thử client và kích thước tệp thực tế.

`/ping` chỉ cho biết tiến trình còn chạy, không chứng minh storage hay backup hoạt động. Cần thử thao tác tệp và khôi phục thực tế. Các biện pháp giảm rủi ro, không đảm bảo tuyệt đối an toàn.

### Upload Local và sao chép S3

Upload Local có kích thước khai báo sẽ bị từ chối nếu thiếu hoặc thừa dữ liệu; file đích cũ được giữ khi xảy ra các lỗi này. Dữ liệu file tạm được đồng bộ trước khi đổi tên, nhưng điều này không bảo đảm an toàn khi mất điện và không thay thế backup. Sao chép phía máy chủ S3 yêu cầu `CopyObjectResult` hợp lệ có ETag trước khi báo thành công; nhà cung cấp không trả kết quả chuẩn này có thể bị từ chối. Các kiểm tra này không xác minh byte tại đích hoặc làm thao tác di chuyển giữa các ổ trở nên nguyên tử. Hãy thử bằng dữ liệu giả trên nhà cung cấp của bạn.

Danh sách S3 đọc các trang tiếp theo, tối đa 100.000 mục hoặc 1.000 trang; lỗi phân trang hoặc lỗi ở trang sau làm cả danh sách báo lỗi thay vì âm thầm trả thiếu. Khi sao chép từ nguồn HTTP, chỉ chấp nhận phản hồi HTTP 200 hoàn chỉnh; phản hồi chuyển hướng và trang lỗi bị từ chối. Các kiểm tra này mới được thử với máy chủ giả lập cục bộ, chưa thử với nhà cung cấp S3 thực.

Tra cứu đối tượng S3 phân biệt file, marker thư mục và prefix có nội dung với
đường dẫn không tồn tại. HEAD bị từ chối hoặc lỗi liệt kê được báo lỗi thay vì
âm thầm coi là thư mục tồn tại. Mới thử với máy chủ giả lập; vẫn cần kiểm thử
tách biệt trên từng nhà cung cấp.

## MCP chỉ đọc thử nghiệm

Máy chủ native có endpoint MCP JSON-RPC không trạng thái, **mặc định tắt**, tại `POST /api/mcp`. Đặt `ALIST_ENABLE_MCP=true` và khởi động lại để bật; biến này không được lưu vào `config.json`. Dùng HTTPS qua reverse proxy tin cậy và bảo vệ token như mật khẩu; tránh dùng token admin hoặc system. Tạo tài khoản riêng với base path giới hạn và chỉ cấp quyền MCP access cho scope cần thiết. Xác thực bằng `Authorization: Bearer <login-token>`; guest không được truy cập.

Phiên bản giao thức hỗ trợ là `2025-03-26`, gồm `initialize`, `ping`, `tools/list` và `tools/call` cho `fs_list`, `fs_get` chỉ đọc metadata. Đường dẫn tuyệt đối tính **bên trong base path của tài khoản**. `fs_list` trả tối đa 50 mục mỗi trang, tùy chọn `page`; từng mục phải có quyền MCP và quyền hiển thị thông thường. Đường dẫn được bảo vệ bằng mật khẩu metadata, kể cả mật khẩu kế thừa khi có metadata gần hơn, bị loại trừ với admin và quyền bỏ qua mật khẩu; MCP không nhận mật khẩu. Chưa hỗ trợ tải file, URL thô, sửa/xóa, stdio, SSE, phiên, OAuth hoặc đầy đủ tool upstream. Driver vẫn có thể tải và lọc toàn bộ thư mục trước khi phân trang.

Mọi yêu cầu có header `Origin`, kể cả cùng nguồn từ trình duyệt, đều bị từ chối và không được cấp CORS. Client ngoài trình duyệt phải bỏ header này. Endpoint giới hạn body 64 KiB (HTTP server có thể đã buffer trước khi kiểm tra). GET trả 405; không có server push. Đã kiểm thử khởi tạo và hai tool qua HTTP nội bộ với SDK MCP Python chính thức 1.30.0 trên instance tạm; chưa kiểm chứng các client khác, HTTPS proxy hay môi trường production. Tắt biến môi trường nếu không dùng.

## Tra cứu hash VirusTotal thử nghiệm (API admin)

Chủ động bật bằng cách đặt `KOVA_VIRUSTOTAL_API_KEY` trong biến môi trường bí mật của service rồi khởi động lại. Không lưu khóa vào mã nguồn, `config.json`, URL hoặc chat. Đây là API chỉ dành cho admin, **chưa có nút trên web**. Gửi `POST /api/admin/security/virustotal` có xác thực với JSON `{ "sha256": "<64 ký tự hex>" }`. Response theo envelope API thông thường; `data` gồm tổng số `malicious`, `suspicious`, `harmless`, `undetected`, `timeout` và `last_analysis_date` (giây Unix). Mã `404` trong envelope nghĩa là chưa có báo cáo, **không** nghĩa là file sạch. Không kết quả nào chứng minh file an toàn; báo cáo có thể cũ hoặc thiếu.

Chỉ gửi hash đã cung cấp tới VirusTotal sau yêu cầu chủ động; hệ thống **không** tự đọc, tính hash, upload hay quét file. Hash vẫn có thể tiết lộ sự quan tâm tới file đã biết, nên cần quyền phù hợp trước khi tra cứu. Endpoint không trả khóa và không ghi khóa vào config. Kết nối dùng đích HTTPS cố định, kiểm tra chứng chỉ, không redirect, timeout 10 giây và giới hạn response 1 MiB. Mỗi process chờ tối thiểu 16 giây giữa hai lần gửi; đây **không** phải giới hạn ngày hay giới hạn chung giữa các lần khởi động, replica và ứng dụng khác dùng cùng khóa. [Điều khoản/quota Public API của VirusTotal](https://docs.virustotal.com/v2.0/reference/public-vs-private-api) cấm dùng trong sản phẩm, dịch vụ thương mại hoặc quy trình kinh doanh và nêu 4 request/phút, 500/ngày; cần tự kiểm tra license và điều khoản mới nhất. Chưa thử kết nối VirusTotal thực hay xác nhận response từ provider.
