#include "ui/channel_tile.h"

#include "ui/video_widget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QString>
#include <QVBoxLayout>
#include <QtGlobal>

#include <cstdio>
#include <utility>

ChannelTile::ChannelTile(ChannelConfig config, QWidget* parent)
    : QWidget(parent), config_(std::move(config)) {
    setObjectName("ChannelTile");
    setAttribute(Qt::WA_StyledBackground, true);
    fps_start_ = std::chrono::steady_clock::now();

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(1, 1, 1, 1);
    layout->setSpacing(0);

    auto* header = new QWidget(this);
    auto* header_layout = new QHBoxLayout(header);
    header_layout->setContentsMargins(6, 3, 6, 3);
    header_layout->setSpacing(6);

    name_ = new QLabel(QString::fromStdString(config_.name), header);
    name_->setObjectName("TileHeader");
    status_ = new QLabel("连接中...", header);
    status_->setObjectName("TileStatus");

    header_layout->addWidget(name_);
    header_layout->addStretch();
    header_layout->addWidget(status_);

    video_ = new VideoWidget(this);
    video_->setObjectName("VideoArea");

    footer_ = new QLabel("--", this);
    footer_->setObjectName("TileFooter");

    layout->addWidget(header);
    layout->addWidget(video_);
    layout->addWidget(footer_);
}

ChannelTile::~ChannelTile() {
    if (worker_) {
        worker_->stop();
        worker_->join();
    }
}

void ChannelTile::start() {
    const bool is_rtsp =
        config_.url.rfind("rtsp://", 0) == 0 ||
        config_.url.rfind("rtsps://", 0) == 0;

    latency_sum_ms_ = 0;
    latency_count_ = 0;
    last_latency_ms_ = 0;

    // 队列开小一点，UI 来不及取时自动丢旧帧
    worker_ = std::make_unique<DecodeWorker>(config_.url, 3, !is_rtsp);
    worker_->start();
}

void ChannelTile::onTick() {
    if (!worker_) {
        return;
    }

    FramePtr frame;
    // 非阻塞取最新帧
    if (worker_->queue().tryPopLatest(frame)) {
        latest_ = frame;
        video_->showFrame(frame);

        const auto now_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count();
        if (frame->decoded_ms > 0) {
            last_latency_ms_ = now_ms - frame->decoded_ms;
            if (last_latency_ms_ >= 0) {
                latency_sum_ms_ += last_latency_ms_;
                ++latency_count_;
            }
        }

        ++displayed_;
        ++frames_window_;

        const auto now = std::chrono::steady_clock::now();
        const auto elapsed_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                now - fps_start_)
                .count();
        if (displayed_ == 1 || elapsed_ms >= 1000) {
            if (elapsed_ms > 0) {
                fps_ = frames_window_ * 1000.0 / elapsed_ms;
            }
            frames_window_ = 0;
            fps_start_ = now;
            updateFooter(frame);
        }

        setStatus("● LIVE", "#3fb950");
        return;
    }

    if (worker_->isRunning()) {
        setStatus(worker_->isConnected() ? "● LIVE" : "● 重连中",
                  worker_->isConnected() ? "#3fb950" : "#d29922");
        return;
    }

    if (!worker_->error().empty()) {
        setStatus("● 错误", "#f85149");
    } else {
        setStatus("● 结束", "#8b98a7");
    }
}

bool ChannelTile::isRunning() const {
    return worker_ && worker_->isRunning();
}

bool ChannelTile::isConnected() const {
    return worker_ && worker_->isConnected();
}

ChannelStats ChannelTile::stats() const {
    return worker_ ? worker_->stats() : ChannelStats{};
}

void ChannelTile::setStatus(const QString& text, const char* color) {
    status_->setText(text);
    status_->setStyleSheet(QString("color: %1;").arg(color));
}

void ChannelTile::updateFooter(const FramePtr& frame) {
    if (!frame) {
        return;
    }

    const ChannelStats stats = this->stats();
    const QString fps = fps_ > 0.0 ? QString::number(fps_, 'f', 1) : "--";

    QString text = QString("%1x%2 · %3 fps")
                       .arg(frame->width)
                       .arg(frame->height)
                       .arg(fps);
    if (stats.dropped_frames > 0) {
        text += QString(" · 丢 %1")
                    .arg(static_cast<qulonglong>(stats.dropped_frames));
    }
    if (stats.reconnect_count > 0) {
        text += QString(" · 重连 %1").arg(stats.reconnect_count);
    }
    if (latency_count_ > 0) {
        text += QString(" · 延迟 %1 ms")
                    .arg(static_cast<qlonglong>(last_latency_ms_));
    }
    footer_->setText(text);

    const qlonglong avg_latency =
        latency_count_ > 0 ? latency_sum_ms_ / latency_count_ : 0;
    footer_->setToolTip(
        QString("解码 %1 · 丢弃 %2 · 重连 %3 · 恢复 %4 ms · 延迟 %5/%6 ms\n%7")
            .arg(static_cast<qulonglong>(stats.decoded_frames))
            .arg(static_cast<qulonglong>(stats.dropped_frames))
            .arg(stats.reconnect_count)
            .arg(static_cast<qlonglong>(stats.last_recovery_ms))
            .arg(static_cast<qlonglong>(last_latency_ms_))
            .arg(avg_latency)
            .arg(QString::fromStdString(stats.last_error)));

    // 设置后每秒往 stderr 打一行，方便脚本采集
    if (qEnvironmentVariableIsSet("RTSP_MONITOR_STATS")) {
        std::fprintf(stderr,
                     "[stats] %s fps=%.1f latency=%lld avg_latency=%lld "
                     "decoded=%llu dropped=%llu reconnect=%d recovery=%lld "
                     "connected=%d\n",
                     config_.name.c_str(), fps_,
                     static_cast<long long>(last_latency_ms_),
                     static_cast<long long>(avg_latency),
                     static_cast<unsigned long long>(stats.decoded_frames),
                     static_cast<unsigned long long>(stats.dropped_frames),
                     stats.reconnect_count,
                     static_cast<long long>(stats.last_recovery_ms),
                     stats.connected ? 1 : 0);
    }
}
