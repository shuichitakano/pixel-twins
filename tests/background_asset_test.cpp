#include "pixel_twins/background_asset.hpp"
#include "test_check.hpp"

#include <array>
#include <cstdint>

namespace {

constexpr std::array<std::uint8_t, 164> makePack() {
    std::array<std::uint8_t, 164> result{};
    result[0] = 'P'; result[1] = 'T'; result[2] = 'B'; result[3] = 'G';
    result[4] = 1; result[6] = 32;
    result[8] = 8; result[9] = 8; result[10] = 2;
    result[11] = 1; result[12] = 2; result[13] = 1; result[14] = 1; result[15] = 1;
    result[16] = 32;
    result[20] = 36;
    result[24] = 128;
    result[32] = 0; result[33] = 1; result[34] = 0; result[35] = 1;
    for (std::size_t index = 0; index < 64; ++index) {
        result[36 + index] = 3;
        result[100 + index] = 4;
    }
    return result;
}

constexpr auto kPack = makePack();

} // namespace

int main() {
    using namespace pixel_twins;
    BackgroundAssetPackView view{kPack.data(), kPack.size()};
    check(view.valid());
    check(view.tileWidth() == 8 && view.tileHeight() == 8);
    check(view.patternCount() == 2);
    std::uint8_t pattern = 0xff;
    check(view.terrainPattern(0, 0, pattern) && pattern == 0);
    check(view.terrainPattern(0, 1, pattern) && pattern == 1);
    check(view.boundaryPattern(0, 0, pattern) && pattern == 0);
    check(view.objectPattern(0, pattern) && pattern == 1);
    check(!view.terrainPattern(1, 0, pattern));
    const std::array<std::uint8_t, 1> tilemap{0x81};
    const auto background = view.makeBackground(1, 1, tilemap.data(), 0x7f);
    check(background.patterns == view.patterns());
    check(background.tileIndexMask == 0x7f);
    BackgroundAssetPackView split{kPack.data(), 0};
    check(split.resetSplit(kPack.data(), 36, kPack.data() + 36, 128));
    check(split.patterns()[0] == 3 && split.patterns()[64] == 4);

    BackgroundAssetPackView truncated{kPack.data(), kPack.size() - 1};
    check(!truncated.valid());
    auto broken = kPack;
    broken[32] = 2;
    BackgroundAssetPackView badMapping{broken.data(), broken.size()};
    check(!badMapping.valid());
    check(!split.resetSplit(kPack.data(), 36, kPack.data() + 36, 127));
    return 0;
}
