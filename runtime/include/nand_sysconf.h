#pragma once

// /shared2/sys/SYSCONF, the console settings file the SC library loads at OSInit. A real console always
// has one; without it every SC getter falls back to its built-in default (English, no sensor bar
// setting, ...). The runtime creates one on a new NAND and never replaces an existing file; the only
// change it makes to an existing file is IPL.LNG, when [system] language is set in Config.toml.
//
// Layout (0x4000 bytes): "SCv0", a big-endian u16 item count, one u16 offset per item plus one past the
// last, the items, zero padding, and "SCed" in the last four bytes. An item is a byte
// (type << 5 | (name length - 1)), the name, a size field for arrays (u16 size - 1 for big arrays,
// u8 size - 1 for small ones), then the value.

#include "nand_settings.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace RuntimeNandSysconf {

constexpr size_t kFileSize = 0x4000;

enum class ItemType : uint8_t {
    BigArray = 1,
    SmallArray = 2,
    Byte = 3,
    Short = 4,
    Long = 5,
    LongLong = 6,
    Bool = 7,
};

struct Item {
    std::string name;
    ItemType type;
    std::vector<uint8_t> value;
};

using Region = RuntimeNandSettings::Region;

// IPL.LNG values.
inline std::optional<uint8_t> LanguageCode(std::string_view name) {
    static constexpr std::array<std::string_view, 10> kNames = {
        "japanese", "english", "german", "french", "spanish",
        "italian", "dutch", "simplified_chinese", "traditional_chinese", "korean",
    };
    for (size_t i = 0; i < kNames.size(); ++i) {
        if (name == kNames[i]) {
            return static_cast<uint8_t>(i);
        }
    }
    return std::nullopt;
}

inline uint8_t DefaultLanguage(Region region) {
    switch (region) {
    case Region::Japan: return 0;
    case Region::Korea: return 9;
    default: return 1;
    }
}

// IPL.SADR starts with the country code: Japan 1, United States 49, United Kingdom 110, Korea 136.
inline uint8_t DefaultCountry(Region region) {
    switch (region) {
    case Region::Japan: return 1;
    case Region::Usa: return 49;
    case Region::Korea: return 136;
    default: return 110;
    }
}

// The settings a console has after its first-boot setup, with Dolphin's values where the choice is
// arbitrary. Items whose effect reaches beyond the game's own settings (the RTC counter bias, network
// profiles, parental controls) are left out, so their getters keep the defaults they return today.
inline std::vector<Item> DefaultItems(Region region, uint8_t language, bool widescreen) {
    std::vector<uint8_t> address(0x1008, 0);
    address[0] = DefaultCountry(region);
    return {
        {"BT.DINF", ItemType::BigArray, std::vector<uint8_t>(0x461, 0)}, // no paired remotes
        {"BT.CDIF", ItemType::BigArray, std::vector<uint8_t>(0x205, 0)},
        {"BT.SENS", ItemType::Long, {0, 0, 0, 3}},                      // pointer sensitivity
        {"BT.BAR", ItemType::Byte, {1}},                                // sensor bar above the screen
        {"BT.SPKV", ItemType::Byte, {0x58}},                            // remote speaker volume
        {"BT.MOT", ItemType::Byte, {1}},                                // rumble on
        {"IPL.LNG", ItemType::Byte, {language}},
        {"IPL.SADR", ItemType::BigArray, std::move(address)},
        {"IPL.AR", ItemType::Byte, {static_cast<uint8_t>(widescreen ? 1 : 0)}},
        {"IPL.SSV", ItemType::Byte, {1}},                               // screen burn-in reduction
        {"IPL.CD", ItemType::Bool, {1}},
        {"IPL.CD2", ItemType::Bool, {1}},
        {"IPL.EULA", ItemType::Bool, {1}},
        {"IPL.UPT", ItemType::Byte, {2}},
        {"IPL.PGS", ItemType::Byte, {0}},                               // progressive scan off
        {"IPL.E60", ItemType::Byte, {1}},                               // EuRGB60 on
        {"IPL.DH", ItemType::Byte, {0}},                                // no horizontal offset
        {"IPL.SND", ItemType::Byte, {1}},                               // stereo
    };
}

