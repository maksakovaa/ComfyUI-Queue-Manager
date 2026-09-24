#include "MainWindow.h"
#include "QueueManager.h"
#include "ComfyApi.h"
#include <QApplication>
#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QMessageBox>
#include <QSettings>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QUrl>
#include <QDir>
#include <QTextStream>

// static QString elidePrompt(const QString &s) { return s.size() > 90 ? s.left(87) + "..." : s; }

MainWindow::MainWindow(QWidget *p) : QMainWindow(p)
{
    setWindowTitle("ComfyUI Queue Manager");
    resize(1280, 820);
    buildUi();
    loadSettings();
}
MainWindow::~MainWindow()
{
    if (m_workerThread)
    {
        if (auto *m = qobject_cast<QueueManager *>(m_worker))
            m->stop();
        m_workerThread->quit();
        m_workerThread->wait(3000);
    }
    saveSettings();
}

void MainWindow::buildUi()
{
    auto *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    auto *mainLayout = new QVBoxLayout(centralWidget);
    auto *box = new QGroupBox("ComfyUI и workflow", this);
    auto *form = new QFormLayout(box);

    m_urlEdit = new QLineEdit("http://127.0.0.1:8188", this);

    auto *workflowWidget = new QWidget(this);
    auto *workflowLayout = new QHBoxLayout(workflowWidget);
    workflowLayout->setContentsMargins(0, 0, 0, 0);
    m_workflowEdit = new QLineEdit(this);
    auto *workflowButton = new QPushButton("Обзор…", this);
    workflowLayout->addWidget(m_workflowEdit);
    workflowLayout->addWidget(workflowButton);

    auto *promtWidget = new QWidget(this);
    auto *promtLayout = new QHBoxLayout(promtWidget);
    promtLayout->setContentsMargins(0, 0, 0, 0);
    m_promptsEdit = new QLineEdit(this);
    auto *promtButton = new QPushButton("Обзор…", this);
    promtLayout->addWidget(m_promptsEdit);
    promtLayout->addWidget(promtButton);

    m_outputEdit = new QLineEdit(this);
    m_outputEdit->setPlaceholderText("~/");
    auto *outputWidget = new QWidget(this);
    auto *outputLayout = new QHBoxLayout(outputWidget);
    outputLayout->setContentsMargins(0,0,0,0);
    auto *outputBrowse = new QPushButton("Обзор…", this);
    auto *outputOpen = new QPushButton("Открыть", this);
    outputLayout->addWidget(m_outputEdit);
    outputLayout->addWidget(outputBrowse);
    outputLayout->addWidget(outputOpen);

    connect(outputBrowse, &QPushButton::clicked, this, &MainWindow::chooseOutputDirectory);
    connect(outputOpen, &QPushButton::clicked, this, &MainWindow::openOutputFolder);

    m_positivePathEdit = new QLineEdit("6.inputs.text", this);
    m_negativePathEdit = new QLineEdit("7.inputs.text", this);
    m_seedPathEdit = new QLineEdit("3.inputs.seed", this);
    form->addRow("Папка output ComfyUI:",outputWidget);
    form->addRow("URL:", m_urlEdit);
    form->addRow("Workflow API JSON:", workflowWidget);
    form->addRow("Prompts TXT:", promtWidget);
    form->addRow("Positive path:", m_positivePathEdit);
    form->addRow("Negative path:", m_negativePathEdit);
    form->addRow("Seed path:", m_seedPathEdit);
    mainLayout->addWidget(box);
    connect(workflowButton, &QPushButton::clicked, this, &MainWindow::chooseWorkflow);
    connect(promtButton, &QPushButton::clicked, this, &MainWindow::choosePrompts);
    auto *br = new QHBoxLayout;
    m_loadButton = new QPushButton("Загрузить задания", this);
    m_checkButton = new QPushButton("Проверить ComfyUI", this);
    m_startButton = new QPushButton("▶ Запустить очередь", this);
    m_stopButton = new QPushButton("■ Остановить", this);
    m_clearButton = new QPushButton("Очистить очередь ComfyUI", this);
    br->addWidget(m_loadButton);
    br->addWidget(m_checkButton);
    br->addStretch();
    br->addWidget(m_startButton);
    br->addWidget(m_stopButton);
    br->addWidget(m_clearButton);
    mainLayout->addLayout(br);

    auto *split = new QSplitter(Qt::Horizontal, this);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(6);
    m_table->setHorizontalHeaderLabels({"#", "Статус", "Prompt", "Preview", "Seed", "Prompt ID"});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->hide();
    m_table->setWordWrap(true);
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->setColumnWidth(0, 45);    // #
    m_table->setColumnWidth(1, 120);   // статус
    m_table->setColumnWidth(2, 500);   // prompt
    m_table->setColumnWidth(3, 140);   // preview
    m_table->setColumnWidth(4, 120);   // seed
    m_table->setColumnWidth(5, 250);   // prompt ID

    m_table->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(3,QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(2,QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(4,QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(5,QHeaderView::Fixed);

    m_table->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    split->addWidget(m_table);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
            if (row < 0 || row >= m_jobs.size())
                return;

            const QString path = m_jobs[row].imagePath;

            if (!path.isEmpty() && QFileInfo::exists(path)) {
                QDesktopServices::openUrl(
                    QUrl::fromLocalFile(path)
                    );
            }
        }
    );

    mainLayout->addWidget(split, 1);
    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 100);
    m_log = new QPlainTextEdit(this);
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(1000);
    m_log->setMaximumHeight(130);
    mainLayout->addWidget(m_progress);
    mainLayout->addWidget(m_log);
    connect(m_loadButton, &QPushButton::clicked, this, &MainWindow::loadJobs);
    connect(m_checkButton, &QPushButton::clicked, this, &MainWindow::checkConnection);
    connect(m_startButton, &QPushButton::clicked, this, &MainWindow::startQueue);
    connect(m_stopButton, &QPushButton::clicked, this, &MainWindow::stopQueue);
    connect(m_clearButton, &QPushButton::clicked, this, &MainWindow::clearQueue);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int, int)
            { openSelectedPreview(); });
    m_stopButton->setEnabled(false);
}
void MainWindow::loadSettings()
{
    QSettings s("ComfyUIQueueManager", "Qt6");
    m_urlEdit->setText(s.value("url", "http://127.0.0.1:8188").toString());
    m_workflowEdit->setText(s.value("workflow").toString());
    m_promptsEdit->setText(s.value("prompts").toString());
    m_positivePathEdit->setText(s.value("positivePath", "6.inputs.text").toString());
    m_negativePathEdit->setText(s.value("negativePath", "7.inputs.text").toString());
    m_seedPathEdit->setText(s.value("seedPath", "3.inputs.seed").toString());
    m_outputEdit->setText(s.value("outputDirectory").toString());
}
void MainWindow::saveSettings()
{
    QSettings s("ComfyUIQueueManager", "Qt6");
    s.setValue("url", m_urlEdit->text());
    s.setValue("workflow", m_workflowEdit->text());
    s.setValue("prompts", m_promptsEdit->text());
    s.setValue("positivePath", m_positivePathEdit->text());
    s.setValue("negativePath", m_negativePathEdit->text());
    s.setValue("seedPath", m_seedPathEdit->text());
    s.setValue("outputDirectory",m_outputEdit->text().trimmed());
}
void MainWindow::chooseWorkflow()
{
    auto f = QFileDialog::getOpenFileName(this, "Выберите workflow API JSON", {}, "JSON (*.json);;Все файлы (*)");
    if (!f.isEmpty())
        m_workflowEdit->setText(f);
}
void MainWindow::choosePrompts()
{
    auto f = QFileDialog::getOpenFileName(this, "Выберите prompts.txt", {}, "Text (*.txt);;Все файлы (*)");
    if (!f.isEmpty())
        m_promptsEdit->setText(f);
}
void MainWindow::loadJobs()
{
    QFile f(m_promptsEdit->text().trimmed());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, "Ошибка", f.errorString());
        return;
    }
    m_jobs.clear();
    m_table->setRowCount(0);
    QTextStream st(&f);
    int n = 1;
    while (!st.atEnd())
    {
        QString line = st.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#'))
            continue;
        auto p = line.split('|');
        Job j;
        j.index = n++;
        j.positive = p.value(0).trimmed();
        j.negative = p.value(1).trimmed();
        j.seed = p.value(2).trimmed();
        if (!j.positive.isEmpty())
            m_jobs.append(j);
    }
    m_table->setRowCount(m_jobs.size());
    for (int i = 0; i < m_jobs.size(); ++i)
    {
        auto &j = m_jobs[i];
        auto *previewItem = new QTableWidgetItem();
        previewItem->setTextAlignment(Qt::AlignCenter);
        setRow(i, 0, QString::number(j.index));
        setRow(i, 1, j.status);
        setRow(i, 2, j.positive);
        setRow(i, 4, j.seed);
        setRow(i, 5, {});
        m_table->setItem(i,3,previewItem);
    }
    m_progress->setRange(0, m_jobs.size());
    m_progress->setValue(0);
    m_log->appendPlainText(QString("Загружено заданий: %1").arg(m_jobs.size()));
}
void MainWindow::checkConnection()
{
    auto *t = new QThread(this);
    auto *w = new QObject;
    w->moveToThread(t);
    const QString url = m_urlEdit->text().trimmed();
    connect(t, &QThread::started, w, [this, t, url]()
            {ComfyApi api(url);QString e;bool ok=api.checkConnection(&e);QMetaObject::invokeMethod(this,[this,ok,e](){m_log->appendPlainText(ok?"✓ ComfyUI доступен.":"✗ ComfyUI недоступен: "+e);},Qt::QueuedConnection);QMetaObject::invokeMethod(t,"quit",Qt::QueuedConnection); });
    connect(t, &QThread::finished, w, &QObject::deleteLater);
    connect(t, &QThread::finished, t, &QThread::deleteLater);
    t->start();
}
void MainWindow::startQueue()
{
    if (m_jobs.isEmpty())
    {
        QMessageBox::warning(this, "Queue Manager", "Сначала загрузите задания.");
        return;
    }
    if (m_workflowEdit->text().trimmed().isEmpty())
    {
        QMessageBox::warning(this, "Queue Manager", "Укажите workflow API JSON.");
        return;
    }
    saveSettings();
    setRunning(true);
    m_workerThread = new QThread(this);
    auto *m = new QueueManager;
    m_worker = m;
    m->moveToThread(m_workerThread);
    connect(m_workerThread, &QThread::finished, m, &QObject::deleteLater);
    connect(m, &QueueManager::jobStatusChanged, this, &MainWindow::onJobStatusChanged, Qt::QueuedConnection);
    connect(m, &QueueManager::jobPromptIdChanged, this, &MainWindow::onJobPromptIdChanged, Qt::QueuedConnection);
    connect(m, &QueueManager::previewReady, this, &MainWindow::onPreviewReady, Qt::QueuedConnection);
    connect(m, &QueueManager::progressChanged, this, &MainWindow::onProgressChanged, Qt::QueuedConnection);
    connect(m, &QueueManager::logMessage, this, &MainWindow::onLogMessage, Qt::QueuedConnection);
    connect(m, &QueueManager::finished, this, &MainWindow::onQueueFinished, Qt::QueuedConnection);
    connect(m, &QueueManager::stopped, this, &MainWindow::onQueueStopped, Qt::QueuedConnection);
    connect(m_workerThread, &QThread::started, m, [this, m]()
            {
                m->start(
                            m_jobs,
                            m_urlEdit->text().trimmed(),
                            m_workflowEdit->text().trimmed(),
                            m_positivePathEdit->text().trimmed(),
                            m_negativePathEdit->text().trimmed(),
                            m_seedPathEdit->text().trimmed(),
                            outputDirectory());
            }
    );
    connect(m_workerThread, &QThread::finished, this, [this]()
            {
                m_workerThread->deleteLater();
                m_workerThread = nullptr;
                m_worker=nullptr;
            }
    );
    m_workerThread->start();
}
void MainWindow::stopQueue()
{
    if (auto *m = qobject_cast<QueueManager *>(m_worker))
    {
        m->stop();
        m_log->appendPlainText("Запрошена остановка очереди...");
    }
}
void MainWindow::clearQueue()
{
    if (QMessageBox::question(this, "Очистить очередь", "Очистить очередь ComfyUI?", QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;
    auto *t = new QThread(this);
    auto *w = new QObject;
    w->moveToThread(t);
    const QString url = m_urlEdit->text().trimmed();
    connect(t, &QThread::started, w, [this, t, url]()
            {ComfyApi api(url);QString e;bool ok=api.clearQueue(&e);QMetaObject::invokeMethod(this,[this,ok,e](){m_log->appendPlainText(ok?"✓ Очередь ComfyUI очищена.":"✗ Ошибка очистки: "+e);},Qt::QueuedConnection);QMetaObject::invokeMethod(t,"quit",Qt::QueuedConnection); });
    connect(t, &QThread::finished, w, &QObject::deleteLater);
    connect(t, &QThread::finished, t, &QThread::deleteLater);
    t->start();
}
void MainWindow::onJobStatusChanged(int i, const QString &s)
{
    if (i >= 1 && i <= m_table->rowCount())
        setRow(i - 1, 1, s);
}
void MainWindow::onJobPromptIdChanged(int i, const QString &s)
{
    if (i >= 1 && i <= m_table->rowCount())
        setRow(i - 1, 5, s);
}
void MainWindow::onPreviewReady(int i, const QImage &im, const QString &path)
{
    if (i < 1 || i > m_jobs.size()) {
        return;
    }

    m_jobs[i - 1].preview = im;
    m_jobs[i - 1].imagePath = path;

    setRowPreview(i - 1, im);
}
void MainWindow::onProgressChanged(int d, int t)
{
    m_progress->setRange(0, t);
    m_progress->setValue(d);
}
void MainWindow::onLogMessage(const QString &s) { m_log->appendPlainText(s); }
void MainWindow::onQueueFinished()
{
    m_log->appendPlainText("✓ Очередь завершена.");
    setRunning(false);
    if (m_workerThread)
        m_workerThread->quit();
}
void MainWindow::onQueueStopped()
{
    m_log->appendPlainText("■ Очередь остановлена.");
    setRunning(false);
    if (m_workerThread)
        m_workerThread->quit();
}

void MainWindow::chooseOutputDirectory()
{
    QString start = outputDirectory();

    if (start.isEmpty())
        start = QDir::homePath();

    const QString dir =
        QFileDialog::getExistingDirectory(
            this,
            "Выберите папку output ComfyUI",
            start
            );

    if (!dir.isEmpty()) {
        m_outputEdit->setText(QDir::cleanPath(dir));
        saveSettings();
    }
}

void MainWindow::openOutputFolder()
{
    const QString dir = outputDirectory();

    if (dir.isEmpty() || !QDir(dir).exists()) {
        QMessageBox::warning(
            this,
            "Queue Manager",
            "Укажите существующую папку output ComfyUI."
            );
        return;
    }

    QDesktopServices::openUrl(
        QUrl::fromLocalFile(dir)
        );
}

QString MainWindow::outputDirectory() const
{
    return QDir::cleanPath(m_outputEdit->text().trimmed());
}
void MainWindow::openSelectedPreview()
{
    const int row = m_table->currentRow();

    if (row < 0 || row >= m_jobs.size())
        return;

    const QString path = m_jobs.at(row).imagePath;

    if (path.isEmpty() || !QFileInfo::exists(path)) {
        QMessageBox::warning(
            this,
            "Queue Manager",
            "Оригинальный файл изображения не найден."
            );
        return;
    }

    QDesktopServices::openUrl(
        QUrl::fromLocalFile(path)
        );
}
void MainWindow::setRunning(bool r)
{
    m_running = r;
    m_startButton->setEnabled(!r);
    m_stopButton->setEnabled(r);
    m_loadButton->setEnabled(!r);
    m_checkButton->setEnabled(!r);
}
void MainWindow::setRow(int r, int c, const QString &s)
{
    if (!m_table->item(r, c))
        m_table->setItem(r, c, new QTableWidgetItem);
    m_table->item(r, c)->setText(s);
}

void MainWindow::setRowPreview(int row, const QImage &image)
{
    if (row < 0 || row >= m_table->rowCount())
        return;

    if (image.isNull())
        return;

    auto *label = new QLabel(m_table);

    label->setAlignment(Qt::AlignCenter);
    label->setMinimumSize(130, 90);
    label->setMaximumSize(130, 90);

    QPixmap pixmap = QPixmap::fromImage(image);

    pixmap = pixmap.scaled(
        120,
        85,
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation
        );

    label->setPixmap(pixmap);

    m_table->setCellWidget(row, 3, label);

    // Высота строки
    if (m_table->rowHeight(row) < 95)
        m_table->setRowHeight(row, 95);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if(m_running == true)
    {
        event->ignore();
        auto result = QMessageBox::question(
            this,
            "Подтверждение выхода",
            "Еще выполняется очередь заданий.\nВы уверены, что хотите закрыть приложение?",
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No // Кнопка по умолчанию
            );
        if (result == QMessageBox::Yes) {
            event->accept();
        }
        else return;
    }
    else
        event->accept();
}