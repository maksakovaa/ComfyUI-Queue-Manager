#include "QueueManager.h"
#include "ComfyApi.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDir>
#include <QFileInfo>

QueueManager::QueueManager(QObject *p) : QObject(p) {}
void QueueManager::stop() { m_stopRequested.store(true); }
bool QueueManager::loadWorkflow(const QString &f, QJsonObject *w, QString *e)
{
    QFile file(f);
    if (!file.open(QIODevice::ReadOnly))
    {
        if (e)
            *e = file.errorString();
        return false;
    }
    QJsonParseError pe;
    auto d = QJsonDocument::fromJson(file.readAll(), &pe);
    if (pe.error != QJsonParseError::NoError || !d.isObject())
    {
        if (e)
            *e = pe.errorString();
        return false;
    }
    *w = d.object();
    return true;
}
bool QueueManager::setNested(QJsonObject &o, const QStringList &p, int i, const QJsonValue &v)
{
    if (i >= p.size())
        return false;
    QString k = p[i];
    if (i == p.size() - 1)
    {
        o[k] = v;
        return true;
    }
    QJsonObject child = o.value(k).toObject();
    if (o.contains(k) && !o.value(k).isObject())
        return false;
    if (!setNested(child, p, i + 1, v))
        return false;
    o[k] = child;
    return true;
}
bool QueueManager::setJsonPath(QJsonObject &o, const QString &path, const QJsonValue &v)
{
    auto p = path.split('.', Qt::SkipEmptyParts);
    return !p.isEmpty() && setNested(o, p, 0, v);
}
bool QueueManager::processJob(Job &job, const QJsonObject &base, ComfyApi &api, const QString &pp,
                              const QString &np, const QString &sp, const QString &outputDirectory)
{
    QJsonObject w = base;
    if (!pp.isEmpty() && !setJsonPath(w, pp, job.positive))
    {
        emit logMessage("Не найден positive path.");
        return false;
    }
    if (!np.isEmpty() && !job.negative.isEmpty() && !setJsonPath(w, np, job.negative))
    {
        emit logMessage("Не найден negative path.");
        return false;
    }
    if (!sp.isEmpty() && !job.seed.isEmpty())
    {
        bool ok = false;
        qint64 n = job.seed.toLongLong(&ok);
        if (!setJsonPath(w, sp, ok ? QJsonValue(n) : QJsonValue(job.seed)))
        {
            emit logMessage("Не найден seed path.");
            return false;
        }
    }
    emit jobStatusChanged(job.index, "Отправка...");
    QString error, id;
    if (!api.submitPrompt(w, &id, &error))
    {
        emit logMessage(QString("#%1: %2").arg(job.index).arg(error));
        return false;
    }
    emit jobPromptIdChanged(job.index, id);
    emit jobStatusChanged(job.index, "Генерация...");
    QJsonObject history;
    if (!api.waitForHistory(id, &history, 1800000, &error))
    {
        emit logMessage(QString("#%1: %2").arg(job.index).arg(error));
        return false;
    }

    QString fileName, subFolder, type = "output";

    const auto outputs = history.value("outputs").toObject();

    for (auto it = outputs.begin(); it != outputs.end(); ++it)
    {
        const auto images = it.value().toObject().value("images").toArray();
        if(images.isEmpty())
            continue;

        const auto imageInfo = images.first().toObject();
        fileName = imageInfo.value("filename").toString();

        fileName = imageInfo.value("filename").toString();
        subFolder = imageInfo.value("subfolder").toString();
        type = imageInfo.value("type").toString("output");

        break;
    }

    QString localPath;

    if (!outputDirectory.trimmed().isEmpty()) {
        QString relativePath;

        if (subFolder.isEmpty()) {
            relativePath = fileName;
        } else {
            relativePath = QDir(subFolder).filePath(fileName);
        }

        localPath = QDir(outputDirectory).absoluteFilePath(relativePath);
        localPath = QDir::cleanPath(localPath);
    }

    QImage image;

    if (!localPath.isEmpty() && QFileInfo::exists(localPath)) {
        image.load(localPath);
    }

    if (image.isNull()) {
        QByteArray bytes;
        QString error;

        if (!api.downloadImage(fileName,
                               subFolder,
                               type,
                               &bytes,
                               &error)) {
            emit logMessage(error);
            return false;
        }

        image.loadFromData(bytes);
    }
    // for (auto it = outputs.begin(); it != outputs.end() && fn.isEmpty(); ++it)
    // {
    //     auto imgs = it.value().toObject().value("images").toArray();
    //     if (!imgs.isEmpty())
    //     {
    //         auto im = imgs.first().toObject();
    //         fn = im.value("filename").toString();
    //         sf = im.value("subfolder").toString();
    //         type = im.value("type").toString("output");
    //     }
    // }
    // if (fn.isEmpty())
    // {
    //     emit logMessage(QString("#%1: изображение не найдено.").arg(job.index));
    //     return false;
    // }
    // QByteArray bytes;
    // if (!api.downloadImage(fn, sf, type, &bytes, &error))
    // {
    //     emit logMessage(QString("#%1: %2").arg(job.index).arg(error));
    //     return false;
    // }
    // QImage image;
    // if (!image.loadFromData(bytes))
    // {
    //     emit logMessage(QString("#%1: данные не являются изображением.").arg(job.index));
    //     return false;
    // }
    // QDir dir(dirPath);
    // if (!dir.exists() && !dir.mkpath("."))
    // {
    //     emit logMessage("Не удалось создать previews.");
    //     return false;
    // }
    // QString ext = QFileInfo(fn).suffix().toLower();
    // if (ext.isEmpty())
    //     ext = "png";
    // QString path = dir.filePath(QString("job_%1.%2").arg(job.index, 4, 10, QChar('0')).arg(ext));
    // if (!image.save(path))
    // {
    //     emit logMessage("Не удалось сохранить preview.");
    //     return false;
    // }
    job.preview = image;
    job.imagePath = localPath;
    job.status = "Готово";
    emit previewReady(job.index, image, localPath);
    emit jobStatusChanged(job.index, "Готово");
    return true;
}
void QueueManager::start(QVector<Job> jobs, QString url, QString wf, QString pp, QString np, QString sp, QString outputDirectory)
{
    m_stopRequested.store(false);
    QJsonObject base;
    QString e;
    if (!loadWorkflow(wf, &base, &e))
    {
        emit logMessage("Workflow: " + e);
        emit finished();
        return;
    }
    ComfyApi api(url);
    emit logMessage("Проверка ComfyUI...");
    if (!api.checkConnection(&e))
    {
        emit logMessage("ComfyUI недоступен: " + e);
        emit finished();
        return;
    }
    emit logMessage("✓ ComfyUI доступен.");
    int done = 0;
    emit progressChanged(0, jobs.size());
    for (Job &j : jobs)
    {
        if (m_stopRequested.load())
        {
            emit jobStatusChanged(j.index, "Остановлено");
            break;
        }
        emit jobStatusChanged(j.index, "В очереди");
        processJob(j, base, api, pp, np, sp, outputDirectory);
        emit progressChanged(++done, jobs.size());
    }
    if (m_stopRequested.load())
        emit stopped();
    else
        emit finished();
}
