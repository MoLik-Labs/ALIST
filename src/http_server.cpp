#include "alist/http_server.hpp"
#include "alist/common.hpp"
#include "alist/crypto.hpp"
#include "alist/logger.hpp"
#include "alist/security.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

namespace alist {

std::optional<HttpRange> parse_http_range(std::string_view range_header, int64_t total_size, bool* unsatisfiable) {
    if (unsatisfiable) *unsatisfiable = false;
    range_header = trim_ascii(range_header);
    if (range_header.empty()) return std::nullopt;
    if (!range_header.starts_with("bytes=")) return std::nullopt;
    if (total_size <= 0) {
        if (unsatisfiable) *unsatisfiable = true;
        return std::nullopt;
    }

    std::string_view spec = range_header.substr(6);
    auto comma = spec.find(',');
    if (comma != std::string_view::npos) {
        spec = spec.substr(0, comma); // First range segment
    }
    auto dash = spec.find('-');
    if (dash == std::string_view::npos) {
        if (unsatisfiable) *unsatisfiable = true;
        return std::nullopt;
    }

    std::string_view start_sv = trim_ascii(spec.substr(0, dash));
    std::string_view end_sv   = trim_ascii(spec.substr(dash + 1));

    int64_t start = 0;
    int64_t end = total_size - 1;

    if (start_sv.empty()) {
        // Suffix range: bytes=-N
        auto suffix = parse_i64(end_sv);
        if (!suffix || *suffix <= 0) {
            if (unsatisfiable) *unsatisfiable = true;
            return std::nullopt;
        }
        start = std::max<int64_t>(0, total_size - *suffix);
        end = total_size - 1;
    } else {
        auto s = parse_i64(start_sv);
        if (!s || *s < 0 || *s >= total_size) {
            if (unsatisfiable) *unsatisfiable = true;
            return std::nullopt;
        }
        start = *s;
        if (!end_sv.empty()) {
            auto e = parse_i64(end_sv);
            if (!e || *e < start) {
                if (unsatisfiable) *unsatisfiable = true;
                return std::nullopt;
            }
            end = std::min<int64_t>(total_size - 1, *e);
        }
    }
    return HttpRange{start, end};
}

namespace {

constexpr size_t kMaxHeaderBytes        = 32768;
constexpr size_t kMaxBufferedBodyBytes  = 8 * 1024 * 1024; // 8 MiB for JSON/XML control plane

const char* default_reason_phrase(int code) noexcept {
    switch (code) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 206: return "Partial Content";
        case 207: return "Multi-Status";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 304: return "Not Modified";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 409: return "Conflict";
        case 412: return "Precondition Failed";
        case 413: return "Payload Too Large";
        case 416: return "Range Not Satisfiable";
        case 423: return "Locked";
        case 429: return "Too Many Requests";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 503: return "Service Unavailable";
        default:  return "OK";
    }
}

bool send_all_flags(int fd, const char* data, size_t len, int extra_flags, std::atomic<uint64_t>* bytes_counter = nullptr) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = ::send(fd, data + sent, len - sent, MSG_NOSIGNAL | extra_flags);
        if (n < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (n == 0) return false;
        sent += static_cast<size_t>(n);
    }
    if (bytes_counter && sent > 0) bytes_counter->fetch_add(sent, std::memory_order_relaxed);
    return true;
}

bool send_all(int fd, const char* data, size_t len, std::atomic<uint64_t>* bytes_counter = nullptr) {
    return send_all_flags(fd, data, len, 0, bytes_counter);
}

class SocketReader {
public:
    SocketReader(int fd, std::atomic<uint64_t>* rx_counter = nullptr) : fd_(fd), rx_counter_(rx_counter) {}

