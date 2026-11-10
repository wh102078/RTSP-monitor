#include "core/video_decoder.h"

#include <cstdio>

int main(int argc, char* argv[]) {
    const char* path = argc > 1 ? argv[1] : "media/gate.mp4";

    VideoDecoder decoder;
    std::string error;

    if (!decoder.open(path, error)) {
        std::printf("open failed: %s\n", error.c_str());
        return 1;
    }

    std::printf("size: %dx%d\n", decoder.width(), decoder.height());

    int count = 0;
    for (int i = 0; i < 10; ++i) {
        FramePtr frame;
        if (!decoder.readFrame(frame, error)) {
            std::printf("read stopped at %d: %s\n", i, error.c_str());
            break;
        }

        std::printf("frame %d: %dx%d rgb=%zu pts=%lld\n",
                    i, frame->width, frame->height, frame->rgb.size(),
                    static_cast<long long>(frame->pts));
        ++count;
    }

    decoder.close();
    std::printf("decoded %d frame(s)\n", count);

    return count == 10 ? 0 : 1;
}
