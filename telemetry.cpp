#include "telemetry.h"
#include "ui_telemetry.h"



#include <windows.h>

// for timeBeginPeriod
#pragma comment(lib, "Winmm")

// bool, driver is in the car and physics are running
// shut off motion if this is not true
irsdkCVar g_playerInCar("IsOnTrack");

// double, cars position in lat/lon decimal degrees
irsdkCVar g_carLat("Lat");
irsdkCVar g_carLon("Lon");
// float, cars altitude in meters relative to sea levels
irsdkCVar g_carAlt("Alt");

// float, cars velocity in m/s
irsdkCVar g_carVelX("VelocityX");
irsdkCVar g_carVelY("VelocityY");
irsdkCVar g_carVelZ("VelocityZ");

irsdkCVar g_SessionNum("SessionNum");
irsdkCVar g_carSpeed("Speed");
irsdkCVar g_carCurTime("LapCurrentLapTime");
irsdkCVar g_carBestLapTime("LapBestLapTime");
irsdkCVar g_carLastLapTime("LapLastLapTime");


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


Telemetry::Telemetry(QWidget *parent) :
    QMainWindow(parent), m_isStarted(false), m_trackLength(-1),
    ui(new Ui::Telemetry)
{
    ui->setupUi(this);
    setWindowFlags(Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
    setAttribute( Qt::WA_TranslucentBackground, true );
    setGeometry(0,0,1600,400);

    ui->m_tblTimes->setHorizontalHeaderLabels(QStringList() << "Name" << "Class Pos" << "Est Time" << "F2 Time" << "Lap %" << "Last Lap Time" << "InPits" << "Speed");
    ui->m_tblTimes->verticalHeader()->setDefaultAlignment(Qt::AlignHCenter);
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
    char szRes[100];
    QString res = QString();
    if(irsdkClient::instance().getSessionStrVal(name.toLatin1(), szRes, 100) == 1) {
        res = QString::fromLatin1(szRes);
    }
    return res;
}

QTableWidgetItem* Telemetry::newItem(const QString& name, int type)
{
    QTableWidgetItem* item = new QTableWidgetItem(name);
    item->setTextAlignment(Qt::AlignCenter);

    if(type == 1)   //good
    {
        item->setTextColor(Qt::green);
    }
    else if(type == 2) //best
    {
        item->setTextColor(Qt::magenta);
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

    ui->m_tblTimes->setRowCount(60);
    ui->m_tblTimes->setColumnCount(8);

    while(m_isStarted)
    {
        if(irsdkClient::instance().waitForData(16))
        {
            ui->m_lblTitle->setText("Connected!");

            mapData();  //update map data

            QString strIdx("DriverInfo:DriverCarIdx:");
            int sessionNo = g_SessionNum.getInt();
            QString strCarIdx("SessionInfo:Sessions:SessionNum:{%1}ResultsPositions:Position:{%2}CarIdx:");
            QString strUsername("DriverInfo:Drivers:CarIdx:{%1}UserName:");
          //  QString strTime("SessionInfo:Sessions:SessionNum:{%1}ResultsPositions:Position:{%2}:Time:");
          //  QString strFastestTime("SessionInfo:Sessions:SessionNum:{%1}ResultsPositions:Position:{%2}:FastestTime:");
         //   QString strLastTime("SessionInfo:Sessions:SessionNum:{%1}ResultsPositions:Position:{%2}:LastTime:");

            if(m_trackLength <= 0.0000000)
            {
                QString length = getSessionVar("WeekendInfo:TrackLength:");
                if(!length.isEmpty())
                    m_trackLength = length.split(" ").first().toFloat();
            }

            int nbCars = 50;
            if(ui->m_btnFriends->isChecked())
                nbCars = 3;

            for(int pos = 0; pos < nbCars; pos++)
            {
                QString strid = strCarIdx.arg(sessionNo).arg(pos);
                QString idxIdx = getSessionVar(strid);

                QString name = strUsername.arg(m_mapCarDataByPos[pos].entry);
                QString idxName = getSessionVar(name);

                if(!idxName.isEmpty())
                {
                    bool ok = true;
                    if(ui->m_btnFriends->isChecked()) {
                        if((idxName.contains("pier-antoine", Qt::CaseInsensitive) || idxName.contains("Simon Ro", Qt::CaseInsensitive) || idxName.contains("Alexandre Ca",Qt::CaseInsensitive)))
                            ok = false;
                        else
                            ok = true;
                    }

                    if(ok)
                    {
                       // QString time = strTime.arg(sessionNo).arg(i);
                    //    QString fastesttime = strFastestTime.arg(sessionNo).arg(i);
                    //    QString lasttime = strLastTime.arg(sessionNo).arg(i);
                        //  QString idxTime = getSessionVar(time);
                       //  QString idxFastestTime = getSessionVar(fastesttime);
                        // QString idxLastTime = getSessionVar(lasttime);

                        calculateLapTime(pos, m_mapCarDataByPos[pos].lapDist);

                        ui->m_tblTimes->setItem(pos, 0, newItem(idxName));
                        ui->m_tblTimes->setItem(pos, 1, newItem(QString::number(m_mapCarDataByPos[pos].ClassPos)));
                        ui->m_tblTimes->setItem(pos, 2, newItem(QString::number(m_mapCarDataByPos[pos].EstTime)));
                        ui->m_tblTimes->setItem(pos, 3, newItem(QString::number(m_mapCarDataByPos[pos].F2Time)));
                        ui->m_tblTimes->setItem(pos, 4, newItem(QString::number(m_mapDistByEntry[m_mapCarDataByPos[pos].entry])));
                        if(m_mapLapTimeByEntry[pos] > 0.0)
                            ui->m_tblTimes->setItem(pos, 5, newItem(QString::number(m_mapLapTimeByEntry[m_mapCarDataByPos[pos].entry])));
                        ui->m_tblTimes->setItem(pos, 6, newItem(QString::number(m_mapCarDataByPos[pos].OnPitRoad)));
                        if(m_mapFastestLapSpeedByEntry[pos] > 0.0)
                            ui->m_tblTimes->setItem(pos, 7, newItem(QString::number(m_mapFastestLapSpeedByEntry[m_mapCarDataByPos[pos].entry])));
                    }
                }
            }
        }
        else {
            ui->m_lblTitle->setText("Not!!");
        }

        qApp->processEvents();
    }

    ui->m_lblTitle->setText("Stopped!!");
}

void Telemetry::calculateLapTime(int idx, float dist)
{
    if(m_mapDistTimeStamp.value(idx, -1) == -1)  //first time
    {
        m_mapDistByEntry[idx] = dist;
        m_mapDistTimeStamp[idx] = QDateTime::currentMSecsSinceEpoch();
        m_mapDistSpdTimeStamp[idx] = QDateTime::currentMSecsSinceEpoch();
    }
    else
    {
        if(dist > m_mapDistByEntry[idx]) { //during lap

            float diffDist = dist - m_mapDistByEntry[idx];
            float diffTime = QDateTime::currentMSecsSinceEpoch() - m_mapDistSpdTimeStamp[idx];
            float spd = (diffDist * m_trackLength) / (diffTime / 1000.0 / 60.0 / 60.0); //km\h
            if(spd > 1.0 && spd < 500.0)
            {
                m_mapLapSpeedByEntry[idx] = spd;
                if(spd > m_mapFastestLapSpeedByEntry[idx])
                    m_mapFastestLapSpeedByEntry[idx] = spd;
            }
            m_mapDistSpdTimeStamp[idx] = QDateTime::currentMSecsSinceEpoch();

        }
        else if(dist < m_mapDistByEntry[idx]){  //new lap
            m_mapLapTimeByEntry[idx] = (QDateTime::currentMSecsSinceEpoch() - m_mapDistTimeStamp[idx]) / 1000.0;
            m_mapDistTimeStamp[idx] = QDateTime::currentMSecsSinceEpoch();
        }
        m_mapDistByEntry[idx] = dist;
    }
}

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
    }

    bool bFound = false;
    carData tmpData;
    //then by CarPosition
    for(int i = 0; i < 64; i++)
    {
        for(int j = 0; j < 64 && !bFound; j++)
        {
            if(tmpMapByEntry[j].CarPos == i){
                bFound = true;
                m_mapCarDataByPos[i] = tmpMapByEntry[j];
            }
        }
    }
}

void Telemetry::on_m_btnFriends_clicked(bool checked)
{
    if(checked)
        ui->m_btnFriends->setText("Friends Only");
    else
        ui->m_btnFriends->setText("Display All");
}
