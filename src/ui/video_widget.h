#pragma once

#include <QWidget>

#include "core/frame.h"

class QPaintEvent;

class VideoWidget : public QWidget {
public:
    explicit VideoWidget(QWidget* parent = nullptr);

    void showFrame(const FramePtr& frame);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    FramePtr frame_;
};
