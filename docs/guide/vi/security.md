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