    bool read_until(std::string_view delim, std::string& out, size_t max_bytes) {
        out.clear();
        while (true) {
            auto pos = buf_.find(delim, pos_);
            if (pos != std::string::npos) {
                size_t take = (pos + delim.size()) - pos_;
                if (out.size() + take > max_bytes) return false;
                out.append(buf_.data() + pos_, take);
                pos_ += take;
                compact();
                return true;
            }
            size_t avail = buf_.size() - pos_;
            if (avail > delim.size()) {
                size_t safe_take = avail - delim.size();
                if (out.size() + safe_take > max_bytes) return false;
                out.append(buf_.data() + pos_, safe_take);
                pos_ += safe_take;
                compact();
            }
            if (out.size() + (buf_.size() - pos_) > max_bytes) return false;
            if (!fill()) return false;
        }
    }

    ssize_t read_some(char* dst, size_t max_len) {
        if (max_len == 0) return 0;
        if (pos_ < buf_.size()) {
            size_t n = std::min(max_len, buf_.size() - pos_);
            std::memcpy(dst, buf_.data() + pos_, n);
            pos_ += n;
            compact();
            return static_cast<ssize_t>(n);
        }
        while (true) {
            ssize_t n = ::recv(fd_, dst, max_len, 0);
            if (n < 0 && errno == EINTR) continue;
            if (n > 0 && rx_counter_) rx_counter_->fetch_add(static_cast<uint64_t>(n), std::memory_order_relaxed);
            return n;
        }
    }

    bool read_exact(char* dst, size_t exact_len) {
        size_t got = 0;
        while (got < exact_len) {
            ssize_t n = read_some(dst + got, exact_len - got);
            if (n <= 0) return false;
            got += static_cast<size_t>(n);
        }
        return true;
    }

private:
    int fd_;
    std::atomic<uint64_t>* rx_counter_ = nullptr;
    std::string buf_;
    size_t pos_ = 0;

    bool fill() {
        char tmp[16384];
        while (true) {
            ssize_t n = ::recv(fd_, tmp, sizeof(tmp), 0);
            if (n < 0 && errno == EINTR) continue;
            if (n <= 0) return false;
            if (rx_counter_) rx_counter_->fetch_add(static_cast<uint64_t>(n), std::memory_order_relaxed);
            buf_.append(tmp, static_cast<size_t>(n));
            return true;
        }
    }

    void compact() {
        if (pos_ == buf_.size()) {
            buf_.clear();
            pos_ = 0;
        } else if (pos_ > 32768) {
            buf_.erase(0, pos_);
            pos_ = 0;
        }
    }
};

void parse_query_map(std::string_view q, std::map<std::string, std::string>& out) {
    for (auto pair_sv : split_sv(q, '&', true)) {
        auto eq = pair_sv.find('=');
        std::string_view k_raw = (eq == std::string_view::npos) ? pair_sv : pair_sv.substr(0, eq);
        std::string_view v_raw = (eq == std::string_view::npos) ? std::string_view{} : pair_sv.substr(eq + 1);
        auto k = url_decode(k_raw, true);
        auto v = url_decode(v_raw, true);
        if (k && v) out[std::move(*k)] = std::move(*v);
    }
}

} // namespace

HttpServer::HttpServer(std::string bind_addr, int port, int worker_threads, int max_connections)
    : bind_addr_(std::move(bind_addr)), port_(port), max_connections_(max_connections) {
    if (worker_threads <= 0) {
        unsigned int hw = std::thread::hardware_concurrency();
        num_workers_ = static_cast<int>(std::clamp<unsigned int>(hw ? hw * 2 : 8, 8, 32));
    } else {
        num_workers_ = std::clamp(worker_threads, 2, 128);
    }
}

HttpServer::~HttpServer() { stop(); }

bool HttpServer::start() {
    if (running_.load()) return true;

    listen_fd_ = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (listen_fd_ < 0) {
        LOG_ERROR("http", "Failed creating listen socket");
        return false;
    }

    int yes = 1;
    ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
#ifdef SO_REUSEPORT
    ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(yes));
