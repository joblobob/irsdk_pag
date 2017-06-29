#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <QMainWindow>
#include <QMap>
#include <QTime>
#include <QTableWidgetItem>
#include "../irsdk_client.h"
#include "../irsdk_defines.h"


namespace Ui {
class Telemetry;
}

struct carData {
    int ClassPos;
    float EstTime;
    float F2Time;
    int Gear;
    int LapNo;
    float lapDist;
    bool OnPitRoad;
    int CarPos;
    int TrackSurface;
    int entry;

    carData(const carData& copy){
        ClassPos = copy.ClassPos;
        EstTime = copy.EstTime;
        F2Time = copy.F2Time;
        Gear = copy.Gear;
        LapNo = copy.LapNo;
        lapDist = copy.lapDist;
        OnPitRoad = copy.OnPitRoad;
        CarPos = copy.CarPos;
        TrackSurface = copy.TrackSurface;
        entry = copy.entry;
    }
};

class Telemetry : public QMainWindow
{
    Q_OBJECT

public:
    explicit Telemetry(QWidget *parent = 0);
    ~Telemetry();

private slots:
    void on_m_btnStart_clicked();

    void on_m_btnFriends_clicked(bool checked);

private:
    Ui::Telemetry *ui;
    bool m_isStarted;

    QMap<int, carData> m_mapCarDataByPos;

    QMap<int, qint64> m_mapDistSpdTimeStamp;
    QMap<int, qint64> m_mapDistTimeStamp;

    //maps by entrys
    QMap<int, float> m_mapDistByEntry;
    QMap<int, float> m_mapLapTimeByEntry;
    QMap<int, float> m_mapLapSpeedByEntry;
    QMap<int, float> m_mapFastestLapSpeedByEntry;
    QMap<int, float> m_mapLapTimeDelta1;

    //maps by pos
    QMap<int, int> m_mapLapTimeByPos;

    float m_trackLength;

    QString getSessionVar(const QString& name);

    QTableWidgetItem* newItem(const QString& name, int type = 0);

    void calculateLapTime(int idx, float dist);
    void mapData();

    void run();
};

#endif // TELEMETRY_H
