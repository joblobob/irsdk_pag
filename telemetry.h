#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <QMainWindow>
#include <QMap>
#include <QTime>
#include <QTableWidgetItem>
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

    QMap<int, float> m_mapDist;
    QMap<int, qint64> m_mapDistTimeStamp;
    QMap<int, float> m_mapLapTime;
    QMap<int, float> m_mapLapSpeed;
    QMap<int, float> m_mapLapTimeDelta1;

    float m_trackLength;

    QString getSessionVar(const QString& name);

    QTableWidgetItem* newItem(const QString& name, int type = 0);

    void calculateLapTime(int idx, float dist);

    void run();
};

#endif // TELEMETRY_H
