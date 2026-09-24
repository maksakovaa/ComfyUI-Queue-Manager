#pragma once
#include <QObject>
#include <QJsonObject>
#include <QByteArray>
class QNetworkAccessManager;
class ComfyApi : public QObject
{
    Q_OBJECT
public:
    explicit ComfyApi(const QString &baseUrl, QObject *parent = nullptr);
    bool checkConnection(QString *error = nullptr);
    bool submitPrompt(const QJsonObject &workflow, QString *promptId, QString *error = nullptr);
    bool waitForHistory(const QString &promptId, QJsonObject *history,
                        int timeoutMs = 1800000, QString *error = nullptr);
    bool downloadImage(const QString &filename, const QString &subfolder,
                       const QString &type, QByteArray *data, QString *error = nullptr);
    bool clearQueue(QString *error = nullptr);

private:
    QByteArray request(const QUrl &url, const QByteArray &body, const QByteArray &method,
                       int *statusCode, QString *error);
    QByteArray get(const QUrl &url, int *statusCode, QString *error);
    QByteArray post(const QUrl &url, const QByteArray &body, int *statusCode, QString *error);
    QNetworkAccessManager *m_network = nullptr;
    QString m_baseUrl;
};
