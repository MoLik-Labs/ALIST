#pragma once

#include "alist/models.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace alist {

struct ZipEntryInfo {
    std::string path; // Normalized relative path inside archive
    bool is_dir = false;
    uint64_t compressed_size = 0;
    uint64_t uncompressed_size = 0;
    uint16_t method = 0; // 0 = Store, 8 = Deflate
    uint32_t crc32 = 0;
    uint64_t local_header_offset = 0;
    int64_t modified = 0;
};

// Inspect ZIP central directory safely (rejects Zip-Slip & malformed headers)
bool zip_list_entries(const std::string& local_zip_path, std::vector<ZipEntryInfo>& out, std::string& err_out);

// Extract single entry in memory (guarded by max_entry_bytes & max_ratio Zip-Bomb protection)
std::optional<std::string> zip_extract_entry(const std::string& local_zip_path,
                                             std::string_view inner_path,
                                             std::string& err_out,
                                             size_t max_entry_bytes = 128 * 1024 * 1024);

// Decompress entire ZIP (or inner subpath) safely into target local directory using secure_archive_join
bool zip_decompress_to_dir(const std::string& local_zip_path,
                           const std::string& dst_local_dir,
                           std::string& err_out,
                           size_t max_total_bytes = 2ULL * 1024 * 1024 * 1024);

} // namespace alist
