#include "ui/main_window.h"

#include "ui/channel_tile.h"

#include <QGridLayout>
#include <QGuiApplication>
#include <QLabel>
#include <QScreen>
#include <QStatusBar>
#include <QString>
#include <QWidget>

#include <algorithm>
#include <cmath>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    auto* central = new QWidget(this);
    central->setObjectName("Central");
    grid_ = new QGridLayout(central);
    grid_->setContentsMargins(8, 8, 8, 8);
    grid_->setSpacing(8);
    setCentralWidget(central);

    setWindowTitle("RTSP 多路监控客户端");

    status_label_ = new QLabel("--", this);
    status_label_->setObjectName("StatusText");
    statusBar()->addWidget(status_label_);

    if (auto* screen = QGuiApplication::primaryScreen()) {
        const QRect available = screen->availableGeometry();
        resize(std::min(1280, available.width() - 60),
               std::min(960, available.height() - 100));
    } else {
        resize(1280, 900);
    }

    last_status_update_ = std::chrono::steady_clock::now();

    connect(&timer_, &QTimer::timeout, this, &MainWindow::onTick);
    timer_.setInterval(20);
}

MainWindow::~MainWindow() {
    timer_.stop();
}

void MainWindow::start(const std::vector<ChannelConfig>& channels) {
    if (!tiles_.empty() || channels.empty()) {
        return;
    }

    const int count = static_cast<int>(channels.size());
    const int columns =
        std::max(1, static_cast<int>(std::ceil(std::sqrt(count))));

    for (int i = 0; i < count; ++i) {
        auto* tile = new ChannelTile(channels[static_cast<size_t>(i)], this);
        tiles_.push_back(tile);
        grid_->addWidget(tile, i / columns, i % columns);
        tile->start();
    }

    timer_.start();
}

void MainWindow::onTick() {
    for (auto* tile : tiles_) {
        tile->onTick();
    }

    const auto now = std::chrono::steady_clock::now();
    if (now - last_status_update_ >= std::chrono::seconds(1)) {
        updateStatusBar();
        last_status_update_ = now;
    }
}

void MainWindow::updateStatusBar() {
    int connected = 0;
    int reconnecting = 0;

    for (auto* tile : tiles_) {
        if (tile->isConnected()) {
            ++connected;
        } else if (tile->isRunning()) {
            ++reconnecting;
        }
    }

    status_label_->setText(QString("在线 %1 · 重连 %2 · 共 %3")
                               .arg(connected)
                               .arg(reconnecting)
                               .arg(tiles_.size()));
}
