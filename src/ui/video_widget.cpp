#include "ui/video_widget.h"

#include <QImage>
#include <QPainter>
#include <QPaintEvent>
#include <QSizePolicy>

VideoWidget::VideoWidget(QWidget* parent) : QWidget(parent) {
    setObjectName("VideoArea");
    setMinimumSize(160, 120);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void VideoWidget::showFrame(const FramePtr& frame) {
    if (!frame) {
        return;
    }
    frame_ = frame;  // 保持引用，避免绘制时像素被释放
    update();
}

void VideoWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);

    if (!frame_ || frame_->width <= 0 || frame_->height <= 0) {
        return;
    }

    QImage image(frame_->rgb.data(), frame_->width, frame_->height,
                 frame_->width * 3, QImage::Format_RGB888);

    const QSize target = image.size().scaled(size(), Qt::KeepAspectRatio);
    const QRect target_rect(
        QPoint((width() - target.width()) / 2,
               (height() - target.height()) / 2),
        target);

    painter.drawImage(target_rect, image);
}
