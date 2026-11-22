---
layout: home
hero:
  name: Kova
  text: Tệp của bạn, không gian của bạn.
  tagline: Hướng dẫn thực tế để triển khai và vận hành kho tệp riêng một cách an toàn.
  actions:
    - theme: brand
      text: Bắt đầu
      link: /vi/start
    - theme: alt
      text: Hướng dẫn Cloudflare
      link: /vi/cloudflare
features:
  - title: Chọn đúng nền tảng
    details: Hiểu nơi chạy server và nơi lưu dữ liệu trước khi triển khai.
  - title: Giữ dữ liệu an toàn
    details: Thiết lập lưu trữ bền vững, bảo vệ truy cập và kiểm tra sao lưu.
  - title: Làm theo từng bước
    details: Kết nối fork, kiểm tra triển khai và xử lý những lỗi thường gặp.
---

::: warning Tình trạng hiện tại
Server Kova đầy đủ hiện chạy trong container Linux native. Worker và Pages của Cloudflare mới là **proxy**, chưa phải backend độc lập. Đã có bộ đọc R2 Cloudflare-native thử nghiệm và tùy chọn tải tệp nhỏ, nhưng chưa phải backend hoàn chỉnh hoặc sẵn sàng production; bản Go cũng đang thử nghiệm.
:::
