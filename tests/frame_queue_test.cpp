#include "core/frame_queue.h"

#include <cstdio>
#include <memory>

static int failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::printf("FAIL (line %d): %s\n", __LINE__, #cond);          \
            ++failures;                                                    \
        }                                                                  \
    } while (0)

static FramePtr makeFrame(int value) {
    auto frame = std::make_shared<Frame>();
    frame->width = value;
    frame->height = 1;
    frame->rgb.resize(3);
    return frame;
}

int main() {
    FramePtr out;

    FrameQueue empty(3);
    CHECK(!empty.tryPopLatest(out));

    FrameQueue queue(3);
    queue.push(makeFrame(1));
    queue.push(makeFrame(2));
    queue.push(makeFrame(3));
    CHECK(queue.tryPopLatest(out));
    CHECK(out && out->width == 3);
    CHECK(!queue.tryPopLatest(out));

    FrameQueue limited(2);
    limited.push(makeFrame(10));
    limited.push(makeFrame(11));
    limited.push(makeFrame(12));       // 容量 2，帧 10 被丢弃
    CHECK(limited.dropped() == 1);
    CHECK(limited.tryPopLatest(out));
    CHECK(out && out->width == 12);

    FrameQueue stopped(2);
    stopped.stop();
    CHECK(!stopped.pop(out));

    if (failures == 0) {
        std::printf("all tests passed\n");
        return 0;
    }
    std::printf("%d test(s) failed\n", failures);
    return 1;
}
