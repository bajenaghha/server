#ifndef SERVER_H
#define SERVER_H

#include <QObject>
#include<QWebSocketServer>
#include<QWebSocket>
#include<QHash>
class Server : public QObject
{
    Q_OBJECT
public:
    explicit Server(QObject *parent = 0);
    void startServers(qint16 cmdSrvPort,qint16 dataSrvPort);

signals:

public slots:
    void onCmdServerNewConnection();
    void onDataServerNewConnection();
    void onCmdServerClientBinaryMessageReceived(QByteArray);
    void onDataServerBinaryMessageReceived(QByteArray);
    void onDataServerClientTextMessageReceived(QString);
    void onCmdServerClientDisconnected();

private:
    QWebSocketServer*commandServer;
    QWebSocketServer*dataServer;

    QHash<QString,QWebSocket*> clNameToCmdSocket;
    QHash<QWebSocket*,QString> clCmdSocketToName;

    QHash<QString,QWebSocket*> clNameToDataSocket;
    QHash<QWebSocket*,QString> clDataSocketToName;

    QHash<QString,QString> peers;
    void authClient(QWebSocket*client,QString userName);
    void joinClient(QWebSocket*client,QString peerUserName);
    void setKeepAliveTimedOut(int timedOut,int packetCount,int interval);

    bool isUserAuth(QString userName);
    bool isUserAuth(QWebSocket*websocket);
};

#endif // SERVER_H