inline std::optional<std::vector<uint8_t>> Encode(const std::vector<Item>& items) {
    std::vector<uint8_t> file(kFileSize, 0);
    std::memcpy(file.data(), "SCv0", 4);
    const size_t count = items.size();
    file[4] = static_cast<uint8_t>(count >> 8);
    file[5] = static_cast<uint8_t>(count);
    size_t position = 6 + 2 * (count + 1);
    const auto writeOffset = [&](size_t index) {
        file[6 + 2 * index] = static_cast<uint8_t>(position >> 8);
        file[7 + 2 * index] = static_cast<uint8_t>(position);
    };
    for (size_t index = 0; index < count; ++index) {
        const Item& item = items[index];
        const bool big = item.type == ItemType::BigArray;
        const bool small = item.type == ItemType::SmallArray;
        if (item.name.empty() || item.name.size() > 32 || item.value.empty() ||
            (small && item.value.size() > 0x100) || (big && item.value.size() > 0x10000)) {
            return std::nullopt;
        }
        const size_t size = 1 + item.name.size() + (big ? 2 : small ? 1 : 0) + item.value.size();
        if (position + size > kFileSize - 4) {
            return std::nullopt;
        }
        writeOffset(index);
        file[position++] = static_cast<uint8_t>((static_cast<uint8_t>(item.type) << 5) |
                                                (item.name.size() - 1));
        std::memcpy(file.data() + position, item.name.data(), item.name.size());
        position += item.name.size();
        if (big) {
            file[position++] = static_cast<uint8_t>((item.value.size() - 1) >> 8);
            file[position++] = static_cast<uint8_t>(item.value.size() - 1);
        } else if (small) {
            file[position++] = static_cast<uint8_t>(item.value.size() - 1);
        }
        std::memcpy(file.data() + position, item.value.data(), item.value.size());
        position += item.value.size();
    }
    writeOffset(count);
    std::memcpy(file.data() + kFileSize - 4, "SCed", 4);
    return file;
}

// Offset of a Byte item's value in an encoded file, or nothing when the file is not a SYSCONF or does
// not have that item as a Byte.
inline std::optional<size_t> FindByteItem(const std::vector<uint8_t>& file, std::string_view name) {
    if (file.size() != kFileSize || std::memcmp(file.data(), "SCv0", 4) != 0 ||
        std::memcmp(file.data() + kFileSize - 4, "SCed", 4) != 0) {
        return std::nullopt;
    }
    const size_t count = (static_cast<size_t>(file[4]) << 8) | file[5];
    if (6 + 2 * count > kFileSize) {
        return std::nullopt;
    }
    for (size_t index = 0; index < count; ++index) {
        const size_t offset = (static_cast<size_t>(file[6 + 2 * index]) << 8) | file[7 + 2 * index];
        if (offset >= kFileSize - 4) {
            return std::nullopt;
        }
        const uint8_t header = file[offset];
        const size_t nameLength = (header & 0x1F) + 1;
        if (offset + 1 + nameLength + 1 > kFileSize - 4) {
            return std::nullopt;
        }
        if (static_cast<ItemType>(header >> 5) == ItemType::Byte && nameLength == name.size() &&
            std::memcmp(file.data() + offset + 1, name.data(), nameLength) == 0) {
            return offset + 1 + nameLength;
        }
    }
    return std::nullopt;
}

inline std::filesystem::path FilePath(const std::filesystem::path& root) {
    return root / "shared2" / "sys" / "SYSCONF";
}

inline std::optional<std::vector<uint8_t>> Read(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    std::vector<uint8_t> bytes(kFileSize);
    if (!input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) {
        return std::nullopt;
    }
    return bytes;
}

inline bool Write(const std::filesystem::path& path, const std::vector<uint8_t>& bytes) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
        return false;
    }
    const std::filesystem::path temporary = path.parent_path() / "SYSCONF.runtime-tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!output) {
            std::filesystem::remove(temporary, ec);
            return false;
        }
    }
    std::filesystem::rename(temporary, path, ec);
    if (ec) {
        std::filesystem::remove(temporary, ec);
        return false;
    }
    return true;
}

// Creates SYSCONF when it is missing; applies `language` (when given) to an existing one. A damaged
// or foreign file is left alone. Returns false only when a file had to be written and could not be.
inline bool Ensure(const std::filesystem::path& root, Region region, std::optional<uint8_t> language,
                   bool widescreen, std::string& error) {
    const std::filesystem::path path = FilePath(root);
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        const auto bytes = Encode(DefaultItems(region, language.value_or(DefaultLanguage(region)), widescreen));
        if (!bytes || !Write(path, *bytes)) {
            error = "Cannot create NAND SYSCONF";
            return false;
        }
        return true;
    }
    if (!language) {
        return true;
    }
    auto bytes = Read(path);
    if (!bytes) {
        return true;
    }
    const auto offset = FindByteItem(*bytes, "IPL.LNG");
    if (!offset || (*bytes)[*offset] == *language) {
        return true;
    }
    (*bytes)[*offset] = *language;
    if (!Write(path, *bytes)) {
        error = "Cannot update the language in NAND SYSCONF";
        return false;
    }
    return true;
}

} // namespace RuntimeNandSysconf
