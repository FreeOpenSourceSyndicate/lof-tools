#include "core/file_io.h"

#include <cstdio>

namespace loftools {

bool read_file(const std::string& path, std::vector<uint8_t>& out) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::vector<uint8_t> buf;
    uint8_t chunk[65536];
    size_t n;
    while ((n = std::fread(chunk, 1, sizeof(chunk), f)) > 0) {
        buf.insert(buf.end(), chunk, chunk + n);
    }
    bool ok = std::ferror(f) == 0;
    std::fclose(f);
    if (ok) out = std::move(buf);
    return ok;
}

bool write_file(const std::string& path, const std::vector<uint8_t>& data) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    size_t written = data.empty() ? 0 : std::fwrite(data.data(), 1, data.size(), f);
    bool ok = written == data.size() && std::ferror(f) == 0;
    ok = std::fclose(f) == 0 && ok;
    return ok;
}

}  // namespace loftools
