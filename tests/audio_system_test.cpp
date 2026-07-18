#include "pixel_twins/audio_system.hpp"
#include "pixel_twins/sound_waves.hpp"

#include "test_check.hpp"

#include <cstddef>

namespace {

using namespace pixel_twins;

const Timbre kLongLeft{
    &kStandardWaves.square, Envelope{0.0F, 0.0F, 1.0F, 0.0F}, 0.1F, -1.0F};
const Timbre kLongRight{
    &kStandardWaves.square, Envelope{0.0F, 0.0F, 1.0F, 0.0F}, 0.1F, 1.0F};

SfxRequest request(const Timbre& timbre, std::uint8_t priority) {
    return SfxRequest{VoiceStart{&timbre, 1500.0F, 1500.0F, 0.0F, 1.0F, 1.0F, 0.0F},
                      priority};
}

void testQueueCapacityAndOrder() {
    SfxRequestQueue queue;
    for (std::size_t i = 0; i < kSfxRequestCapacity; ++i) {
        check(queue.tryPush(request(kLongLeft, static_cast<std::uint8_t>(i))));
    }
    check(!queue.tryPush(request(kLongLeft, 255)));
    for (std::size_t i = 0; i < kSfxRequestCapacity; ++i) {
        SfxRequest value;
        check(queue.tryPop(value));
        check(value.priority == i);
    }
    SfxRequest value;
    check(!queue.tryPop(value));
}

void testPriorityPreventsSteal() {
    AudioSystem audio;
    for (std::size_t i = 0; i < kSfxVoiceCount; ++i) check(audio.playSfx(request(kLongLeft, 10)));
    AudioBlock output{};
    audio.renderBlock(output);
    check(output[0] > 0);
    check(output[1] == 0);

    check(audio.playSfx(request(kLongRight, 9)));
    audio.renderBlock(output);
    check(output[0] > 0);
    check(output[1] == 0);

    check(audio.playSfx(request(kLongRight, 11)));
    audio.renderBlock(output);
    check(output[0] > 0);
    check(output[1] > 0);
}

void testInvalidRequestIsRejected() {
    AudioSystem audio;
    check(!audio.playSfx(SfxRequest{}));
}

void testStopAllDiscardsPendingRequests() {
    AudioSystem audio;
    check(audio.playSfx(request(kLongLeft, 1)));
    audio.stopAll();
    AudioBlock output{};
    audio.renderBlock(output);
    check(output[0] == 0);
}

} // namespace

int main() {
    testQueueCapacityAndOrder();
    testPriorityPreventsSteal();
    testInvalidRequestIsRejected();
    testStopAllDiscardsPendingRequests();
    return 0;
}
