#pragma once

#include "Job.h"
#include <QMainWindow>
#include <QVector>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QLabel>
#include <QProgressBar>
#include <QPlainTextEdit>
#include <QThread>

class MainWindow:public QMainWindow{
    Q_OBJECT
public: explicit MainWindow(QWidget* p = nullptr);
    ~MainWindow() override;
private slots:
    void chooseWorkflow();
    void choosePrompts();
    void loadJobs();
    void checkConnection();
    void startQueue();
    void stopQueue();
    void clearQueue();
    void openSelectedPreview();
    void onJobStatusChanged(int,const QString&);
    void onJobPromptIdChanged(int,const QString&);
    void onPreviewReady(int,const QImage&,const QString&);
    void onProgressChanged(int,int);
    void onLogMessage(const QString&);
    void onQueueFinished();
    void onQueueStopped();
private:
    void chooseOutputDirectory();
    void openOutputFolder();
    QString outputDirectory() const;
    void buildUi();
    void loadSettings();
    void saveSettings();
    void setRunning(bool);
    void setRow(int,int,const QString&);
    void setRowPreview(int row, const QImage &image);
protected:
    void closeEvent(QCloseEvent *event) override;
private:
    QLineEdit *m_urlEdit,*m_workflowEdit,*m_promptsEdit,*m_positivePathEdit,*m_negativePathEdit,*m_seedPathEdit, *m_outputEdit;
    QPushButton *m_loadButton,*m_checkButton,*m_startButton,*m_stopButton,*m_clearButton;
    QTableWidget *m_table;
    QProgressBar *m_progress; QPlainTextEdit *m_log;
    QVector<Job> m_jobs;
    QThread *m_workerThread=nullptr;
    QObject *m_worker=nullptr;
    bool m_running;
};