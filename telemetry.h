#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <QMainWindow>
#include <QTimer>
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
    QString userName;
    bool isFriend;
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
        userName = "";
        isFriend = false;
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
        userName = copy.userName;
        isFriend = copy.isFriend;
    }
    bool operator==(const carData& rhs) const
    {
        return (ClassPos == rhs.ClassPos) &&
               (EstTime == rhs.EstTime) &&
                (F2Time == rhs.F2Time) &&
                (Gear == rhs.Gear) &&
                (LapNo == rhs.LapNo) &&
                (lapDist == rhs.lapDist) &&
                (OnPitRoad == rhs.OnPitRoad) &&
                (CarPos == rhs.CarPos) &&
                (TrackSurface == rhs.TrackSurface) &&
                (entry == rhs.entry) &&
                (userName == rhs.userName) &&
                (isFriend == rhs.isFriend);
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

    void on_m_btnSaveTrack_clicked();

    void on_m_btnLoadTrack_clicked();

    void on_m_btnResetView_clicked();

    void on_m_btnResetView_2_clicked();

    void on_m_btnLeftTransform_clicked();

    void on_m_btnRightTransform_clicked();

    void on_refreshUserNames();

    void on_refreshData();

private:
    Ui::Telemetry *ui;
    bool m_isStarted;

    QMap<int, carData> m_mapCarDataByIdx;
    QMap<int, QString> m_mapUserNameByIdx;
    QMap<int, carData> m_mapCarDataByPos;
    QMap<QString, carData> m_mapTeamCarDataByPos;

    QMap<int, qint64> m_mapDistSpdTimeStamp;
    QMap<int, qint64> m_mapDistTimeStamp;

    union lapTime{float currentTime; float previousTime;};

    QTimer m_userNamesTimer;

    QTimer m_dataTimer;

    //maps by entrys
    /*QMap<int, float> m_mapDistByEntry;
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
    QMap<int, int> m_mapLapTimeDeltaType3;*/

    //maps by pos
    QMap<int, int> m_mapLapTimeByPos;

    float m_trackLength;
    QString m_trackName;
    QString m_trackDisplayName;
    QString m_trackConfigName;
    QString m_trackPitName;

    QString getSessionVar(const QString& name);

    QTableWidgetItem* updateItem(int row, int column, const QString& name, int type = 0, bool isFriend = false);

    void calculateLapTime(int idx, float dist);
    void mapData();

    QGraphicsScene* m_scene;
    QGraphicsEllipseItem* m_pag;
    QGraphicsTextItem* m_pagText;

    bool m_isPathClosed;
    bool m_isFirstPitLap;
    bool m_isPathPitClosed;
    bool m_isFirstLap;
    int m_firstLapNo;
    bool m_startDrawing;
    bool m_trackIsLoaded;
    bool m_trackPitIsLoaded;
    double m_firstPitPct;
    QMap<int, QGraphicsEllipseItem*> m_mapCarEllipse;
    QMap<int, QGraphicsTextItem*> m_mapCarText;

    QGraphicsPathItem* m_trackLine;
    QPainterPath m_trackPath;

    QGraphicsPathItem* m_pitLine;
    QPainterPath m_pitPath;


    void run();

    void addCarToPainter(int pos);
    void calculateTrackLength();
    bool checkForTrackMap(double x, double y);
    bool checkForTrackPitMap(double x, double y);

    void drawCarsOnTrack();
    void drawPAGDriver(const carData& aCarData);
    void drawOtherDrivers(const QString& strName, const carData& aCarData);

    void saveTrackPath();
    void saveTrackPitPath();
    void loadTrackPath();
    void loadTrackPitPath();

    QString convertTime(float secs);

    bool isUserAFriend(int entryId);
};

#endif // TELEMETRY_H
