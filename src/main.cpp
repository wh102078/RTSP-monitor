#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QString>

#include "core/channel_config.h"
#include "ui/config_loader.h"
#include "ui/main_window.h"
#include "ui/theme.h"

#include <string>
#include <vector>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setStyleSheet(darkStyleSheet());

    std::vector<ChannelConfig> channels;

    if (argc > 1) {
        for (int i = 1; i < argc; ++i) {
            ChannelConfig config;
            config.name = "通道 " + std::to_string(i);
            config.url = argv[i];
            channels.push_back(std::move(config));
        }
    } else {
        const QString config_path =
            QDir(QCoreApplication::applicationDirPath())
                .filePath("config/channels.json");

        std::string error;
        if (!loadChannelConfigs(config_path.toStdString(), channels, error)) {
            // 配置读取失败时退回本地演示视频
            qWarning() << "load config failed:"
                       << QString::fromStdString(error)
                       << "- fallback to media/gate.mp4";
            const QString fallback =
                QDir(QCoreApplication::applicationDirPath())
                    .filePath("media/gate.mp4");
            channels.push_back({"通道 1", fallback.toStdString()});
        }
    }

    MainWindow window;
    window.start(channels);
    window.show();

    return app.exec();
}
