#pragma once
#include "Job.h"
#include <QObject>
#include <QJsonObject>
#include <QVector>
#include <atomic>
class QueueManager : public QObject
{
    Q_OBJECT
public:
    explicit QueueManager(QObject *parent = nullptr);
public slots:
    void start(QVector<Job> jobs, QString baseUrl, QString workflowFile, QString positivePath,
               QString negativePath, QString seedPath, QString outputDirectory);
    void stop();
signals:
    void jobStatusChanged(int index, const QString &status);
    void jobPromptIdChanged(int index, const QString &promptId);
    void previewReady(int index, const QImage &image, const QString &path);
    void progressChanged(int done, int total);
    void logMessage(const QString &message);
    void finished();
    void stopped();

private:
    bool loadWorkflow(const QString &, QJsonObject *, QString *);
    bool setJsonPath(QJsonObject &, const QString &, const QJsonValue &);
    bool setNested(QJsonObject &, const QStringList &, int, const QJsonValue &);
    bool processJob(Job &, const QJsonObject &, class ComfyApi &, const QString &, const QString &, const QString &, const QString &);
    std::atomic_bool m_stopRequested{false};
};