#endif

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port_));
    if (bind_addr_.empty() || bind_addr_ == "0.0.0.0" || bind_addr_ == "::") {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else if (::inet_pton(AF_INET, bind_addr_.c_str(), &addr.sin_addr) != 1) {
        addr.sin_addr.s_addr = INADDR_ANY;
    }

    if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        LOG_ERROR("http", "Failed binding to " + bind_addr_ + ":" + std::to_string(port_) + " (" + std::strerror(errno) + ")");
        ::close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }

    sockaddr_in actual{};
    socklen_t alen = sizeof(actual);
    if (::getsockname(listen_fd_, reinterpret_cast<sockaddr*>(&actual), &alen) == 0) {
        bound_port_ = ntohs(actual.sin_port);
    } else {
        bound_port_ = port_;
    }

    if (::listen(listen_fd_, 4096) != 0) {
        LOG_ERROR("http", "Failed listening on socket");
        ::close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }

    epoll_fd_ = ::epoll_create1(EPOLL_CLOEXEC);
    wakeup_fd_ = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (epoll_fd_ < 0 || wakeup_fd_ < 0) {
        stop();
        return false;
    }

    epoll_event ev1{};
    ev1.events = EPOLLIN;
    ev1.data.fd = listen_fd_;
    ::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, listen_fd_, &ev1);

    epoll_event ev2{};
    ev2.events = EPOLLIN;
    ev2.data.fd = wakeup_fd_;
    ::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, wakeup_fd_, &ev2);

    started_at_unix_.store(unix_now(), std::memory_order_relaxed);
    running_.store(true);
    for (int i = 0; i < num_workers_; ++i) {
        workers_.emplace_back([this] { worker_loop(); });
    }
    accept_thread_ = std::thread([this] { accept_loop(); });
    return true;
}

void HttpServer::stop() {
    if (!running_.exchange(false)) {
        if (listen_fd_ >= 0) { ::close(listen_fd_); listen_fd_ = -1; }
        if (epoll_fd_ >= 0)  { ::close(epoll_fd_);  epoll_fd_ = -1; }
        if (wakeup_fd_ >= 0) { ::close(wakeup_fd_); wakeup_fd_ = -1; }
        return;
    }
    if (wakeup_fd_ >= 0) {
        uint64_t one = 1;
        if (::write(wakeup_fd_, &one, sizeof(one)) < 0) {}
    }
    queue_cv_.notify_all();

    if (accept_thread_.joinable()) accept_thread_.join();
    for (auto& th : workers_) {
        if (th.joinable()) th.join();
    }
    workers_.clear();

    {
        std::lock_guard<std::mutex> lk(queue_mu_);
        while (!conn_queue_.empty()) {
            ::close(conn_queue_.front().fd);
            conn_queue_.pop_front();
        }
    }
    if (listen_fd_ >= 0) { ::close(listen_fd_); listen_fd_ = -1; }
    if (epoll_fd_ >= 0)  { ::close(epoll_fd_);  epoll_fd_ = -1; }
    if (wakeup_fd_ >= 0) { ::close(wakeup_fd_); wakeup_fd_ = -1; }
}

void HttpServer::accept_loop() {
    std::array<epoll_event, 64> events{};
    while (running_.load(std::memory_order_relaxed)) {
        int nfds = ::epoll_wait(epoll_fd_, events.data(), static_cast<int>(events.size()), 1000);
        if (nfds < 0) {
            if (errno == EINTR) continue;
            break;
        }
        for (int i = 0; i < nfds; ++i) {
            if (events[static_cast<size_t>(i)].data.fd == wakeup_fd_) {
                return;
            }
            if (events[static_cast<size_t>(i)].data.fd == listen_fd_) {
                while (running_.load(std::memory_order_relaxed)) {
                    sockaddr_in caddr{};
                    socklen_t clen = sizeof(caddr);
                    int cfd = ::accept4(listen_fd_, reinterpret_cast<sockaddr*>(&caddr), &clen, SOCK_CLOEXEC);
                    if (cfd < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                        if (errno == EINTR) continue;
                        break;
                    }
                    if (active_conns_.load(std::memory_order_relaxed) >= max_connections_) {
                        static constexpr std::string_view kBusy = "HTTP/1.1 503 Service Unavailable\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
                        send_all(cfd, kBusy.data(), kBusy.size());
                        ::close(cfd);
                        continue;
                    }
                    char ipbuf[INET_ADDRSTRLEN]{};
                    ::inet_ntop(AF_INET, &caddr.sin_addr, ipbuf, sizeof(ipbuf));
                    {
                        std::lock_guard<std::mutex> lk(queue_mu_);
                        conn_queue_.push_back(ConnItem{cfd, std::string(ipbuf)});
                    }
                    queue_cv_.notify_one();
                }
            }
        }
    }
}

