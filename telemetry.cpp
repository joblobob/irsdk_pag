#include "telemetry.h"
#include "ui_telemetry.h"

#include "../irsdk_client.h"
#include "../irsdk_defines.h"

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


irsdkCVar g_ClassPos("CarIdxClassPosition");
irsdkCVar g_EstTime("CarIdxEstTime");
irsdkCVar g_F2Time("CarIdxF2Time");
irsdkCVar g_Gear("CarIdxGear");
irsdkCVar g_LapNo("CarIdxLap");
irsdkCVar g_lapDist("CarIdxLapDistPct");
irsdkCVar g_OnPitRoad("CarIdxOnPitRoad");
irsdkCVar g_CarPos("CarIdxPosition");
irsdkCVar g_TrackSurface("CarIdxTrackSurface");
/*irsdk_TrkLoc
irsdk_NotInWorld
-1
irsdk_OffTrack
0
irsdk_InPitStall
1
irsdk_AproachingPits
2
irsdk_OnTrack
3*/

Telemetry::Telemetry(QWidget *parent) :
    QMainWindow(parent), m_isStarted(false),
    ui(new Ui::Telemetry)
{
    ui->setupUi(this);
    setWindowFlags(Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
    setAttribute( Qt::WA_TranslucentBackground, true );
    setGeometry(0,0,1500,400);

    ui->m_tblTimes->setHorizontalHeaderLabels(QStringList() << "Name" << "Pos" << "Time" << "FastestTime" << "LastTime" << "Speed" << "InPits");
    ui->m_tblTimes->horizontalHeader()->setStyleSheet("QHeaderView::section {color: white; background-color: rgba(0,0,0,0); font-weight: bold; font-size: 14px; height: 30}");
    ui->m_tblTimes->verticalHeader()->setStyleSheet("QHeaderView::section {color: white; background-color: rgba(0,0,0,0); font-weight: bold; font-size: 14px; width: 30}");
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

QString Telemetry::getSessionVar(QString name)
{
    char szRes[50];
    QString res = QString();
    if(irsdkClient::instance().getSessionStrVal(name.toLatin1(), szRes, 50) == 1) {
        res = QString::fromLatin1(szRes);
    }
    return res;
}

void Telemetry::run()
{

    // bump priority up so we get time from the sim
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

    // ask for 1ms timer so sleeps are more precise
    timeBeginPeriod(1);

    ui->m_tblTimes->setRowCount(60);
    ui->m_tblTimes->setColumnCount(7);

    while(m_isStarted)
    {
        if(irsdkClient::instance().waitForData(16))
        {
            ui->m_lblTitle->setText("Connected!");

            QString strIdx("DriverInfo:DriverCarIdx:");
            int sessionNo = g_SessionNum.getInt();
            QString strCarIdx("SessionInfo:Sessions:SessionNum:{%1}ResultsPositions:Position:{%2}CarIdx:");
            QString strUsername("DriverInfo:Drivers:CarIdx:{%1}UserName:");
            QString strTime("SessionInfo:Sessions:SessionNum:{%1}ResultsPositions:Position:{%2}:Time:");
            QString strFastestTime("SessionInfo:Sessions:SessionNum:{%1}ResultsPositions:Position:{%2}:FastestTime:");
            QString strLastTime("SessionInfo:Sessions:SessionNum:{%1}ResultsPositions:Position:{%2}:LastTime:");

            for(int i = 0; i < 50; i++)
            {
                QString strid = strCarIdx.arg(sessionNo).arg(i);
                QString idxIdx = getSessionVar(strid);

                QString name = strUsername.arg(i);
                QString idxName = getSessionVar(name);

                if(!idxName.isEmpty())
                {
                    QString time = strTime.arg(sessionNo).arg(i);
                    QString fastesttime = strFastestTime.arg(sessionNo).arg(i);
                    QString lasttime = strLastTime.arg(sessionNo).arg(i);

                    //float spd = g_carSpeed.getFloat(0) * 3.6;
                 //   if(spd > m_map[i].toFloat())
                //        m_map.insert(i, spd);    //insert speed

                    //ui->m_tblTimes->insertRow(i);
                    ui->m_tblTimes->setItem(i, 0, new QTableWidgetItem(idxName));
                    ui->m_tblTimes->setItem(i, 1, new QTableWidgetItem(g_ClassPos.getInt(i)));

                    irsdkCVar g_lapDist("CarIdxLapDistPct");
                    irsdkCVar g_OnPitRoad("CarIdxOnPitRoad");
                    irsdkCVar g_CarPos("CarIdxPosition");

                  //  QString idxTime = getSessionVar(time);
                 ///  QString idxFastestTime = getSessionVar(fastesttime);
//                    QString idxLastTime = getSessionVar(lasttime);

                    ui->m_tblTimes->setItem(i, 2, new QTableWidgetItem(QString::number(g_EstTime.getFloat(i))));
                    ui->m_tblTimes->setItem(i, 3, new QTableWidgetItem(QString::number(g_F2Time.getFloat(i))));
                   // ui->m_tblTimes->item(i, 3)->setTextColor(Qt::magenta);
                    ui->m_tblTimes->setItem(i, 4, new QTableWidgetItem(QString::number(g_lapDist.getFloat(i))));
                    ui->m_tblTimes->setItem(i, 5, new QTableWidgetItem(QString::number(g_F2Time.getFloat(i))));
                    ui->m_tblTimes->setItem(i, 6, new QTableWidgetItem(QString::number(g_OnPitRoad.getBool(i))));
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
