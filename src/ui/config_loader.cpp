#include "ui/config_loader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QString>

#include <utility>

bool loadChannelConfigs(const std::string& path,
                        std::vector<ChannelConfig>& out,
                        std::string& error) {
    out.clear();
    error.clear();

    QFile file(QString::fromStdString(path));
    if (!file.open(QIODevice::ReadOnly)) {
        error = "cannot open config: " + path;
        return false;
    }

    const QDir config_dir = QFileInfo(QString::fromStdString(path)).absoluteDir();

    QJsonParseError parse_error;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parse_error);
    if (parse_error.error != QJsonParseError::NoError) {
        error = parse_error.errorString().toStdString();
        return false;
    }

    QJsonArray array;
    if (doc.isArray()) {
        array = doc.array();
    } else if (doc.isObject()) {
        array = doc.object().value("channels").toArray();
    }

    for (const auto& value : array) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject object = value.toObject();
        ChannelConfig config;
        config.name = object.value("name").toString().toStdString();
        config.url = object.value("url").toString().toStdString();

        if (config.url.empty()) {
            continue;
        }

        // 本地相对路径按配置文件所在目录解析
        if (config.url.find("://") == std::string::npos) {
            const QFileInfo file_info(QString::fromStdString(config.url));
            if (file_info.isRelative()) {
                config.url = QDir::cleanPath(
                                 config_dir.filePath(file_info.filePath()))
                                 .toStdString();
            }
        }

        if (config.name.empty()) {
            config.name = "通道 " + std::to_string(out.size() + 1);
        }
        out.push_back(std::move(config));
    }

    if (out.empty()) {
        error = "no valid channels in config";
        return false;
    }
    return true;
}
