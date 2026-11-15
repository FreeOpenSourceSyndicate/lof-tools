#ifndef LOFTOOLS_CORE_FILE_IO_H
#define LOFTOOLS_CORE_FILE_IO_H

#include <cstdint>
#include <string>
#include <vector>

namespace loftools {

// Read a whole file into memory.  Returns false (and leaves `out` untouched) if
// the file cannot be opened or read.
bool read_file(const std::string& path, std::vector<uint8_t>& out);

// Write `data` to `path`, replacing any existing file.  Returns false on
// failure.
bool write_file(const std::string& path, const std::vector<uint8_t>& data);

}  // namespace loftools

#endif  // LOFTOOLS_CORE_FILE_IO_H
