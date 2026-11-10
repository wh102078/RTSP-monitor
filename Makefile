CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 $(shell pkg-config --cflags libavformat libavcodec libswscale libavutil)
LDLIBS   = $(shell pkg-config --libs   libavformat libavcodec libswscale libavutil)

QT_CFLAGS = $(shell pkg-config --cflags Qt6Widgets)
QT_LIBS   = $(shell pkg-config --libs   Qt6Widgets)

SANFLAGS = -fsanitize=address,undefined -fno-omit-frame-pointer

CORE_SRCS = src/core/decode_worker.cpp src/core/video_decoder.cpp src/core/frame_queue.cpp
UI_SRCS   = src/ui/main_window.cpp src/ui/channel_tile.cpp src/ui/video_widget.cpp src/ui/config_loader.cpp src/ui/theme.cpp

all: rtsp_monitor

rtsp_monitor: src/main.cpp $(UI_SRCS) $(CORE_SRCS)
	$(CXX) $(CXXFLAGS) $(QT_CFLAGS) -I src -pthread src/main.cpp $(UI_SRCS) $(CORE_SRCS) -o $@ $(QT_LIBS) $(LDLIBS)

test_frame_queue: src/core/frame_queue.cpp tests/frame_queue_test.cpp
	$(CXX) $(CXXFLAGS) -I src -pthread src/core/frame_queue.cpp tests/frame_queue_test.cpp -o $@

test_video_decoder: src/core/video_decoder.cpp tests/video_decoder_test.cpp
	$(CXX) $(CXXFLAGS) -I src src/core/video_decoder.cpp tests/video_decoder_test.cpp -o $@ $(LDLIBS)

test_decode_worker: src/core/decode_worker.cpp src/core/video_decoder.cpp src/core/frame_queue.cpp tests/decode_worker_test.cpp
	$(CXX) $(CXXFLAGS) -I src -pthread src/core/decode_worker.cpp src/core/video_decoder.cpp src/core/frame_queue.cpp tests/decode_worker_test.cpp -o $@ $(LDLIBS)

test: test_frame_queue test_video_decoder test_decode_worker
	./test_frame_queue
	./test_video_decoder media/gate.mp4
	./test_decode_worker media/gate.mp4

test_frame_queue_asan: src/core/frame_queue.cpp tests/frame_queue_test.cpp
	$(CXX) $(CXXFLAGS) $(SANFLAGS) -I src -pthread src/core/frame_queue.cpp tests/frame_queue_test.cpp -o $@

test_video_decoder_asan: src/core/video_decoder.cpp tests/video_decoder_test.cpp
	$(CXX) $(CXXFLAGS) $(SANFLAGS) -I src src/core/video_decoder.cpp tests/video_decoder_test.cpp -o $@ $(LDLIBS)

test_decode_worker_asan: src/core/decode_worker.cpp src/core/video_decoder.cpp src/core/frame_queue.cpp tests/decode_worker_test.cpp
	$(CXX) $(CXXFLAGS) $(SANFLAGS) -I src -pthread src/core/decode_worker.cpp src/core/video_decoder.cpp src/core/frame_queue.cpp tests/decode_worker_test.cpp -o $@ $(LDLIBS)

asan-tests: test_frame_queue_asan test_video_decoder_asan test_decode_worker_asan
	ASAN_OPTIONS=detect_leaks=1 ./test_frame_queue_asan
	ASAN_OPTIONS=detect_leaks=1 ./test_video_decoder_asan media/gate.mp4
	ASAN_OPTIONS=detect_leaks=1 ./test_decode_worker_asan media/gate.mp4

clean:
	rm -f rtsp_monitor test_frame_queue test_video_decoder test_decode_worker \
	      test_frame_queue_asan test_video_decoder_asan test_decode_worker_asan

.PHONY: all test asan-tests clean