void HttpServer::worker_loop() {
    while (true) {
        ConnItem item;
        {
            std::unique_lock<std::mutex> lk(queue_mu_);
            queue_cv_.wait(lk, [&] { return !running_.load(std::memory_order_relaxed) || !conn_queue_.empty(); });
            if (!running_.load(std::memory_order_relaxed) && conn_queue_.empty()) return;
            item = std::move(conn_queue_.front());
            conn_queue_.pop_front();
        }
        active_conns_.fetch_add(1, std::memory_order_relaxed);
        handle_connection(item.fd, std::move(item.ip));
        active_conns_.fetch_sub(1, std::memory_order_relaxed);
    }
}

void HttpServer::handle_connection(int client_fd, std::string remote_ip) {
    UniqueFd fd(client_fd);

    int yes = 1;
    ::setsockopt(fd.get(), IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
    timeval tv{15, 0}; // 15s idle/header timeout (Slowloris protection!)
    ::setsockopt(fd.get(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    timeval stv{60, 0};
    ::setsockopt(fd.get(), SOL_SOCKET, SO_SNDTIMEO, &stv, sizeof(stv));

    SocketReader reader(fd.get(), &bytes_received_);
    int req_count = 0;

    while (running_.load(std::memory_order_relaxed) && req_count < 500) {
        ++req_count;
        std::string raw_hdr;
        if (!reader.read_until("\r\n\r\n", raw_hdr, kMaxHeaderBytes)) {
            break;
        }

        std::string_view hdr_sv(raw_hdr);
        auto line_end = hdr_sv.find("\r\n");
        if (line_end == std::string_view::npos) break;
        std::string_view req_line = hdr_sv.substr(0, line_end);

        auto sp1 = req_line.find(' ');
        if (sp1 == std::string_view::npos) break;
        auto sp2 = req_line.find(' ', sp1 + 1);
        if (sp2 == std::string_view::npos) break;

        HttpRequest req;
        req.method       = std::string(req_line.substr(0, sp1));
        req.raw_target   = std::string(req_line.substr(sp1 + 1, sp2 - sp1 - 1));
        req.http_version = std::string(req_line.substr(sp2 + 1));
        req.remote_ip    = remote_ip;

        // Parse headers
        std::string_view rest = hdr_sv.substr(line_end + 2);
        bool has_cl = false;
        bool has_te = false;
        bool malformed_hdr = false;
        int hdr_lines = 0;

        while (!rest.empty() && rest != "\r\n") {
            auto nl = rest.find("\r\n");
            if (nl == std::string_view::npos) break;
            std::string_view hline = rest.substr(0, nl);
            rest.remove_prefix(nl + 2);
            if (hline.empty()) break;
            if (++hdr_lines > 128) { malformed_hdr = true; break; }

            auto colon = hline.find(':');
            if (colon == std::string_view::npos) { malformed_hdr = true; break; }
            std::string key = to_lower_ascii(trim_ascii(hline.substr(0, colon)));
            std::string val = std::string(trim_ascii(hline.substr(colon + 1)));

            if (key == "content-length") {
                if (has_cl) { malformed_hdr = true; break; } // Duplicate Content-Length smuggling defense!
                has_cl = true;
                auto cl_opt = parse_i64(val);
                if (!cl_opt || *cl_opt < 0) { malformed_hdr = true; break; }
                req.content_length = *cl_opt;
            } else if (key == "transfer-encoding") {
                has_te = true;
                if (to_lower_ascii(val).find("chunked") != std::string::npos) {
                    req.is_chunked = true;
                } else {
                    malformed_hdr = true;
                    break;
                }
            }
            req.headers[std::move(key)] = std::move(val);
        }

        // Strict HTTP Request Smuggling Defense: reject simultaneous Content-Length AND Transfer-Encoding!
        if (malformed_hdr || (has_cl && has_te)) {
            static constexpr std::string_view kBad = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
            send_all(fd.get(), kBad.data(), kBad.size());
            break;
        }

        // Security: Only honor X-Forwarded-For when trust_proxy_ is explicitly enabled
        // (prevents untrusted clients from spoofing X-Forwarded-For to bypass IP rate limits!)
        if (trust_proxy_) {
            if (auto xff = req.header("x-forwarded-for"); !xff.empty()) {
                auto comma = xff.find(',');
                auto candidate = trim_ascii(comma == std::string::npos ? std::string_view(xff) : std::string_view(xff).substr(0, comma));
                if (!candidate.empty() && candidate.size() < 64) {
                    req.remote_ip = std::string(candidate);
                }
            }
        }
        total_requests_.fetch_add(1, std::memory_order_relaxed);

        auto qpos = req.raw_target.find('?');
        if (qpos == std::string::npos) {
            req.raw_path = req.raw_target;
        } else {
            req.raw_path = req.raw_target.substr(0, qpos);
            req.query    = req.raw_target.substr(qpos + 1);
            parse_query_map(req.query, req.query_params);
        }
        auto decoded_path = url_decode(req.raw_path, false);
        if (!decoded_path) {
            static constexpr std::string_view kBadUrl = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
            send_all(fd.get(), kBadUrl.data(), kBadUrl.size());
            break;
        }
        req.path = std::move(*decoded_path);

        bool use_streaming_body = stream_pred_ && stream_pred_(req);
        bool body_fully_drained = true;

        if (use_streaming_body) {
            // Extend socket timeout for large uploads
            timeval up_tv{300, 0};
            ::setsockopt(fd.get(), SOL_SOCKET, SO_RCVTIMEO, &up_tv, sizeof(up_tv));

            if (req.is_chunked) {
                size_t chunk_rem = 0;
                bool chunk_eof = false;
                body_fully_drained = false;
                req.stream_body = [&](char* dst, size_t max_len) -> ssize_t {
                    if (chunk_eof) return 0;
                    if (chunk_rem == 0) {
                        std::string sz_line;
                        if (!reader.read_until("\r\n", sz_line, 128)) return -1;
                        std::string_view sv(sz_line);
                        sv.remove_suffix(2);
                        auto semi = sv.find(';');
                        if (semi != std::string_view::npos) sv = sv.substr(0, semi);
                        sv = trim_ascii(sv);
                        if (sv.empty()) return -1;
                        size_t csz = 0;
                        for (char ch : sv) {
                            csz <<= 4;
                            if (ch >= '0' && ch <= '9') csz |= static_cast<size_t>(ch - '0');
                            else if (ch >= 'a' && ch <= 'f') csz |= static_cast<size_t>(ch - 'a' + 10);
                            else if (ch >= 'A' && ch <= 'F') csz |= static_cast<size_t>(ch - 'A' + 10);
                            else return -1;
                        }
                        if (csz == 0) {
                            std::string trl;
                            reader.read_until("\r\n", trl, 4096);
                            chunk_eof = true;
                            body_fully_drained = true;
                            return 0;
                        }
                        chunk_rem = csz;
                    }
                    size_t to_read = std::min(max_len, chunk_rem);
                    ssize_t n = reader.read_some(dst, to_read);
                    if (n <= 0) return -1;
                    chunk_rem -= static_cast<size_t>(n);
                    if (chunk_rem == 0) {
                        char crlf[2];
                        if (!reader.read_exact(crlf, 2) || crlf[0] != '\r' || crlf[1] != '\n') return -1;
                    }
                    return n;
                };
            } else {
                int64_t remaining = req.content_length;
                body_fully_drained = (remaining == 0);
                req.stream_body = [&, remaining](char* dst, size_t max_len) mutable -> ssize_t {
                    if (remaining <= 0) {
                        body_fully_drained = true;
                        return 0;
                    }
                    size_t to_read = static_cast<size_t>(std::min<int64_t>(static_cast<int64_t>(max_len), remaining));
                    ssize_t n = reader.read_some(dst, to_read);
                    if (n <= 0) return -1;
                    remaining -= n;
                    if (remaining == 0) body_fully_drained = true;
                    return n;
                };
            }
        } else {
            if (req.is_chunked) {
                while (true) {
                    std::string sz_line;
                    if (!reader.read_until("\r\n", sz_line, 128)) { body_fully_drained = false; break; }
                    std::string_view sv = trim_ascii(std::string_view(sz_line).substr(0, sz_line.size() - 2));
                    auto semi = sv.find(';');
                    if (semi != std::string_view::npos) sv = sv.substr(0, semi);
                    size_t csz = 0;
                    bool ok_hex = !sv.empty();
                    for (char ch : sv) {
                        csz <<= 4;
                        if (ch >= '0' && ch <= '9') csz |= static_cast<size_t>(ch - '0');
                        else if (ch >= 'a' && ch <= 'f') csz |= static_cast<size_t>(ch - 'a' + 10);
                        else if (ch >= 'A' && ch <= 'F') csz |= static_cast<size_t>(ch - 'A' + 10);
                        else { ok_hex = false; break; }
                    }
                    if (!ok_hex || req.body.size() + csz > kMaxBufferedBodyBytes) { body_fully_drained = false; break; }
                    if (csz == 0) {
                        std::string trl;
                        reader.read_until("\r\n", trl, 4096);
                        break;
                    }
                    size_t old_sz = req.body.size();
                    req.body.resize(old_sz + csz);
                    if (!reader.read_exact(req.body.data() + old_sz, csz)) { body_fully_drained = false; break; }
                    char crlf[2];
                    if (!reader.read_exact(crlf, 2)) { body_fully_drained = false; break; }
                }
                if (!body_fully_drained) {
                    static constexpr std::string_view kLarge = "HTTP/1.1 413 Payload Too Large\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
                    send_all(fd.get(), kLarge.data(), kLarge.size());
                    break;
                }
            } else if (req.content_length > 0) {
                if (static_cast<size_t>(req.content_length) > kMaxBufferedBodyBytes) {
                    static constexpr std::string_view kLarge = "HTTP/1.1 413 Payload Too Large\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
                    send_all(fd.get(), kLarge.data(), kLarge.size());
                    break;
                }
                req.body.resize(static_cast<size_t>(req.content_length));
                if (!reader.read_exact(req.body.data(), req.body.size())) break;
            }
        }

        HttpResponse resp;
        // Baseline security headers (Note: no X-Frame-Options: DENY on UI so live iframe preview works cleanly!)
        resp.headers["X-Content-Type-Options"] = "nosniff";
        resp.headers["Server"] = "AList-nggkhaiz/4.1";

        if (handler_) {
            try {
                handler_(req, resp);
            } catch (const std::exception& ex) {
                LOG_ERROR("http", std::string("Unhandled handler exception: ") + ex.what());
                resp.set_json(500, R"({"code":500,"message":"internal server error","data":null})");
            }
        }

        bool want_close = !body_fully_drained ||
                          iequals_ascii(req.header("connection"), "close") ||
                          (req.http_version == "HTTP/1.0" && !iequals_ascii(req.header("connection"), "keep-alive"));
        resp.headers["Connection"] = want_close ? "close" : "keep-alive";

        bool is_head = (req.method == "HEAD");

        // Case 1: Zero-Copy Kernel sendfile64() Local File Delivery
        if (!resp.sendfile_path.empty()) {
            UniqueFd ffd(::open(resp.sendfile_path.c_str(), O_RDONLY | O_CLOEXEC));
            if (!ffd) {
                resp.sendfile_path.clear();
                resp.set_json(404, R"({"code":404,"message":"file not found","data":null})");
            } else {
                resp.headers["Accept-Ranges"] = "bytes";
                if (!resp.sendfile_etag.empty()) resp.headers["ETag"] = resp.sendfile_etag;
                if (resp.sendfile_mtime > 0) resp.headers["Last-Modified"] = format_http_date(resp.sendfile_mtime);

                // Conditional ETag check (304 Not Modified)
                std::string inm = req.header("if-none-match");
                if (!inm.empty() && !resp.sendfile_etag.empty() && inm == resp.sendfile_etag && req.header("range").empty()) {
                    resp.status = 304;
                    resp.headers["Content-Length"] = "0";
                    std::string wire = "HTTP/1.1 304 Not Modified\r\n";
                    for (const auto& [k, v] : resp.headers) wire += k + ": " + v + "\r\n";
                    wire += "\r\n";
                    if (!send_all(fd.get(), wire.data(), wire.size())) break;
                    if (want_close) break;
                    continue;
                }

                bool unsat = false;
                auto rng = parse_http_range(req.header("range"), resp.sendfile_size, &unsat);
                if (unsat) {
                    resp.status = 416;
                    resp.headers["Content-Range"] = "bytes */" + std::to_string(resp.sendfile_size);
                    resp.headers["Content-Length"] = "0";
                    std::string wire = "HTTP/1.1 416 Range Not Satisfiable\r\n";
                    for (const auto& [k, v] : resp.headers) wire += k + ": " + v + "\r\n";
                    wire += "\r\n";
                    send_all(fd.get(), wire.data(), wire.size());
                    break;
                }

                int64_t offset = 0;
                int64_t send_len = resp.sendfile_size;
                if (rng) {
                    resp.status = 206;
                    offset = rng->start;
                    send_len = rng->end - rng->start + 1;
                    resp.headers["Content-Range"] = "bytes " + std::to_string(rng->start) + "-" +
                                                    std::to_string(rng->end) + "/" + std::to_string(resp.sendfile_size);
                } else {
                    resp.status = 200;
                }
                resp.headers["Content-Length"] = std::to_string(send_len);

                const char* reason = resp.status_text.empty() ? default_reason_phrase(resp.status) : resp.status_text.c_str();
                std::string wire = "HTTP/1.1 " + std::to_string(resp.status) + " " + reason + "\r\n";
                for (const auto& [k, v] : resp.headers) wire += k + ": " + v + "\r\n";
                wire += "\r\n";
                // Coalesce HTTP response headers with the first sendfile64() kernel page via MSG_MORE!
                int hdr_flags = (!is_head && send_len > 0) ? MSG_MORE : 0;
                if (!send_all_flags(fd.get(), wire.data(), wire.size(), hdr_flags, &bytes_sent_)) break;

                if (!is_head && send_len > 0) {
                    off_t sf_off = static_cast<off_t>(offset);
                    int64_t rem = send_len;
                    bool send_ok = true;
                    const int64_t max_slice = (download_limiter_ && download_limiter_->rate() > 0) ? 262144 : 1048576; // 1 MiB zero-copy slices!
                    while (rem > 0) {
                        size_t chunk = static_cast<size_t>(std::min<int64_t>(rem, max_slice));
                        if (download_limiter_) download_limiter_->consume(chunk);
                        ssize_t n = ::sendfile(fd.get(), ffd.get(), &sf_off, chunk);
                        if (n < 0) {
                            if (errno == EINTR) continue;
                            if (errno == EINVAL || errno == ENOSYS) {
                                std::vector<char> fbuf(chunk);
                                ssize_t rn = ::pread(ffd.get(), fbuf.data(), chunk, sf_off);
                                if (rn <= 0 || !send_all(fd.get(), fbuf.data(), static_cast<size_t>(rn), &bytes_sent_)) {
                                    send_ok = false;
                                    break;
                                }
                                sf_off += rn;
                                rem -= rn;
                                continue;
                            }
                            send_ok = false;
                            break;
                        }
                        if (n == 0) break;
                        bytes_sent_.fetch_add(static_cast<uint64_t>(n), std::memory_order_relaxed);
                        rem -= n;
                    }
                    if (!send_ok) break;
                }
                if (want_close) break;
                continue;
            }
        }

        // Case 2: Streaming Callback Response (Proxy / Crypt Chunked AEAD)
        if (resp.stream_writer) {
            bool use_chunked_out = (resp.stream_content_length < 0);
            if (use_chunked_out) {
                resp.headers["Transfer-Encoding"] = "chunked";
            } else {
                resp.headers["Content-Length"] = std::to_string(resp.stream_content_length);
            }
            const char* reason = resp.status_text.empty() ? default_reason_phrase(resp.status) : resp.status_text.c_str();
            std::string wire = "HTTP/1.1 " + std::to_string(resp.status) + " " + reason + "\r\n";
            for (const auto& [k, v] : resp.headers) wire += k + ": " + v + "\r\n";
            wire += "\r\n";
            if (!send_all(fd.get(), wire.data(), wire.size(), &bytes_sent_)) break;

            if (!is_head) {
                bool ok = resp.stream_writer([&](const char* data, size_t len) -> bool {
                    if (len == 0) return true;
                    if (download_limiter_) download_limiter_->consume(len);
                    if (use_chunked_out) {
                        char hexbuf[32];
                        int hn = std::snprintf(hexbuf, sizeof(hexbuf), "%zx\r\n", len);
                        if (!send_all(fd.get(), hexbuf, static_cast<size_t>(hn), &bytes_sent_)) return false;
                        if (!send_all(fd.get(), data, len, &bytes_sent_)) return false;
                        return send_all(fd.get(), "\r\n", 2, &bytes_sent_);
                    }
                    return send_all(fd.get(), data, len, &bytes_sent_);
                });
                if (!ok) break;
                if (use_chunked_out && !send_all(fd.get(), "0\r\n\r\n", 5, &bytes_sent_)) break;
            }
            if (want_close) break;
            continue;
        }

        // Case 3: Standard Buffered Response (with automatic Gzip compression!)
        if (!resp.pre_compressed && resp.body.size() >= 1024 && resp.status != 204 && resp.status != 304) {
            std::string ae = to_lower_ascii(req.header("accept-encoding"));
            std::string ct = to_lower_ascii(resp.headers["Content-Type"]);
            bool compressible = ct.starts_with("application/json") || ct.starts_with("text/") ||
                                ct.starts_with("application/xml") || ct.starts_with("application/javascript") ||
                                ct.find("svg") != std::string::npos;
            if (compressible && ae.find("gzip") != std::string::npos) {
                if (auto gz = gzip_compress(resp.body, 5)) {
                    resp.body = std::move(*gz);
                    resp.headers["Content-Encoding"] = "gzip";
                    resp.headers["Vary"] = "Accept-Encoding";
                }
            }
        }

        if (resp.status == 204 || resp.status == 304) {
            resp.body.clear();
        }
        resp.headers["Content-Length"] = std::to_string(resp.body.size());

        const char* reason = resp.status_text.empty() ? default_reason_phrase(resp.status) : resp.status_text.c_str();
        std::string wire;
        wire.reserve(256 + (is_head ? 0 : resp.body.size()));
        wire += "HTTP/1.1 " + std::to_string(resp.status) + " " + reason + "\r\n";
        for (const auto& [k, v] : resp.headers) {
            wire += k + ": " + v + "\r\n";
        }
        wire += "\r\n";
        if (!is_head && !resp.body.empty()) {
            wire += resp.body;
        }
        if (!send_all(fd.get(), wire.data(), wire.size(), &bytes_sent_)) break;
        if (want_close) break;
    }
}

} // namespace alist
