#include <QCoreApplication>
#include "myforwarder.h"
int main(int argc, char *argv[])
{
    //4th commit
    QCoreApplication a(argc, argv);
    MyForwarder myForwarder;
    myForwarder.startServer(15672);
    return a.exec();
}
