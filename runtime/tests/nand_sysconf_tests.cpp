#include "nand_sysconf.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>

static void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

static uint16_t Read16(const std::vector<uint8_t>& bytes, size_t offset) {
    return static_cast<uint16_t>((bytes[offset] << 8) | bytes[offset + 1]);
}

int main() {
    const auto root = std::filesystem::temp_directory_path() /
        ("wiicomp-nand-sysconf-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        using namespace RuntimeNandSysconf;
        Require(RuntimeNandSettings::RegionFromGameCode("SMNE") == Region::Usa, "E is the USA");
        Require(RuntimeNandSettings::RegionFromGameCode("RMCP") == Region::Europe, "P is Europe");
        Require(RuntimeNandSettings::RegionFromGameCode("SMNJ") == Region::Japan, "J is Japan");
        Require(RuntimeNandSettings::RegionFromGameCode("") == Region::Europe, "No code keeps PAL");
        Require(LanguageCode("french") == 3 && LanguageCode("english") == 1 &&
                LanguageCode("japanese") == 0 && !LanguageCode("klingon"), "IPL.LNG values");

        // Hand-checked encoding of two items: header, offsets (one past the end), items, footer.
        const auto small = Encode({{"IPL.LNG", ItemType::Byte, {3}},
                                   {"IPL.IDL", ItemType::SmallArray, {0, 1}}});
        Require(small && small->size() == kFileSize, "Fixed file size");
        const std::vector<uint8_t> expected = {
            'S', 'C', 'v', '0', 0x00, 0x02, 0x00, 0x0C, 0x00, 0x15, 0x00, 0x20,
            0x66, 'I', 'P', 'L', '.', 'L', 'N', 'G', 0x03,
            0x46, 'I', 'P', 'L', '.', 'I', 'D', 'L', 0x01, 0x00, 0x01,
        };
        Require(std::equal(expected.begin(), expected.end(), small->begin()), "Item layout");
        Require(std::memcmp(small->data() + kFileSize - 4, "SCed", 4) == 0, "Footer");
        Require(FindByteItem(*small, "IPL.LNG") == size_t{0x14}, "Find a byte item");
        Require(!FindByteItem(*small, "IPL.IDL"), "A small array is not a byte item");

        const auto defaults = Encode(DefaultItems(Region::Usa, 1, true));
        Require(defaults.has_value(), "Defaults fit in the file");
        const size_t count = Read16(*defaults, 4);
        Require(count == DefaultItems(Region::Usa, 1, true).size(), "Item count");
        for (size_t index = 0; index < count; ++index) {
            Require(Read16(*defaults, 6 + 2 * index) < Read16(*defaults, 8 + 2 * index), "Offsets increase");
        }
        const auto lng = FindByteItem(*defaults, "IPL.LNG");
        const auto ar = FindByteItem(*defaults, "IPL.AR");
        Require(lng && (*defaults)[*lng] == 1 && ar && (*defaults)[*ar] == 1, "Default values");

        std::string error;
        const auto fresh = root / "fresh";
        Require(Ensure(fresh, Region::Usa, std::nullopt, false, error), "Create a missing SYSCONF");
        const auto created = Read(FilePath(fresh));
        Require(created && (*created)[*FindByteItem(*created, "IPL.LNG")] == 1, "USA default is English");
        Require(Ensure(fresh, Region::Usa, uint8_t{3}, false, error), "Apply a configured language");
        const auto french = Read(FilePath(fresh));
        Require(french && (*french)[*FindByteItem(*french, "IPL.LNG")] == 3, "Language updated");
        auto patched = *created;
        patched[*FindByteItem(patched, "IPL.LNG")] = 3;
        Require(*french == patched, "Only the language byte changes");
        Require(Ensure(fresh, Region::Usa, std::nullopt, true, error), "Second boot");
        Require(Read(FilePath(fresh)) == patched, "An existing file is never regenerated");

        const auto foreign = root / "foreign";
        std::filesystem::create_directories(FilePath(foreign).parent_path());
        { std::ofstream output(FilePath(foreign), std::ios::binary); output << "not a SYSCONF"; }
        Require(Ensure(foreign, Region::Usa, uint8_t{3}, false, error), "A damaged file is not an error");
        std::ifstream input(FilePath(foreign), std::ios::binary);
        const std::string kept{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
        Require(kept == "not a SYSCONF", "A damaged file is left alone");
        input.close();

        std::filesystem::remove_all(root);
        std::cout << "NAND SYSCONF checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::filesystem::remove_all(root);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
