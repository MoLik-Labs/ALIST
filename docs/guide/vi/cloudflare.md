# Cloudflare: mỗi thành phần chạy ở đâu?

Cloudflare Workers hỗ trợ JavaScript, TypeScript, Python và Rust; WebAssembly còn hỗ trợ ngôn ngữ khác. Không phải chỉ có TypeScript. Server Linux native thông thường không thể đưa nguyên xi lên Worker. Muốn Kova chạy độc lập cần viết backend riêng cho runtime Cloudflare, kết hợp dịch vụ bền vững như D1 và R2. **Repo mới có bộ đọc R2 thử nghiệm, tải tệp nhỏ chỉ tạo mới khi bật tùy chọn và connector Google Drive chỉ đọc cho một tài khoản; backend hoàn chỉnh chưa được triển khai. Connector Google Drive mới kiểm thử bằng giả lập phản hồi, chưa tích hợp giao diện.**

Worker và Pages hiện tại chỉ chuyển tiếp request tới server Kova đặt ở nơi khác. Chúng không tự tạo database, tài khoản, driver tệp hay server gốc.

## Proxy Worker

1. Triển khai server Kova trước trên host có ổ đĩa bền vững. Kiểm tra URL HTTPS, `/ping` và đăng nhập.
2. Dùng nút Worker trong README, hoặc kết nối fork với Workers Builds. Với fork, chọn đúng nhánh và root directory `deploy/cloudflare-worker`.
3. `KOVA_ORIGIN` là URL HTTPS của **server Kova bạn đã triển khai riêng**, không phải mã do Cloudflare cấp. Ví dụ sau khi deploy Render, lấy URL service trên dashboard Render như `https://example.onrender.com`, kiểm tra `/ping` và đăng nhập; dùng phần origin không có đường dẫn, query, tên người dùng hoặc mật khẩu. Đặt giá trị đó làm **runtime secret** `KOVA_ORIGIN`. Biến chỉ dùng lúc build không thay thế runtime secret. Nếu chưa có server riêng, bạn chưa có giá trị này; bản backend Cloudflare độc lập vẫn chưa hoàn chỉnh.
4. Deploy, mở URL Worker và thử đăng nhập, xem danh sách, tải lên/tải xuống và khởi động lại. 503 có thể do origin thiếu hoặc sai; 502 là lỗi kết nối phía server. Xem log Cloudflare nhưng không chia sẻ secret.

## Proxy Pages

Tạo dự án Pages kết nối repo và nhánh của bạn. Đặt root directory là `deploy/cloudflare-pages`, build command là `exit 0`, output directory là `public`. Đặt runtime secret `KOVA_ORIGIN` như trên rồi deploy lại. Xem [hướng dẫn Pages](https://github.com/MoLik-Labs/Kova-INC/tree/BETA/deploy/cloudflare-pages). Cần kiểm tra kết nối Git và deploy trên tài khoản của bạn.

## Trang mất giao diện hoặc nút không hoạt động?

Điều này có thể xảy ra khi deploy `web/index.html` dạng tĩnh mà không tải được CSS/JS tương ứng, hoặc script bị chặn. Mã nguồn mới tham chiếu asset nội bộ, nhưng **giao diện tĩnh không phải server**: API sẽ lỗi và giao diện báo không kết nối được server. Hãy dùng một trong hai proxy trên cùng origin thay vì chỉ xuất bản `web` như ứng dụng.

::: warning Ranh giới bảo mật
Origin vẫn có thể bị truy cập trực tiếp nếu bạn không chặn. Giữ `ALIST_TRUST_PROXY` tắt nếu client không tin cậy có thể bỏ qua proxy. Không cache dữ liệu riêng tư, và kiểm tra giới hạn truyền tải của Cloudflare. Dùng tài nguyên và secret riêng cho preview; không kết nối preview không tin cậy vào dữ liệu production.
:::

## Workers Builds báo thiếu `previews`?

Nếu log ghi `Executing user deploy command: npx wrangler preview` và `missing a previews block`, đây là **lỗi build preview của Worker**, không phải lỗi Cloudflare Pages. Nếu bạn chưa cần preview, vào Worker → **Settings → Build → Branch control** và tắt **Enable Preview Builds**; theo [hướng dẫn Cloudflare](https://developers.cloudflare.com/workers/ci-cd/builds/build-branches/), thao tác này không đổi nhánh production. Giao diện có thể khác theo tài khoản. Không thay preview command bằng `wrangler deploy`: điều đó có thể ghi đè bản production.

Nếu cần preview, trước hết xác định đúng Worker và root directory trong cấu hình build. Cấu hình `previews` chỉ nên được thêm cho dự án thực sự định triển khai, sau khi tách biệt tài nguyên và secret preview khỏi production. Không cấp dữ liệu thật cho preview không tin cậy. `deploy/cloudflare-worker` hiện là **proxy**, còn `deploy/cloudflare-native` vẫn chỉ là prototype; thêm `previews` không làm backend độc lập hoàn chỉnh. Xem log mới sau khi thay đổi để xác nhận, vì sửa lỗi đầu tiên không đảm bảo deploy thành công.
