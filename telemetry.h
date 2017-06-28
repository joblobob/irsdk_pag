#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <QMainWindow>
#include <QMap>
#include "../irsdk_client.h"


namespace Ui {
class Telemetry;
}

class Telemetry : public QMainWindow
{
    Q_OBJECT

public:
    explicit Telemetry(QWidget *parent = 0);
    ~Telemetry();

private slots:
    void on_m_btnStart_clicked();

private:
    Ui::Telemetry *ui;
    bool m_isStarted;

    QMap<int, QVariant> m_map;

    QString getSessionVar(QString name);

    void run();
};

#endif // TELEMETRY_H
