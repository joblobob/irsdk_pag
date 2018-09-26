#include "telemetry.h"
#include "ui_telemetry.h"



#include <Windows.h>
#include <QFile>
#include <QDirIterator>
#include <QDataStream>

// for timeBeginPeriod
#pragma comment(lib, "Winmm")

// bool, driver is in the car and physics are running
// shut off motion if this is not true
irsdkCVar g_playerInCar("IsOnTrack");

// double, cars position in lat/lon decimal degrees
irsdkCVar g_carLat("LatAccel");
irsdkCVar g_carLon("LongAccel");
// float, cars altitude in meters relative to sea levels
irsdkCVar g_carYaw("Yaw");

// float, cars velocity in m/s
irsdkCVar g_carVelX("VelocityX");
irsdkCVar g_carVelY("VelocityY");
irsdkCVar g_carVelZ("VelocityZ");

irsdkCVar g_SessionNum("SessionNum");
irsdkCVar g_carSpeed("Speed");
irsdkCVar g_carCurTime("LapCurrentLapTime");
irsdkCVar g_carBestLapTime("LapBestLapTime");
irsdkCVar g_carLastLapTime("LapLastLapTime");

irsdkCVar g_carLapNo("Lap");

// variables available in real time for each car
irsdkCVar g_ClassPos("CarIdxClassPosition");
irsdkCVar g_EstTime("CarIdxEstTime");
irsdkCVar g_F2Time("CarIdxF2Time");
irsdkCVar g_Gear("CarIdxGear");
irsdkCVar g_LapNo("CarIdxLap");
irsdkCVar g_lapDist("CarIdxLapDistPct");
irsdkCVar g_OnPitRoad("CarIdxOnPitRoad");
irsdkCVar g_CarPos("CarIdxPosition");
irsdkCVar g_TrackSurface("CarIdxTrackSurface"); /*irsdk_TrkLoc irsdk_NotInWorld -1 irsdk_OffTrack 0 irsdk_InPitStall 1 irsdk_AproachingPits 2 irsdk_OnTrack 3*/

#define PEN_SIZE_CAR    300
#define PEN_SIZE_TRACK  200



