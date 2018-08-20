#include "telemetry.h"
#include "ui_telemetry.h"



#include <windows.h>
#include <QFile>
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
#define USE_PLOT 1

Telemetry::Telemetry(QWidget *parent) :
    QMainWindow(parent), m_isStarted(false), m_trackLength(-1), m_trackName(""),
    ui(new Ui::Telemetry)
{
    ui->setupUi(this);
    setWindowFlags(Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
    setAttribute( Qt::WA_TranslucentBackground, true );
    setGeometry(4300,0,1460,900);

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
  //  connect(ui->m_graph->scene()->mouseMoveEvent(),SIGNAL())

#if USE_PLOT==1
    m_isFirstLap = true;
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
    m_pagText->setScale(2);
    m_pagText->setY(-6);


    m_trackLine = new QGraphicsPathItem();

    m_trackLine->setPen(QPen(Qt::gray, PEN_SIZE_TRACK));
    //m_trackLine->setBrush(QBrush(Qt::gray));
    m_trackLine->setPath(m_trackPath);
    m_scene->addItem(m_trackLine);
    m_scene->addItem(m_pag);

    ui->m_graph->setScene(m_scene);
#endif
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
    char szRes[255];
    QString res = QString();
    if(irsdkClient::instance().getSessionStrVal(name.toLatin1(), szRes, 255) == 1) {
        res = QString::fromLatin1(szRes);
    }
    return res;
}

QTableWidgetItem* Telemetry::newItem(int row, int column, const QString& name, int type, bool isFriend)
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
    QString strIdx("DriverInfo:DriverCarIdx:");
    QString strCarIdx("SessionInfo:Sessions:SessionNum:{%1}ResultsPositions:Position:{%2}CarIdx:");
    QString strUsername("DriverInfo:Drivers:CarIdx:{%1}UserName:");
    QString strid;
    QString idxIdx;
    int sessionNo = 0;
    bool ok = true;
    bool isFriend = false;
    QString name;


    float yaw = 0.0;
    float dx = 0.0;
    float dy = 0.0;
    int x = 0;
    int y = 0;

    int lapNo = 0;
    m_isPathClosed = false;
    m_firstLapNo =-1 ;
    m_startDrawing = false;
    m_trackIsLoaded = false;

    while(m_isStarted)
    {
        if(irsdkClient::instance().waitForData(16))
        {
            ui->m_lblTitle->setText("Connected!");

            mapData();  //update internal map data

            sessionNo = g_SessionNum.getInt();
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
                if(m_trackLength <= 0.0000000) {
                    calculateTrackLength();
                    if(QFile::exists(m_trackName + "_res.txt") == true) {
                         m_startDrawing = true;
                         m_isFirstLap = true;
                    }
                }
            }

            ok = true;
            isFriend = false;
            if(m_startDrawing == true)
                drawCarsOnTrack();
        }
        else {
            ui->m_lblTitle->setText("Not!!");
        }

        qApp->processEvents();
    }

    ui->m_lblTitle->setText("Stopped!!");
}

void Telemetry::drawCarsOnTrack()
{
    int type = 0;
    for(const carData& aCarData : m_mapCarDataByIdx.values())
    {
        QString curDriverName = aCarData.userName;
        if(curDriverName != "") {
            if(aCarData.ClassPos != -1)
            {
                if(aCarData.TrackSurface != -1)
                {
                    if(curDriverName.contains("Giguère", Qt::CaseInsensitive))
                        drawPAGDriver(aCarData);
                    else
                        drawOtherDrivers(curDriverName, aCarData);

                    int pos = aCarData.CarPos-1;
                    if( pos == -1)
                        pos = 0;
              //      if(ui->m_tblTimes->isRowHidden(pos))
           //             ui->m_tblTimes->showRow(pos);

                    type = (aCarData.OnPitRoad == true ? 3 : 0);
                     newItem(pos, 0, curDriverName, type);
                     //newItem(pos, 2, QString::number(m_firstLapNo), type);
                     newItem(pos, 2, QString::number(ui->m_graph->transform().m11()), type);
                     newItem(pos, 1, QString::number(aCarData.ClassPos), type);
                     newItem(pos, 5, QString::number(aCarData.EstTime), type);
                   // ui->m_tblTimes->setItem(pos, 0, newItem(pos, 0, idxName, type));
                   // ui->m_tblTimes->setItem(pos, 2, newItem(pos, 2, QString::number(m_firstLapNo), type));
                  //  ui->m_tblTimes->setItem(pos, 1, newItem(pos, 1, QString::number(aCarData.ClassPos), type));
                  //  ui->m_tblTimes->setItem(pos, 5, newItem(pos, 5, QString::number(aCarData.EstTime), type));
                }
            }
        }
    }
}

