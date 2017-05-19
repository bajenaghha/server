#include <QCoreApplication>
#include"server.h"
int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    Server s;
    s.startServers(9595,9596);
    return a.exec();
}
