#include "core/rost.h"

#include "core/checksum.h"

namespace loftools {

void RostRecord::recompute() {
    actual_checksum = lof_checksum(character.raw().data(), kCharSize);
}

bool RostFile::all_intact() const {
    for (const auto& r : records) {
        if (r.status != RecordStatus::Ok) return false;
    }
    return true;
}

size_t RostFile::damaged_count() const {
    size_t n = 0;
    for (const auto& r : records) {
        if (r.status != RecordStatus::Ok) ++n;
    }
    return n;
}

RostFile parse_rost(const std::vector<uint8_t>& file) {
    RostFile out;
    if (file.empty()) {
        out.error = "file is empty";
        return out;
    }
    out.declared_count = file[0];
    const size_t body = file.size() - 1;
    const size_t avail = body / kRostRecordSize;
    out.count_matches = (avail == out.declared_count);
    if (!out.count_matches) {
        out.error = "declared record count (" + std::to_string(out.declared_count) +
                    ") does not match file size (" + std::to_string(avail) +
                    " whole records)";
    }

    const size_t n = out.declared_count;
    out.records.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        const size_t off = 1 + i * kRostRecordSize;
        if (off + kRostRecordSize > file.size()) {
            out.ok = false;
            out.error = "file is truncated at record " + std::to_string(i);
            return out;
        }
        RostRecord rec;
        std::array<uint8_t, kCharSize> raw{};
        for (size_t b = 0; b < kCharSize; ++b) raw[b] = file[off + b];
        rec.character = Character(raw);
        rec.stored_checksum = read_u32le(file.data() + off + kCharSize);
        rec.recompute();

        if (rec.actual_checksum != rec.stored_checksum) {
            rec.status = RecordStatus::BadChecksum;
        } else if (rec.character.tampered()) {
            rec.status = RecordStatus::Tampered;
        } else {
            rec.status = RecordStatus::Ok;
        }
        out.records.push_back(std::move(rec));
    }
    out.ok = out.count_matches;
    return out;
}

std::vector<uint8_t> serialize_rost(const RostFile& rost) {
    std::vector<uint8_t> out;
    out.reserve(1 + rost.records.size() * kRostRecordSize);
    out.push_back((uint8_t)rost.records.size());
    for (const auto& r : rost.records) {
        const auto& raw = r.character.raw();
        out.insert(out.end(), raw.begin(), raw.end());
        uint8_t tail[4];
        write_u32le(tail, r.stored_checksum);
        out.insert(out.end(), tail, tail + 4);
    }
    return out;
}

}  // namespace loftools