void Telemetry::addCarToPainter(int pos)
{
    QGraphicsEllipseItem* item = new QGraphicsEllipseItem(QRect(0,0,PEN_SIZE_CAR,PEN_SIZE_CAR));
    item->setPen(QPen(Qt::red));
    item->setBrush(QBrush(Qt::darkCyan));
    if(m_mapCarEllipse[pos] != NULL) {
        m_mapCarEllipse[pos] = item;
        m_scene->addItem(m_mapCarEllipse[pos]);
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
        if(QFile::exists(m_trackName + "_res.txt") == true) {
            loadTrackPath();
            m_pag->setPos(x, y);
            m_isFirstLap = false;
            m_isPathClosed = true;
            ui->m_graph->fitInView( m_scene->sceneRect(), Qt::KeepAspectRatio );
        }
        else
        {
            m_trackPath.lineTo(m_pag->pos());
            m_trackLine->setPath(m_trackPath);

            //m_trackLine->setPen(QPen(Qt::gray, 100/*ui->m_graph->scene()->width() * 0.05*/));
           // m_pag->setRect(x,y, ui->m_graph->scene()->width() * 0.05, ui->m_graph->scene()->height() * 0.05);
            m_pag->setPos(x, y);
            ui->m_graph->fitInView( m_scene->sceneRect(), Qt::KeepAspectRatio );
        }
    }
    else if(m_isPathClosed == false)
    {
        m_trackPath.closeSubpath();
        m_trackLine->setPath(m_trackPath);
        m_isPathClosed = true;
        //ui->m_graph->fitInView( m_scene->sceneRect(), Qt::KeepAspectRatio );

        saveTrackPath();

    }
    else
    {
        m_pag->setPos(m_trackPath.pointAtPercent(aCarData.lapDist));
        m_pagText->setPlainText(QString::number(aCarData.CarPos));


    }

}

void Telemetry::drawOtherDrivers(const QString& strName, const carData& aCarData)
{
    if(m_isPathClosed == true) {
        int pos = aCarData.CarPos;
        if(m_mapCarEllipse[pos] == NULL)
            addCarToPainter(pos);

        Qt::GlobalColor color = Qt::blue;
        if(strName.contains("Schaffner"    , Qt::CaseInsensitive))
            color = Qt::magenta;
        else if(strName.contains("Canuel"   ,Qt::CaseInsensitive))
            color = Qt::red;
        else if(strName.contains("Lalande"  ,Qt::CaseInsensitive))
            color = Qt::white;
        else if(strName.contains("Caouette"     ,Qt::CaseInsensitive))
            color = Qt::blue;
        else if(strName.contains("cyr"          ,Qt::CaseInsensitive))
            color = Qt::black;

        m_mapCarEllipse[pos]->setPen(QPen(color, 30, Qt::SolidLine , Qt::RoundCap, Qt::RoundJoin));
        m_mapCarEllipse[pos]->setPos(m_trackPath.pointAtPercent(aCarData.lapDist));
        m_mapCarEllipse[pos]->setRect(0, 0, PEN_SIZE_CAR, PEN_SIZE_CAR);
    }
}

void Telemetry::saveTrackPath()
{
    QFile aFile(m_trackName + "_res.txt");
    aFile.open(QIODevice::ReadWrite);
    QDataStream aStream(&aFile);
    aStream << m_trackPath;
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
    }
    else
        qDebug("Track not found");
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
        QString strUsername("DriverInfo:Drivers:CarIdx:{%1}UserName:");
        QString idxName = getSessionVar(strUsername.arg(i));
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
        if(m_mapCarDataByIdx[i].userName != idxName)
            tmpMapByEntry[i].userName = idxName;
    }
    m_mapCarDataByIdx = tmpMapByEntry;

    bool bFound = false;
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

                QString strFriend = isUserAFriend(m_mapCarDataByPos[i].entry);
                if(strFriend.isEmpty() == false)
                    m_mapTeamCarDataByPos[strFriend] = m_mapCarDataByPos[i];
            }

        }
       // if(bFound == false)
       //     m_mapCarDataByPos[i] = tmpMapByEntry[i];
    }
}

QString Telemetry::isUserAFriend(int entryId)
{
    QString strUsername("DriverInfo:Drivers:CarIdx:{%1}UserName:");
    QString name = strUsername.arg(entryId);
    QString idxName = getSessionVar(name);
    QString retName = "";
    if(idxName.isEmpty() == false) {
        if((idxName.contains("Giguère"  , Qt::CaseInsensitive) ||
            idxName.contains("Schaffner"    , Qt::CaseInsensitive) ||
            idxName.contains("Canuel"   ,Qt::CaseInsensitive) ||
            idxName.contains("Lalande"  ,Qt::CaseInsensitive) ||
            idxName.contains("Caouette"     ,Qt::CaseInsensitive) ||
            idxName.contains("cyr"          ,Qt::CaseInsensitive)))
        {
            retName = idxName;
        }
    }
    return retName;
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
    QString trackName = getSessionVar("WeekendInfo:TrackName:");
    m_trackName = trackName;
    QString length = getSessionVar("WeekendInfo:TrackLength:");
    if(!length.isEmpty())
        m_trackLength = length.split(" ").first().toFloat();
}

/*
double Telemetry::getX(double lon, int width)
{
    // width is map width
    double x = fmod((width*(180+lon)/360), (width +(width/2)));

    return x;
}

double Telemetry::getY(double lat, int height, int width)
{
    // height and width are map height and width
    double PI = 3.14159265359;
    double latRad = lat*PI/180;

    // get y value
    double mercN = log(tan((PI/4)+(latRad/2)));
    double y     = (height/2)-(width*mercN/(2*PI));
    return y;
}
*/

void Telemetry::on_m_btnSaveTrack_clicked()
{
    saveTrackPath();
}

void Telemetry::on_m_btnLoadTrack_clicked()
{
    loadTrackPath();
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
