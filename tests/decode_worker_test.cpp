#include "core/decode_worker.h"

#include <cstdio>
#include <memory>

int main(int argc, char* argv[]) {
    const char* path = argc > 1 ? argv[1] : "media/gate.mp4";

    DecodeWorker worker(path, 1000);
    worker.start();
    worker.join();   // 等本地文件解码结束

    int count = 0;
    FramePtr frame;
    while (worker.queue().pop(frame)) {
        ++count;
    }

    const ChannelStats stats = worker.stats();
    std::printf("decoded %d frame(s)\n", count);
    std::printf("stats: decoded=%llu dropped=%llu reconnects=%d\n",
                static_cast<unsigned long long>(stats.decoded_frames),
                static_cast<unsigned long long>(stats.dropped_frames),
                stats.reconnect_count);

    if (!worker.error().empty()) {
        std::printf("error: %s\n", worker.error().c_str());
        return 1;
    }

    return count > 0 && stats.decoded_frames == static_cast<uint64_t>(count)
               ? 0
               : 1;
}
