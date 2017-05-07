#include "myforwarder.h"
#include <QDebug>
#include <QString>
#include <QByteArray>
#include <QUdpSocket>

void MyForwarder::sendCommand()
{
    if (client1.first.isNull() || client2.first.isNull()) return;

    udpServer->writeDatagram("connect",client1.first,client1.second);
    udpServer->writeDatagram("connect",client2.first,client2.second);
}

MyForwarder::MyForwarder(QObject *parent) : QObject(parent)
{
    udpServer=new QUdpSocket(this);

    connect(udpServer,SIGNAL(readyRead()),this,SLOT(onReadyRead()));
}

void MyForwarder::startServer(qint16 mPort)
{
    bool res= udpServer->bind(QHostAddress::Any,mPort);
    if(!res)
    {
        qDebug()<<"Server Not started! :(";
        return;
    }
    qDebug()<<"Server Started on port:"<< mPort <<" :)";

}



void MyForwarder::onReadyRead()
{
    auto pending = udpServer->pendingDatagramSize();

    if(pending >0) {
        QByteArray datagram;
        datagram.resize(pending);
        QHostAddress sender;
        quint16 senderPort;
        udpServer->readDatagram(datagram.data(), datagram.size(),
                           &sender, &senderPort);

        if (client1.first.isNull() && QString(datagram)=="client1")
        {
            qDebug()<<"client1 is connected" << sender << senderPort;

            client1.first = sender;
            client1.second = senderPort;

            sendCommand();
        }
        else if (client2.first.isNull() && QString(datagram)=="client2")
        {
            qDebug()<<"client2 is connected" << sender << senderPort;

            client2.first = sender;
            client2.second = senderPort;

            sendCommand();
        }
        else
        {
            qDebug()<<">>" << sender << senderPort;
            //C2 to C1
            if ((client2.first == sender && client2.second == senderPort) && !client1.first.isNull())
            {
                qDebug()<<"Client2 to Client1: " << pending;
                udpServer->writeDatagram(datagram,client1.first,client1.second);
            }
            else if ((client1.first == sender && client1.second == senderPort) && !client2.first.isNull())
            {
                qDebug()<<"Client1 to Client2: " << pending;
                udpServer->writeDatagram(datagram,client2.first,client2.second);

            }
        }
    }

}
