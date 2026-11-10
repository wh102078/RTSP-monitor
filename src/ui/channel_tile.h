#pragma once

#include <QWidget>
#include <QString>

#include <chrono>
#include <memory>

#include "core/channel_config.h"
#include "core/decode_worker.h"

class QLabel;
class VideoWidget;

class ChannelTile : public QWidget {
public:
    explicit ChannelTile(ChannelConfig config, QWidget* parent = nullptr);
    ~ChannelTile() override;

    void start();
    void onTick();

    bool isRunning() const;
    bool isConnected() const;
    ChannelStats stats() const;
    const ChannelConfig& config() const { return config_; }

private:
    void setStatus(const QString& text, const char* color);
    void updateFooter(const FramePtr& frame);

    ChannelConfig config_;
    VideoWidget* video_ = nullptr;
    QLabel* name_ = nullptr;
    QLabel* status_ = nullptr;
    QLabel* footer_ = nullptr;
    std::unique_ptr<DecodeWorker> worker_;

    FramePtr latest_;
    int displayed_ = 0;
    int frames_window_ = 0;
    double fps_ = 0.0;
    std::chrono::steady_clock::time_point fps_start_;

    int64_t latency_sum_ms_ = 0;
    int latency_count_ = 0;
    int64_t last_latency_ms_ = 0;
};
