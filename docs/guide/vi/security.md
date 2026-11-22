# Bảo mật và khôi phục

1. Dùng HTTPS. Bảo vệ tài khoản quản trị bằng mật khẩu mạnh, riêng biệt và 2FA. Không đưa mật khẩu hoặc API token vào repo, ảnh chụp hay build log.
2. Giữ quyền guest tắt trừ khi chủ động cấu hình. Chỉ cấp đường dẫn và thao tác cần thiết; kiểm tra các link chia sẻ và thời hạn.
3. Hạn chế truy cập server và thư mục dữ liệu. Chỉ bật kết nối outbound vào mạng riêng khi cần và đáng tin cậy. Không tin forwarded headers nếu client có thể bỏ qua proxy.
4. Sao lưu database nhất quán, bao gồm trạng thái SQLite WAL, cùng cấu hình và khóa ký. Cất bản sao an toàn ngoài host. Khôi phục thử vào môi trường riêng và xác nhận đăng nhập, dữ liệu trước khi tin cậy bản sao.
5. Tách secret và storage của staging, preview và production. Theo dõi log triển khai và cập nhật có kiểm soát. Giới hạn Cloudflare và sự cố nhà cung cấp có thể ảnh hưởng upload; thử client và kích thước tệp thực tế.

`/ping` chỉ cho biết tiến trình còn chạy, không chứng minh storage hay backup hoạt động. Cần thử thao tác tệp và khôi phục thực tế. Các biện pháp giảm rủi ro, không đảm bảo tuyệt đối an toàn.
