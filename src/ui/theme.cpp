#include "ui/theme.h"

QString darkStyleSheet() {
    return QStringLiteral(R"(
        QMainWindow, QWidget#Central {
            background: #0f131a;
        }
        QWidget#ChannelTile {
            background: #161c25;
            border: 1px solid #2a3542;
            border-radius: 6px;
        }
        QLabel#TileHeader {
            color: #e6edf3;
            font-size: 13px;
            font-weight: 600;
        }
        QLabel#TileStatus {
            color: #8b98a7;
            font-size: 11px;
        }
        QLabel#TileFooter {
            color: #8b98a7;
            font-size: 11px;
            padding: 2px 6px;
        }
        QWidget#VideoArea {
            background: #000000;
        }
        QStatusBar {
            background: #0f131a;
            color: #8b98a7;
        }
        QLabel#StatusText {
            color: #8b98a7;
            font-size: 11px;
            padding-left: 8px;
        }
    )");
}
