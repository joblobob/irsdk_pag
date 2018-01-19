#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <QMainWindow>
#include <QMap>
#include <QTime>
#include <QTableWidgetItem>
#include <QGraphicsScene>
#include <QGraphicsEllipseItem>
#include <QGraphicsPathItem>
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
    carData(){
        ClassPos = -1;
        EstTime = 0.0;
        F2Time = 0.0;
        Gear = 0;
        LapNo = 0;
        lapDist = 0.0;
        OnPitRoad = true;
        CarPos = 0;
        TrackSurface = 0;
        entry = 0;
    }
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

    union lapTime{float currentTime; float previousTime;};

    //maps by entrys
    QMap<int, float> m_mapDistByEntry;
    QMap<int, lapTime> m_mapLapTimeByEntry;
    QMap<int, int> m_mapLapTimeType;
    QMap<int, float> m_mapLapBestTimeByEntry;
    QMap<int, float> m_mapLapSpeedByEntry;
    QMap<int, float> m_mapFastestLapSpeedByEntry;
    QMap<int, float> m_mapLapTimeBestDelta1;
    QMap<int, float> m_mapLapTimeBestDelta2;
    QMap<int, float> m_mapLapTimeBestDelta3;
    QMap<int, lapTime> m_mapLapTimeDelta1;
    QMap<int, lapTime> m_mapLapTimeDelta2;
    QMap<int, lapTime> m_mapLapTimeDelta3;
    QMap<int, int> m_mapLapTimeDeltaType1;
    QMap<int, int> m_mapLapTimeDeltaType2;
    QMap<int, int> m_mapLapTimeDeltaType3;

    //maps by pos
    QMap<int, int> m_mapLapTimeByPos;

    float m_trackLength;

    QString getSessionVar(const QString& name);

    QTableWidgetItem* newItem(int row, int column, const QString& name, int type = 0, bool isFriend = false);

    void calculateLapTime(int idx, float dist);
    void mapData();
    //double getX(double lon, int width);
    //double getY(double lat, int height, int width);
    QGraphicsScene* m_scene;
    QGraphicsEllipseItem* m_pag;
    QGraphicsTextItem* m_pagText;
    bool m_isPathClosed;
    bool m_isFirstLap;
    int m_firstLapNo;
    QMap<int, QGraphicsEllipseItem*> m_mapCarEllipse;

    QGraphicsPathItem* m_trackLine;
    QPainterPath m_trackPath;

    void run();

    void addCarToPainter(int pos);
};

#endif // TELEMETRY_H
