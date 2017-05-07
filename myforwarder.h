#ifndef MYFORWARDER_H
#define MYFORWARDER_H

#include <QObject>
#include <QUdpSocket>
#include <QPair>

class MyForwarder : public QObject
{
    Q_OBJECT
    void sendCommand();
public:
    explicit MyForwarder(QObject *parent = 0);
    void startServer(qint16 mPort);
signals:

public slots:
    void onReadyRead();
private:
    QUdpSocket* udpServer;

    QPair<QHostAddress,quint16> client1;
    QPair<QHostAddress,quint16> client2;
};

#endif // MYFORWARDER_H
