#pragma once

#include <QMainWindow>
#include <QTimer>

#include <chrono>
#include <vector>

#include "core/channel_config.h"

class ChannelTile;
class QGridLayout;
class QLabel;

class MainWindow : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    void start(const std::vector<ChannelConfig>& channels);

private:
    void onTick();
    void updateStatusBar();

    std::vector<ChannelTile*> tiles_;  // 不拥有，Qt 父子关系负责释放
    QGridLayout* grid_ = nullptr;
    QTimer timer_;
    QLabel* status_label_ = nullptr;
    std::chrono::steady_clock::time_point last_status_update_;
};
