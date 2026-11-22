# Xuất bản trang tài liệu

Trang này dùng giao diện mặc định của [VitePress](https://github.com/vuejs/vitepress), giấy phép MIT. Đây là website riêng với ứng dụng Kova: GitHub Pages chỉ phục vụ tài liệu tĩnh, không chạy API hay lưu tệp của bạn.

1. Fork repository. Trong **Settings → Pages**, chọn **GitHub Actions** làm nguồn build và deploy. Đảm bảo repository được phép dùng Actions và Pages.
2. Trong **Actions**, mở **Deploy documentation to GitHub Pages**, chọn **Run workflow** và nhánh có workflow này. Thay đổi tài liệu trên `BETA` hoặc `main` cũng tự kích hoạt. Nếu có quy tắc bảo vệ environment, hãy phê duyệt khi được yêu cầu.
3. Mở URL ở deployment đã hoàn thành. Dùng menu ngôn ngữ để kiểm tra cả tiếng Việt lẫn tiếng Anh, các liên kết và tìm kiếm. Build xanh không tự chứng minh website đã hoạt động.
4. Project site thường nằm ở `https://<owner>.github.io/<repository>/`; build tự xác định đường dẫn này. Với tên miền riêng hoặc thư mục khác, đặt biến Actions `DOCS_BASE` tại **Settings → Secrets and variables → Actions → Variables**, ví dụ `/` hoặc `/manual/`, rồi chạy lại workflow. Giá trị phải bắt đầu và kết thúc bằng `/`. Xóa biến để trở lại chế độ tự xác định.

Nếu CSS hoặc JavaScript không tải, kiểm tra URL thực tế, `DOCS_BASE` và log workflow. Pages cần được bật và có thể cần cấp quyền hoặc phê duyệt environment. Không đưa mật khẩu hoặc nội dung riêng tư lên website tĩnh công khai.

Xem lệnh chạy cục bộ và lưu ý bảo mật dev server tại [`docs/guide/README.md`](https://github.com/MoLik-Labs/Kova-INC/blob/BETA/docs/guide/README.md). Chưa kiểm thử deploy Pages trên tài khoản thực.
