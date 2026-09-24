#include "ComfyApi.h"
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonArray>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QRegularExpression>

ComfyApi::ComfyApi(const QString &baseUrl, QObject *parent)
    : QObject(parent), m_network(new QNetworkAccessManager(this)),
      m_baseUrl(baseUrl.trimmed().remove(QRegularExpression("/+$"))) {}

QByteArray ComfyApi::request(const QUrl &url, const QByteArray &body,
                             const QByteArray &method, int *statusCode, QString *error)
{
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Accept", "application/json");
    QNetworkReply *reply = nullptr;
    if (method == "GET")
        reply = m_network->get(req);
    else if (method == "POST")
        reply = m_network->post(req, body);
    else
        reply = m_network->sendCustomRequest(req, method, body);

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(30000);
    loop.exec();

    if (!reply->isFinished())
    {
        reply->abort();
        if (error)
            *error = "Тайм-аут запроса к ComfyUI.";
        reply->deleteLater();
        return {};
    }
    if (statusCode)
        *statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QByteArray data = reply->readAll();
    if (reply->error() != QNetworkReply::NoError)
    {
        if (error)
            *error = reply->errorString();
        reply->deleteLater();
        return {};
    }
    reply->deleteLater();
    return data;
}
QByteArray ComfyApi::get(const QUrl &u, int *s, QString *e) { return request(u, {}, "GET", s, e); }
QByteArray ComfyApi::post(const QUrl &u, const QByteArray &b, int *s, QString *e) { return request(u, b, "POST", s, e); }

bool ComfyApi::checkConnection(QString *error)
{
    int code = 0;
    get(QUrl(m_baseUrl + "/system_stats"), &code, error);
    return code >= 200 && code < 300;
}
bool ComfyApi::submitPrompt(const QJsonObject &workflow, QString *promptId, QString *error)
{
    QJsonObject payload{{"prompt", workflow}, {"client_id", "Qt6QueueManager"}};
    int code = 0;
    QByteArray data = post(QUrl(m_baseUrl + "/prompt"),
                           QJsonDocument(payload).toJson(QJsonDocument::Compact), &code, error);
    if (code < 200 || code >= 300)
    {
        if (error && error->isEmpty())
            *error = QString("HTTP %1: %2").arg(code).arg(QString::fromUtf8(data));
        return false;
    }
    QJsonParseError pe;
    auto doc = QJsonDocument::fromJson(data, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject())
    {
        if (error)
            *error = "Некорректный ответ /prompt.";
        return false;
    }
    QString id = doc.object().value("prompt_id").toString();
    if (id.isEmpty())
    {
        if (error)
            *error = "В ответе /prompt нет prompt_id.";
        return false;
    }
    if (promptId)
        *promptId = id;
    return true;
}
bool ComfyApi::waitForHistory(const QString &promptId, QJsonObject *history, int timeoutMs, QString *error)
{
    int elapsed = 0;
    while (elapsed < timeoutMs)
    {
        int code = 0;
        QString e;
        QByteArray data = get(QUrl(m_baseUrl + "/history/" + QString::fromUtf8(QUrl::toPercentEncoding(promptId))), &code, &e);
        if (code >= 200 && code < 300)
        {
            QJsonParseError pe;
            auto doc = QJsonDocument::fromJson(data, &pe);
            if (pe.error == QJsonParseError::NoError && doc.isObject())
            {
                auto record = doc.object().value(promptId).toObject();
                if (!record.isEmpty())
                {
                    auto status = record.value("status").toObject();
                    for (const auto &v : status.value("messages").toArray())
                    {
                        auto a = v.toArray();
                        if (!a.isEmpty() && a[0].toString() == "execution_error")
                        {
                            if (error)
                                *error = "ComfyUI сообщил execution_error.";
                            return false;
                        }
                    }
                    if (status.value("completed").toBool() || record.contains("outputs"))
                    {
                        if (history)
                            *history = record;
                        return true;
                    }
                }
            }
        }
        QThread::msleep(1000);
        elapsed += 1000;
    }
    if (error)
        *error = "Истекло время ожидания результата.";
    return false;
}
bool ComfyApi::downloadImage(const QString &filename, const QString &subfolder,
                             const QString &type, QByteArray *data, QString *error)
{
    QUrl u(m_baseUrl + "/view");
    QUrlQuery q;
    q.addQueryItem("filename", filename);
    q.addQueryItem("subfolder", subfolder);
    q.addQueryItem("type", type);
    u.setQuery(q);
    int code = 0;
    QByteArray b = get(u, &code, error);
    if (code < 200 || code >= 300 || b.isEmpty())
    {
        if (error && error->isEmpty())
            *error = QString("HTTP %1 при получении изображения.").arg(code);
        return false;
    }
    if (data)
        *data = b;
    return true;
}
bool ComfyApi::clearQueue(QString *error)
{
    int code = 0;
    QJsonObject p{{"clear", true}};
    QByteArray b = post(QUrl(m_baseUrl + "/queue"), QJsonDocument(p).toJson(QJsonDocument::Compact), &code, error);
    if (code < 200 || code >= 300)
    {
        if (error && error->isEmpty())
            *error = QString("HTTP %1: %2").arg(code).arg(QString::fromUtf8(b));
        return false;
    }
    return true;
}
