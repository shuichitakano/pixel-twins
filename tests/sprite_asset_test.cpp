#include "pixel_twins/sprite_asset.hpp"
#include "test_check.hpp"

#include <array>
#include <cstdint>

namespace {

constexpr std::array<std::uint8_t, 54> kPack{
    'P', 'T', 'S', 'P',
    1, 0,
    24, 0,
    1, 0,
    12, 0,
    2, 0, 0, 0,
    52, 0, 0, 0,
    2, 0, 0, 0,
    0, 0, 0, 0,
    2, 0,
    2,
    1,
    4,
    4,
    0, 0,
    0, 0, 0, 0,
    1,
    1,
    2,
    1,
    0xff, 0xff, 0xff, 0xff,
    0,
    0,
    0,
    0,
    5, 5,
};

} // namespace

int main() {
    using namespace pixel_twins;

    SpriteAssetPackView view{kPack.data(), kPack.size()};
    check(view.valid());
    check(view.assetCount() == 1);

    SpriteAssetInfo asset{};
    check(view.assetInfo(0, asset));
    check(asset.frameCount == 2);
    check(asset.columns == 2 && asset.rows == 1);
    check(asset.logicalWidth == 4 && asset.logicalHeight == 4);
    check(!view.assetInfo(1, asset));

    SpriteFramePattern frame{};
    check(view.frame(0, 0, frame));
    check(frame.trimX == 1 && frame.trimY == 1);
    check(frame.width == 2 && frame.height == 1);
    check(frame.pixels != nullptr && frame.pixels[0] == 5 && frame.pixels[1] == 5);
    check(view.frame(0, 1, frame));
    check(frame.width == 0 && frame.height == 0 && frame.pixels == nullptr);
    check(!view.frame(0, 2, frame));
    check(view.frameAt(0, 0, 0, frame) && frame.width == 2);
    check(view.frameAt(0, 1, 0, frame) && frame.width == 0);
    check(!view.frameAt(0, 0, 1, frame));

    Sprite sprite{};
    check(view.makeSprite(0, 0, 10, 20, sprite));
    check(sprite.dx == 11 && sprite.dy == 21);
    check(sprite.sw == 2 && sprite.sh == 1 && sprite.p[0] == 5);
    check(view.makeSprite(0, 1, 10, 20, sprite));
    check(sprite.sw == 0 && sprite.sh == 0 && sprite.p == nullptr);
    check(view.makeSpriteAt(0, 0, 0, 10, 20, sprite));
    check(sprite.dx == 11 && sprite.dy == 21);
    SpriteAssetPackView split;
    check(split.resetSplit(kPack.data(), 52, kPack.data() + 52, 2));
    check(split.frame(0, 0, frame) && frame.pixels[0] == 5);

    SpriteAssetPackView truncated{kPack.data(), kPack.size() - 1};
    check(!truncated.valid());
    auto broken = kPack;
    broken[0] = 'X';
    SpriteAssetPackView wrongMagic{broken.data(), broken.size()};
    check(!wrongMagic.valid());
    check(!split.resetSplit(kPack.data(), 52, kPack.data() + 52, 1));
    return 0;
}
