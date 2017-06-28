#include "telemetry.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    Telemetry w;
    w.show();

    return a.exec();
}