Telemetry::Telemetry(QWidget *parent) :
    QMainWindow(parent), m_isStarted(false), m_trackLength(-1), m_trackName(""), m_trackDisplayName(""), m_trackConfigName(""), m_firstPitPct(-1.0), m_trackPitName(""), m_isPathClosed(false), m_isPathPitClosed(false), m_startDrawing(false), m_trackIsLoaded(false),m_trackPitIsLoaded(false),
    ui(new Ui::Telemetry)
{
    ui->setupUi(this);
    setWindowFlags(Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
    setAttribute( Qt::WA_TranslucentBackground, true );
    setGeometry(4300,0,1460,900);

    m_userNamesTimer.setInterval(10000);
    m_userNamesTimer.setSingleShot(false);
    connect(&m_userNamesTimer, SIGNAL(timeout()), this, SLOT(on_refreshUserNames()), Qt::UniqueConnection);

    m_dataTimer.setInterval(16);
    m_dataTimer.setSingleShot(false);
    connect(&m_dataTimer, SIGNAL(timeout()), this, SLOT(on_refreshData()), Qt::UniqueConnection);


    ui->m_tblTimes->setHorizontalHeaderLabels(QStringList() << "Name"  << "Class Pos" << "Split 1" << "Split 2" << "Split 3" << "Last Lap Time" << "Speed");
    ui->m_tblTimes->verticalHeader()->setDefaultAlignment(Qt::AlignHCenter);

    //init tables
    ui->m_tblTimes->setRowCount(65);
    ui->m_tblTimes->setColumnCount(7);
    for(int i = 0; i < ui->m_tblTimes->rowCount(); i++)
    {
        for(int j = 0; j < ui->m_tblTimes->columnCount(); j++)
            ui->m_tblTimes->setItem(i, j, new QTableWidgetItem(""));
    }

    m_isFirstLap = true;
    m_isFirstPitLap = true;
    m_firstLapNo = 0;
    m_scene = new QGraphicsScene();
    for(int i =0; i < 64; i++)
        m_mapCarEllipse[i] = NULL;

    m_pag = new QGraphicsEllipseItem(QRect(0,0,PEN_SIZE_CAR,PEN_SIZE_CAR));
    m_pag->setPen(QPen(Qt::red, 5));
    m_pag->setBrush(QBrush(Qt::green));
    m_pag->setPos(0.5,0.5);

    m_pagText = new QGraphicsTextItem("0", m_pag);
    m_pagText->setDefaultTextColor(Qt::red);
    m_pagText->setScale(40);
    m_pagText->setY(-6);

    m_trackLine = new QGraphicsPathItem();
    m_pitLine = new QGraphicsPathItem();

    m_trackLine->setPen(QPen(Qt::gray, PEN_SIZE_TRACK));
    m_pitLine->setPen(QPen(Qt::lightGray, 600));
    //m_trackLine->setBrush(QBrush(Qt::gray));
    m_trackLine->setPath(m_trackPath);
    m_pitLine->setPath(m_pitPath);
    m_scene->addItem(m_trackLine);
    m_scene->addItem(m_pitLine);
    m_scene->addItem(m_pag);

    ui->m_graph->setScene(m_scene);
}

Telemetry::~Telemetry()
{
    delete ui;
}

void Telemetry::on_m_btnStart_clicked()
{
    m_isStarted = !m_isStarted;

    if(m_isStarted) {
        ui->m_btnStart->setText("Stop");
        run();
    }
    else {
        timeEndPeriod(1);
        close();
    }
}

QString Telemetry::getSessionVar(const QString& name)
{
    char szRes[256];
    QString res = QString();
    if(irsdkClient::instance().getSessionStrVal(name.toLatin1(), szRes, 255) == 1) {
        res = QString::fromLatin1(szRes);
    }
    return res;
}

QTableWidgetItem* Telemetry::updateItem(int row, int column, const QString& name, int type, bool isFriend)
{
    QTableWidgetItem* item = ui->m_tblTimes->item(row, column);

    item->setText(name);
    item->setTextAlignment(Qt::AlignCenter);

    if(type == 1)   //good
    {
        item->setTextColor(Qt::green);
    }
    else if(type == 2) //best
    {
        item->setTextColor(Qt::magenta);
    }
    else if(type == 3) //pit
    {
        item->setTextColor(Qt::blue);
    }
    else
        item->setTextColor(Qt::white);

    return item;
}

void Telemetry::run()
{

    // bump priority up so we get time from the sim
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

    // ask for 1ms timer so sleeps are more precise
    timeBeginPeriod(1);

    //init vars
    m_userNamesTimer.start();

    m_isPathClosed = false;
    m_isPathPitClosed = false;
    m_firstLapNo = -1;
    m_startDrawing = false;
    m_trackIsLoaded = false;
    m_trackPitIsLoaded = false;

    //start data loop
    m_dataTimer.start();

    on_refreshUserNames();
}

void Telemetry::drawCarsOnTrack()
{
    int type = 0;
    for(const carData& aCarData : m_mapCarDataByIdx.values())
    {
        QString curDriverName = aCarData.userName;
        if(curDriverName != "") {
           // if(aCarData.ClassPos != -1)
         //   {
                if(aCarData.TrackSurface != -1)
                {
                    int pos = aCarData.CarPos-1;
                    if( pos != -1){

                        if(m_startDrawing == true)
                        {
                            if(curDriverName.contains("Giguère", Qt::CaseInsensitive))
                                drawPAGDriver(aCarData);
                            else
                                drawOtherDrivers(curDriverName, aCarData);
                        }

                        bool isShown = false;
                        if(ui->m_btnFriends->isChecked() == true) {
                            if(aCarData.isFriend == true)
                                isShown = true;
                        }
                        else {
                            isShown = true;
                        }

                        if(isShown == true) {
                            type = (aCarData.OnPitRoad == true ? 3 : 0);
                            updateItem(pos, 0, curDriverName, type, isShown);
                            updateItem(pos, 1, QString::number(aCarData.CarPos), type, isShown);
                            updateItem(pos, 2, QString::number(aCarData.ClassPos), type, isShown);
                            //updateItem(pos, 3, QString::number(aCarData.lapDist * 100.0), type, isShown);
                            updateItem(pos, 4, convertTime(aCarData.EstTime), type, isShown);
                            updateItem(pos, 5, convertTime(aCarData.F2Time), type, isShown);
                        }
                    }
                }
           // }
        }
    }
}

QString Telemetry::convertTime(float secs)
{
    QString time;
    if(secs < 60.0)
        time = QTime(0,0,0,0).addMSecs(secs*1000.0).toString("ss:zzz");
    else
        time = QTime(0,0,0,0).addMSecs(secs*1000.0).toString("mm:ss:zzz");
    return time;
}

void Telemetry::addCarToPainter(int pos)
{
    QGraphicsEllipseItem* item = new QGraphicsEllipseItem(QRect(0,0,600,600));
    item->setPen(QPen(Qt::red));
    item->setBrush(QBrush(Qt::darkCyan));
    if(m_mapCarEllipse[pos] == NULL) {
        m_mapCarEllipse[pos] = item;
        m_scene->addItem(m_mapCarEllipse[pos]);

        m_mapCarText[pos] = new QGraphicsTextItem(QString::number(pos), m_mapCarEllipse[pos]);
        m_mapCarText[pos]->setDefaultTextColor(Qt::red);
        m_mapCarText[pos]->setScale(40);
        m_mapCarText[pos]->setY(-6);

        m_scene->addItem(m_mapCarText[pos]);
    }
}


void Telemetry::drawPAGDriver(const carData& aCarData)
{
    double yaw = g_carYaw.getFloat();
    double carValX = g_carVelX.getFloat();
    double dx = carValX * sin(yaw);
    double dy = carValX * cos(yaw);
    double x = m_pag->x() + dx;
    double y = m_pag->y() + dy;

    if(m_isFirstLap == true)  //dessin
    {
        //trackData
        if(checkForTrackMap(x, y) == false) { //la piste existe on load

            m_trackPath.lineTo(m_pag->pos());
            m_trackLine->setPath(m_trackPath);

            m_pag->setPos(x, y);
        }
    }
    else if(m_isPathClosed == false)
    {
        m_trackPath.closeSubpath();
        m_trackLine->setPath(m_trackPath);
        m_isPathClosed = true;

        //saveTrackPath(); //save la nouvelle piste
    }
    else
    {
        //la track est loadé, on affiche selon le pct
        if(aCarData.OnPitRoad == false)
        {
            m_pag->setPos(m_trackPath.pointAtPercent(aCarData.lapDist));
        }
        else
        {
             m_pag->setPos(m_trackPath.pointAtPercent(aCarData.lapDist));
            //if pit track exists loadit
            //pitData
         /*   if(m_isFirstPitLap == true)  //dessin
            {
                if(checkForTrackPitMap(x, y) == false) {//la pit piste existe on load

                    //pit data
                    if(m_firstPitPct < 0.0) {
                        m_firstPitPct = aCarData.lapDist;
                        m_pitPath.moveTo(m_pag->pos());
                        m_pitLine->setPath(m_pitPath);
                    }
                    else
                    {
                        m_pitPath.lineTo(m_pag->pos());
                        m_pitLine->setPath(m_pitPath);

                        m_pag->setPos(x, y);
                    }
                }
            }
            else
            {
                QString pitPct = m_trackPitName.split("_").last();
                m_firstPitPct = pitPct.toDouble();

                m_pag->setPos(m_pitPath.pointAtPercent(abs(aCarData.lapDist - m_firstPitPct)));
            }*/

        }
        m_pagText->setPlainText(QString::number(aCarData.CarPos));
    }

}

void Telemetry::drawOtherDrivers(const QString& strName, const carData& aCarData)
{
    if(m_isPathClosed == true) {
        int pos = aCarData.CarPos;
        if(pos >= 0 && pos < 64)
        {
            if(m_mapCarEllipse[pos] == NULL)
                addCarToPainter(pos);

            Qt::GlobalColor color = Qt::darkCyan;
            if(strName.contains("Schaffner"    , Qt::CaseInsensitive))
                color = Qt::magenta;
            else if(strName.contains("Canuel"   ,Qt::CaseInsensitive))
                color = Qt::red;
            else if(strName.contains("Lalande"  ,Qt::CaseInsensitive))
                color = Qt::white;
            else if(strName.contains("Caouette" ,Qt::CaseInsensitive))
                color = Qt::blue;
            else if(strName.contains("cyr"      ,Qt::CaseInsensitive))
                color = Qt::black;
            else if(strName.contains("trudeau"  ,Qt::CaseInsensitive))
                color = Qt::black;


            if(m_mapCarEllipse[pos] != NULL) {
                m_mapCarEllipse[pos]->setBrush(QBrush(color));
                m_mapCarEllipse[pos]->setPos(m_trackPath.pointAtPercent(aCarData.lapDist));
                m_mapCarText[pos]->setPlainText(QString::number(pos));
            }
        }
    }
}

bool Telemetry::checkForTrackMap(double x, double y)
{
    //trackData
    if(QFile::exists(m_trackName + "_res.txt") == true) { //la piste existe on load
        loadTrackPath();

        m_pag->setPos(x, y);
        m_isFirstLap = false;
        m_isPathClosed = true;
        ui->m_graph->fitInView( m_scene->sceneRect(), Qt::KeepAspectRatio );
        checkForTrackPitMap(x, y);
        return true;
    }
    return false;
}

bool Telemetry::checkForTrackPitMap(double x, double y)
{
    //trackData
    if(QFile::exists(m_trackPitName) == true) { //la piste existe on load
        loadTrackPitPath();

        m_pag->setPos(x, y);
        m_isFirstPitLap = false;
        m_isPathPitClosed = true;
        return true;
    }
    return false;
}


void Telemetry::saveTrackPath()
{
    QFile aFile(m_trackName + "_res.txt");
    aFile.open(QIODevice::ReadWrite);
    QDataStream aStream(&aFile);
    aStream << m_trackPath;
    aFile.close();
}

void Telemetry::saveTrackPitPath()
{
    QFile aFile(m_trackName + "_pit_" + QString::number(m_firstPitPct));
    aFile.open(QIODevice::ReadWrite);
    QDataStream aStream(&aFile);
    aStream << m_pitPath;
    aFile.close();
}

void Telemetry::loadTrackPath()
{
    if(QFile::exists(m_trackName + "_res.txt") == true) {
        QFile aFile(m_trackName + "_res.txt");
        aFile.open(QIODevice::ReadOnly);
        QDataStream aStream(&aFile);
        aStream >> m_trackPath;
        m_trackLine->setPath(m_trackPath);
        aFile.close();
        m_trackIsLoaded = true;

        m_trackLine->setPen(QPen(Qt::gray, 600));
        m_pag->setRect(0,0, 600, 600);

        QGraphicsLineItem* line = new QGraphicsLineItem(-400,0,800,0, m_trackLine);
        line->setPen(QPen(Qt::red, 200));
        m_scene->addItem(line);
    }
    else
        qDebug("Track not found");
}

void Telemetry::loadTrackPitPath()
{
    if(QFile::exists(m_trackPitName) == true) {
        QFile aFile(m_trackPitName);
        aFile.open(QIODevice::ReadOnly);
        QDataStream aStream(&aFile);
        aStream >> m_pitPath;
        m_pitLine->setPath(m_pitPath);
        aFile.close();
        m_trackPitIsLoaded = true;

        m_pitLine->setPen(QPen(Qt::lightGray, 600));
    }
    else
        qDebug("Pit Track not found");
}

/*void Telemetry::calculateLapTime(int idx, float dist)
{
    if(m_mapDistTimeStamp.value(idx, -1) == -1)  //first time
    {
        m_mapDistByEntry[idx] = dist;
        m_mapDistTimeStamp[idx] = QDateTime::currentMSecsSinceEpoch();
        m_mapDistSpdTimeStamp[idx] = QDateTime::currentMSecsSinceEpoch();
        m_mapLapTimeBestDelta1[idx] = 9999.9999;
        m_mapLapTimeBestDelta2[idx] = 9999.9999;
        m_mapLapTimeBestDelta3[idx] = 9999.9999;
        m_mapLapTimeDeltaType1[idx] = 0;
        m_mapLapTimeDeltaType2[idx] = 0;
        m_mapLapTimeDeltaType3[idx] = 0;
    }
    else
    {
        if(dist > m_mapDistByEntry[idx]) { //during lap
            float diffDist = dist - m_mapDistByEntry[idx];
            float diffTime = QDateTime::currentMSecsSinceEpoch() - m_mapDistSpdTimeStamp[idx];
            if(diffTime > 10.0) {
                float spd = (double)(diffDist * m_trackLength) / (double)(diffTime / 1000.0 / 60.0 / 60.0); //km\h
                if(spd > 10.0 && spd < 400.0)
                {
                    m_mapLapSpeedByEntry[idx] = spd;
                    if(spd > m_mapFastestLapSpeedByEntry[idx])
                        m_mapFastestLapSpeedByEntry[idx] = spd;
                }
                m_mapDistSpdTimeStamp[idx] = QDateTime::currentMSecsSinceEpoch();
            }

        }
        else if(dist < m_mapDistByEntry[idx]){  //new lap

            m_mapLapTimeByEntry[idx].previousTime = m_mapLapTimeByEntry[idx].currentTime;
            m_mapLapTimeByEntry[idx].currentTime = (QDateTime::currentMSecsSinceEpoch() - m_mapDistTimeStamp[idx]) / 1000.0;

            //best lap
            if(m_mapLapTimeByEntry[idx].currentTime < m_mapLapBestTimeByEntry[idx]) {
                m_mapLapBestTimeByEntry[idx] = m_mapLapTimeByEntry[idx].currentTime;
                m_mapLapTimeType[idx] = 2;
            }

            //better then previous lap
            if(m_mapLapTimeByEntry[idx].currentTime < m_mapLapTimeByEntry[idx].previousTime)
                m_mapLapTimeType[idx] = 1;


            m_mapDistTimeStamp[idx] = QDateTime::currentMSecsSinceEpoch();
            m_mapLapTimeDeltaType1[idx] = 0;
            m_mapLapTimeDeltaType2[idx] = 0;
        }
        m_mapDistByEntry[idx] = dist;

        //split calculation
        if(dist < 0.333)
        {
            m_mapLapTimeDelta1[idx].currentTime = (QDateTime::currentMSecsSinceEpoch() - m_mapDistTimeStamp[idx]) / 1000.0;

            //best
            if(m_mapLapTimeBestDelta3[idx] < 9000.0 || m_mapLapTimeDelta3[idx].currentTime < m_mapLapTimeBestDelta3[idx]) {
                m_mapLapTimeBestDelta3[idx] = m_mapLapTimeDelta3[idx].currentTime;
                m_mapLapTimeDeltaType3[idx] = 2;
            }

            if(m_mapLapTimeDelta3[idx].currentTime < m_mapLapTimeDelta3[idx].previousTime)
                m_mapLapTimeDeltaType3[idx] = 1;
        }
        else if(dist > 0.333 && dist < 0.666)
        {
            m_mapLapTimeDeltaType3[idx] = 0;


            m_mapLapTimeDelta2[idx].currentTime = ((QDateTime::currentMSecsSinceEpoch() - m_mapDistTimeStamp[idx]) / 1000.0) - m_mapLapTimeDelta1[idx].currentTime;

            //best
            if(m_mapLapTimeBestDelta1[idx] < 9000.0 || m_mapLapTimeDelta1[idx].currentTime < m_mapLapTimeBestDelta1[idx]) {
                m_mapLapTimeBestDelta1[idx] = m_mapLapTimeDelta1[idx].currentTime;
                m_mapLapTimeDeltaType1[idx] = 2;
            }

            //better
            if(m_mapLapTimeDelta1[idx].currentTime < m_mapLapTimeDelta1[idx].previousTime)
                m_mapLapTimeDeltaType1[idx] = 1;

        }
        else if(dist > 0.666)
        {
            m_mapLapTimeDelta3[idx].currentTime = ((QDateTime::currentMSecsSinceEpoch() - m_mapDistTimeStamp[idx]) / 1000.0) - (m_mapLapTimeDelta1[idx].currentTime + m_mapLapTimeDelta2[idx].currentTime);

            //best
            if(m_mapLapTimeBestDelta2[idx] < 9000.0 || m_mapLapTimeDelta2[idx].currentTime < m_mapLapTimeBestDelta2[idx]) {
                m_mapLapTimeBestDelta2[idx] = m_mapLapTimeDelta2[idx].currentTime;
                m_mapLapTimeDeltaType2[idx] = 2;
            }

            //better
            if(m_mapLapTimeDelta2[idx].currentTime < m_mapLapTimeDelta2[idx].previousTime)
                m_mapLapTimeDeltaType2[idx] = 1;
        }
    }
}*/

void Telemetry::mapData()
{
    QMap<int, carData> tmpMapByEntry;

    //map first my entry position
    for(int i = 0; i < 64; i++)
    {
        tmpMapByEntry[i].ClassPos = g_ClassPos.getInt(i);
        tmpMapByEntry[i].EstTime = g_EstTime.getFloat(i);
        tmpMapByEntry[i].F2Time = g_F2Time.getFloat(i);
        tmpMapByEntry[i].Gear = g_Gear.getInt(i);
        tmpMapByEntry[i].LapNo = g_LapNo.getInt(i);
        tmpMapByEntry[i].lapDist = g_lapDist.getFloat(i);
        tmpMapByEntry[i].OnPitRoad = g_OnPitRoad.getBool(i);
        tmpMapByEntry[i].CarPos = g_CarPos.getInt(i);
        tmpMapByEntry[i].TrackSurface = g_TrackSurface.getInt(i);
        tmpMapByEntry[i].entry = i;
        tmpMapByEntry[i].userName = m_mapUserNameByIdx[i];
    }
    m_mapCarDataByIdx = tmpMapByEntry;

  /*  bool bFound = false;
    carData tmpData;
    //then by CarPosition
    for(int i = 0; i < 64; i++)
    {
        bFound = false;
        for(int j = 0; j < 64 && !bFound; j++)
        {
          //  QString strFriend = isUserAFriend(0);
          //  if(strFriend.isEmpty() == false)
          //      m_mapTeamCarDataByPos[strFriend] = m_mapCarDataByPos[i];

            if(tmpMapByEntry[j].CarPos == i+1){
                bFound = true;
                m_mapCarDataByPos[i] = tmpMapByEntry[j];

                m_mapCarDataByPos[i].isFriend = isUserAFriend(m_mapCarDataByPos[i].entry);
             //   if(strFriend.isEmpty() == false)
             //       m_mapTeamCarDataByPos[strFriend] = m_mapCarDataByPos[i];
            }
        }

        if(bFound == false && tmpMapByEntry[i].userName.contains("giguère", Qt::CaseInsensitive)) {   //exception pag tour 0
            bFound = true;
            m_mapCarDataByPos[i] = tmpMapByEntry[i];
        }
    }*/
}

bool Telemetry::isUserAFriend(int entryId)
{
    QString strUsername("DriverInfo:Drivers:CarIdx:{%1}UserName:");
    QString name = strUsername.arg(entryId);
    QString idxName = getSessionVar(name);
    if(idxName.isEmpty() == false) {
        if((idxName.contains("Giguère"  , Qt::CaseInsensitive) ||
            idxName.contains("Schaffner"    , Qt::CaseInsensitive) ||
            idxName.contains("Canuel"   ,Qt::CaseInsensitive) ||
            idxName.contains("Lalande"  ,Qt::CaseInsensitive) ||
            idxName.contains("Caouette"     ,Qt::CaseInsensitive) ||
            idxName.contains("cyr"          ,Qt::CaseInsensitive)))
        {
            return true;
        }
    }
    return false;
}

void Telemetry::on_m_btnFriends_clicked(bool checked)
{
    if(checked)
        ui->m_btnFriends->setText("Friends Only");
    else
        ui->m_btnFriends->setText("Display All");
}

/*
    reset la longeur de la track
*/
void Telemetry::calculateTrackLength()
{
    //calculer une seule fois
    if(m_trackLength <= 0.0000000) {
        //le nom
        QString trackName = getSessionVar("WeekendInfo:TrackName:");
        QString trackDisplayName = getSessionVar("WeekendInfo:TrackDisplayName:");
        QString trackConfigName = getSessionVar("WeekendInfo:TrackConfigName:");

        m_trackDisplayName = trackDisplayName;
        m_trackConfigName = trackConfigName;

        m_trackName = trackName;

        //La longeur
        QString length = getSessionVar("WeekendInfo:TrackLength:");
        if(!length.isEmpty())
            m_trackLength = length.split(" ").first().toFloat();

        //le fichier des pits
        QDirIterator aDir("", QStringList() << m_trackName + "_pit*", QDir::Files, 0);
        if(aDir.hasNext()){
            m_trackPitName = aDir.next();
        }

        //le fichier de piste
        if(QFile::exists(m_trackName + "_res.txt") == true) {
             ui->m_lblTitle->setText(m_trackDisplayName);
             m_startDrawing = true;
             m_isFirstLap = true;
        }
    }
}


void Telemetry::on_m_btnSaveTrack_clicked()
{
    saveTrackPath();
}

void Telemetry::on_m_btnLoadTrack_clicked()
{
    saveTrackPitPath();
}

void Telemetry::on_m_btnResetView_clicked()
{
  //  ui->m_graph->scale(0.9, 0.9);
    ui->m_graph->fitInView( m_scene->sceneRect(), Qt::KeepAspectRatio );
}

void Telemetry::on_m_btnResetView_2_clicked()
{
    ui->m_graph->scale(1.1, 1.1);
}

void Telemetry::on_m_btnLeftTransform_clicked()
{
    m_pitLine->moveBy(-10,0);
}

void Telemetry::on_m_btnRightTransform_clicked()
{
    m_pitLine->moveBy(10,0);
}

void Telemetry::on_refreshUserNames()
{
  //  if(m_trackIsLoaded == true)
 //   {
        QString strUsername("DriverInfo:Drivers:CarIdx:{%1}UserName:");
        //map first my entry position
        for(int i = 0; i < 64; i++)
        {
            m_mapUserNameByIdx[i] = getSessionVar(strUsername.arg(i).toLatin1());
        }
 //   }
}

void Telemetry::on_refreshData()
{
    int lapNo = 0;
    if(irsdkClient::instance().waitForData(16))
    {
        if(ui->m_lblTitle->text().contains("Not"))
            ui->m_lblTitle->setText("Connected!");

        mapData();  //update internal map data

        lapNo = g_carLapNo.getInt();

        if(m_trackIsLoaded == false)
        {
            //first lap
            if(m_firstLapNo == -1) {
                m_firstLapNo = lapNo;
                m_isFirstLap = false;
            }

            if(lapNo == m_firstLapNo+1) {
                m_isFirstLap = true;
                m_startDrawing = true;
            }
            else if(lapNo == m_firstLapNo+2)
                m_isFirstLap = false;

            //track length
            calculateTrackLength();
        }

        drawCarsOnTrack();
    }
    else {
        ui->m_lblTitle->setText("Not!!");
    }

        //qApp->processEvents();

}
