#pragma once
#include <QString>
#include <QImage>
struct Job {
    int index = 0;
    QString positive, negative, seed, promptId, status = "Ожидает", imagePath;
    QImage preview;
};
