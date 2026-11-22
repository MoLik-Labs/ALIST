#include "alist/archive.hpp"
#include "alist/common.hpp"
#include "alist/crypto.hpp"
#include "alist/security.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <limits>
#include <unordered_set>
#include <sys/stat.h>
#include <unistd.h>

namespace alist {

namespace {
constexpr uint32_t kZipEocdSig  = 0x06054b50u;
constexpr uint32_t kZipCdSig    = 0x02014b50u;
constexpr uint32_t kZipLocalSig = 0x04034b50u;
constexpr size_t   kMaxZipEntries = 65535;
constexpr uint64_t kMaxCompressionRatio = 200; // Zip-Bomb defense

uint16_t read_u16_le(const uint8_t* p) noexcept {
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}
uint32_t read_u32_le(const uint8_t* p) noexcept {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}
bool pread_exact(int fd, void* buf, size_t len, uint64_t off) {
    if (off > static_cast<uint64_t>(std::numeric_limits<off_t>::max()) ||
        len > static_cast<uint64_t>(std::numeric_limits<off_t>::max()) - off) {
        return false;
    }
    uint8_t* p = static_cast<uint8_t*>(buf);
    size_t got = 0;
    while (got < len) {
        ssize_t n = ::pread(fd, p + got, len - got, static_cast<off_t>(off + got));
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return false;
        got += static_cast<size_t>(n);
    }
    return true;
}

bool write_exact(int fd, const char* data, size_t len) {
    size_t written = 0;
    while (written < len) {
        ssize_t n = ::write(fd, data + written, len - written);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return false;
        written += static_cast<size_t>(n);
    }
    return true;
}

bool zip_list_from_fd(int fd, std::vector<ZipEntryInfo>& out, std::string& err_out) {
    struct stat st{};
    if (::fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 22) {
        err_out = "Not a valid ZIP archive (too small)";
        return false;
    }
    uint64_t file_size = static_cast<uint64_t>(st.st_size);
    size_t tail_len = static_cast<size_t>(std::min<uint64_t>(file_size, 65557));
    uint64_t tail_off = file_size - tail_len;
    std::vector<uint8_t> tail(tail_len);
    if (!pread_exact(fd, tail.data(), tail_len, tail_off)) {
        err_out = "Failed to read archive EOCD";
        return false;
    }

    ssize_t eocd_pos = -1;
    for (ssize_t i = static_cast<ssize_t>(tail_len) - 22; i >= 0; --i) {
        const size_t offset = static_cast<size_t>(i);
        if (read_u32_le(&tail[offset]) != kZipEocdSig) continue;
        const uint16_t comment_len = read_u16_le(&tail[offset + 20]);
        if (offset + 22 + comment_len == tail_len) {
            eocd_pos = i;
            break;
        }
    }
    if (eocd_pos < 0) {
        err_out = "ZIP End-of-Central-Directory record not found or has an invalid comment length";
        return false;
    }

    const uint8_t* eocd = &tail[static_cast<size_t>(eocd_pos)];
    const uint16_t disk_number = read_u16_le(eocd + 4);
    const uint16_t cd_disk_number = read_u16_le(eocd + 6);
    const uint16_t disk_entries = read_u16_le(eocd + 8);
    const uint16_t total_entries = read_u16_le(eocd + 10);
    const uint32_t cd_size = read_u32_le(eocd + 12);
    const uint32_t cd_offset = read_u32_le(eocd + 16);
    const uint64_t eocd_absolute = tail_off + static_cast<uint64_t>(eocd_pos);
    const uint64_t cd_end = static_cast<uint64_t>(cd_offset) + static_cast<uint64_t>(cd_size);

    if (disk_number != 0 || cd_disk_number != 0 || disk_entries != total_entries) {
        err_out = "Multi-disk ZIP archives are not supported";
        return false;
    }
    if (total_entries == std::numeric_limits<uint16_t>::max() ||
        cd_size == std::numeric_limits<uint32_t>::max() ||
        cd_offset == std::numeric_limits<uint32_t>::max() ||
        total_entries > kMaxZipEntries || cd_end > eocd_absolute || cd_size > 64 * 1024 * 1024) {
        err_out = "Corrupted, ZIP64, or oversized ZIP Central Directory";
        return false;
    }

    std::vector<uint8_t> cd(cd_size);
    if (cd_size > 0 && !pread_exact(fd, cd.data(), cd_size, cd_offset)) {
        err_out = "Failed to read ZIP Central Directory";
        return false;
    }

    out.clear();
    out.reserve(total_entries);
    std::unordered_set<std::string> seen_paths;
    seen_paths.reserve(total_entries);
    size_t pos = 0;
    for (size_t idx = 0; idx < total_entries; ++idx) {
        if (cd.size() - pos < 46) {
            err_out = "Truncated ZIP Central Directory";
            return false;
        }
        if (read_u32_le(&cd[pos]) != kZipCdSig) {
            err_out = "Invalid Central Directory entry header";
            return false;
        }
        uint16_t flags     = read_u16_le(&cd[pos + 8]);
        uint16_t method    = read_u16_le(&cd[pos + 10]);
        uint32_t crc       = read_u32_le(&cd[pos + 16]);
        uint32_t comp_sz   = read_u32_le(&cd[pos + 20]);
        uint32_t uncomp_sz = read_u32_le(&cd[pos + 24]);
        uint16_t name_len  = read_u16_le(&cd[pos + 28]);
        uint16_t extra_len = read_u16_le(&cd[pos + 30]);
        uint16_t comm_len  = read_u16_le(&cd[pos + 32]);
        uint32_t local_off = read_u32_le(&cd[pos + 42]);

        size_t rec_len = static_cast<size_t>(46) + name_len + extra_len + comm_len;
        if (rec_len > cd.size() - pos) {
            err_out = "Truncated Central Directory entry";
            return false;
        }
        if ((flags & (0x0001 | 0x0040)) != 0) {
            err_out = "Encrypted ZIP entries are not supported";
            return false;
        }
        if (comp_sz == std::numeric_limits<uint32_t>::max() ||
            uncomp_sz == std::numeric_limits<uint32_t>::max() ||
            local_off == std::numeric_limits<uint32_t>::max()) {
            err_out = "ZIP64 entry metadata is not supported";
            return false;
        }
        std::string raw_name(reinterpret_cast<const char*>(&cd[pos + 46]), name_len);
        pos += rec_len;

        bool is_dir = (!raw_name.empty() && (raw_name.back() == '/' || raw_name.back() == '\\'));
        while (!raw_name.empty() && (raw_name.back() == '/' || raw_name.back() == '\\')) raw_name.pop_back();
        if (raw_name.empty()) continue;

        auto safe_rel = secure_archive_join("/virtual_root", raw_name);
        if (!safe_rel) {
            err_out = "Blocked unsafe archive entry path (Zip-Slip attempt): " + raw_name;
            return false;
        }
        std::string rel_path = safe_rel->substr(std::string_view("/virtual_root/").size());
        if (!seen_paths.insert(rel_path).second) {
            err_out = "Duplicate normalized path in ZIP archive: " + rel_path;
            return false;
        }

        ZipEntryInfo info;
        info.path = std::move(rel_path);
        info.is_dir = is_dir;
        info.compressed_size = comp_sz;
        info.uncompressed_size = uncomp_sz;
        info.method = method;
        info.crc32 = crc;
        info.local_header_offset = local_off;
        info.modified = static_cast<int64_t>(st.st_mtime);
        out.push_back(std::move(info));
    }
    if (pos != cd.size()) {
        // The optional central-directory digital signature is the only accepted trailer.
        if (cd.size() - pos < 6 || read_u32_le(&cd[pos]) != 0x05054b50u ||
            read_u16_le(&cd[pos + 4]) != cd.size() - pos - 6) {
            err_out = "Unexpected trailing data in ZIP Central Directory";
            return false;
        }
    }
    return true;
}

std::optional<std::string> extract_entry_from_fd(int fd,
                                                 const ZipEntryInfo& target,
                                                 std::string& err_out,
                                                 size_t max_entry_bytes) {
    if (target.uncompressed_size > max_entry_bytes || target.compressed_size > max_entry_bytes + 65536) {
        err_out = "Archive entry exceeds maximum allowed extraction size";
        return std::nullopt;
    }
    if (target.uncompressed_size > 1024 * 1024 &&
        (target.compressed_size == 0 ||
         target.uncompressed_size > target.compressed_size * kMaxCompressionRatio)) {
        err_out = "Blocked suspicious archive compression ratio (Zip-Bomb protection)";
        return std::nullopt;
    }

    uint8_t lhdr[30]{};
    if (!pread_exact(fd, lhdr, sizeof(lhdr), target.local_header_offset) ||
        read_u32_le(lhdr) != kZipLocalSig) {
        err_out = "Invalid local ZIP file header";
        return std::nullopt;
    }
    uint16_t l_name = read_u16_le(lhdr + 26);
    uint16_t l_extra = read_u16_le(lhdr + 28);
    uint64_t data_off = target.local_header_offset + uint64_t{30} + l_name + l_extra;

    std::string comp_buf(static_cast<size_t>(target.compressed_size), '\0');
    if (target.compressed_size > 0 &&
        !pread_exact(fd, comp_buf.data(), comp_buf.size(), data_off)) {
        err_out = "Failed to read compressed archive data";
        return std::nullopt;
    }

    std::string plain;
    if (target.method == 0) {
        if (comp_buf.size() != target.uncompressed_size) {
            err_out = "Size mismatch in stored ZIP entry";
            return std::nullopt;
        }
        plain = std::move(comp_buf);
    } else if (target.method == 8) {
        auto inflated = raw_inflate(comp_buf, max_entry_bytes);
        if (!inflated || inflated->size() != target.uncompressed_size) {
            err_out = "Deflate decompression failed or size mismatch";
            return std::nullopt;
        }
        plain = std::move(*inflated);
    } else {
        err_out = "Unsupported ZIP compression method: " + std::to_string(target.method);
        return std::nullopt;
    }

    if (crc32_compute(plain) != target.crc32) {
        err_out = "Archive CRC-32 integrity check failed";
        return std::nullopt;
    }
    return plain;
}
} // namespace

bool zip_list_entries(const std::string& local_zip_path, std::vector<ZipEntryInfo>& out, std::string& err_out) {
    UniqueFd fd(::open(local_zip_path.c_str(), O_RDONLY | O_CLOEXEC));
    if (!fd) { err_out = "Unable to open archive"; return false; }
    return zip_list_from_fd(fd.get(), out, err_out);
}

std::optional<std::string> zip_extract_entry(const std::string& local_zip_path,
                                             std::string_view inner_path,
                                             std::string& err_out,
                                             size_t max_entry_bytes) {
    while (!inner_path.empty() && inner_path.front() == '/') inner_path.remove_prefix(1);
    UniqueFd fd(::open(local_zip_path.c_str(), O_RDONLY | O_CLOEXEC));
    if (!fd) { err_out = "Failed to open archive"; return std::nullopt; }

    std::vector<ZipEntryInfo> entries;
    if (!zip_list_from_fd(fd.get(), entries, err_out)) return std::nullopt;

    for (const auto& e : entries) {
        if (e.path == inner_path && !e.is_dir) {
            return extract_entry_from_fd(fd.get(), e, err_out, max_entry_bytes);
        }
    }
    err_out = "Archive entry not found";
    return std::nullopt;
}

bool zip_decompress_to_dir(const std::string& local_zip_path,
                           const std::string& dst_local_dir,
                           std::string& err_out,
                           size_t max_total_bytes) {
    // Single FD open + single Central Directory parse: O(N) decompression across multi-file archives
    UniqueFd fd(::open(local_zip_path.c_str(), O_RDONLY | O_CLOEXEC));
    if (!fd) { err_out = "Unable to open archive"; return false; }

    std::vector<ZipEntryInfo> entries;
    if (!zip_list_from_fd(fd.get(), entries, err_out)) return false;

    size_t total_written = 0;
    std::error_code ec;
    std::filesystem::create_directories(dst_local_dir, ec);
    if (ec) { err_out = ec.message(); return false; }

    for (const auto& e : entries) {
        auto safe_dst = secure_archive_join(dst_local_dir, e.path);
        if (!safe_dst) {
            err_out = "Unsafe archive entry blocked: " + e.path;
            return false;
        }
        if (e.is_dir) {
            std::filesystem::create_directories(*safe_dst, ec);
            continue;
        }
        if (e.uncompressed_size > max_total_bytes - total_written) {
            err_out = "Archive total extracted size exceeds security limit";
            return false;
        }
        size_t remaining_budget = max_total_bytes - total_written;
        auto data = extract_entry_from_fd(fd.get(), e, err_out,
                                          std::min<size_t>(remaining_budget, 256 * 1024 * 1024));
        if (!data) return false;

        std::filesystem::create_directories(std::filesystem::path(*safe_dst).parent_path(), ec);
        int flags = O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC;
#ifdef O_NOFOLLOW
        flags |= O_NOFOLLOW;
#endif
        UniqueFd wfd(::open(safe_dst->c_str(), flags, 0644));
        if (!wfd || !write_exact(wfd.get(), data->data(), data->size())) {
            err_out = "Failed writing extracted file: " + e.path;
            return false;
        }
        total_written += data->size();
    }
    return true;
}

} // namespace alist
