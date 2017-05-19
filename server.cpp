#include "server.h"
#include<QDebug>
#include<QJsonDocument>
#include<QJsonObject>
#include<QFuture>
#include<QtConcurrent>
#include<QByteArray>
#include<QJsonArray>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
Server::Server(QObject *parent) : QObject(parent)
{
    this->commandServer=new QWebSocketServer("bajenaghhaCommandServer",QWebSocketServer::SslMode::NonSecureMode,this);
    this->dataServer=new QWebSocketServer("bajenaghhaDataServer",QWebSocketServer::SslMode::NonSecureMode,this);
    connect(commandServer,SIGNAL(newConnection()),this,SLOT(onCmdServerNewConnection()));
    connect(dataServer,SIGNAL(newConnection()),this,SLOT(onDataServerNewConnection()));
}

void Server::startServers(qint16 cmdSrvPort, qint16 dataSrvPort)
{
    bool cmdStarted=commandServer->listen(QHostAddress::Any,cmdSrvPort);
    bool dataStarted=dataServer->listen(QHostAddress::Any,dataSrvPort);
    if(!(cmdStarted&&dataStarted))
    {

        qInfo()<<"Servers doenst start!!";
        return;
    }
    else
    {
        qInfo()<<"Servers  started ";
        qInfo()<<"Command server port:"<<cmdSrvPort;
        qInfo()<<"Data server port:"<<dataSrvPort;
    }
    qDebug()<<"socket desc:"<< commandServer->socketDescriptor();

    setKeepAliveTimedOut(10,3,2);
}

void Server::onCmdServerNewConnection()
{
    QWebSocket*newClient=commandServer->nextPendingConnection();
    connect(newClient,SIGNAL(binaryMessageReceived(QByteArray)),this,SLOT(onCmdServerClientBinaryMessageReceived(QByteArray)));
    connect(newClient,SIGNAL(disconnected()),this,SLOT(onCmdServerClientDisconnected()));

    qDebug()<<"cmdServer new Connection :"<<newClient->peerAddress()<<" port:"<<newClient->peerPort();

}

void Server::onDataServerNewConnection()
{
    qDebug()<<"dataServer new Connection";
    QWebSocket*newClient=dataServer->nextPendingConnection();

    connect(newClient,SIGNAL(textMessageReceived(QString)),this,SLOT(onDataServerClientTextMessageReceived(QString)));
    connect(newClient,SIGNAL(binaryMessageReceived(QByteArray)),this,SLOT(onDataServerBinaryMessageReceived(QByteArray)));



}

void Server::onCmdServerClientBinaryMessageReceived(QByteArray data)
{
    QWebSocket*client=qobject_cast<QWebSocket*>(sender());
    QJsonDocument jd=QJsonDocument::fromBinaryData(data);
    if(jd.isNull()||jd.isEmpty())
        return;
    QJsonObject jo=jd.object();
    QString requstType=  jo.value("requestType").toString();

    // auth command received
    if(requstType=="auth")
    {
        QJsonObject authObj=jo["auth"].toObject();
        QString  authUserName=authObj["userName"].toString();
        this->authClient(client,authUserName);
    }
    //join user
    else if(requstType=="joinPeer")
    {
        QJsonObject peerObj=jo["peer"].toObject();
        QString peerUserName=peerObj["userName"].toString();
        this->joinClient(client,peerUserName);
    }
}

void Server::onDataServerBinaryMessageReceived(QByteArray ba)
{
    qDebug()<<"onDataServerBinaryMessageReceived"<<ba.size();
    QWebSocket*mClient=qobject_cast<QWebSocket*>(sender());
    QString mClientUserName=clDataSocketToName[mClient];
    QString otherPeerUserName=peers[mClientUserName];
    QWebSocket*mOtherPeerSck=clNameToDataSocket[otherPeerUserName];
    mOtherPeerSck->sendBinaryMessage(ba);

    qDebug()<<"from "<<clDataSocketToName[mClient]<<" to"<<clDataSocketToName[mOtherPeerSck];

}

void Server::onDataServerClientTextMessageReceived(QString msg)
{
    QWebSocket*mClient=qobject_cast<QWebSocket*>(sender());
    qDebug()<<"UserName:"<<msg;




    // if user authnticated
    if(isUserAuth(msg))
    {
        this->clNameToDataSocket.insert(msg,mClient);
        this->clDataSocketToName.insert(mClient,msg);
        QString peerName=peers[msg];
        QWebSocket*otherWebSocket=clNameToDataSocket[peerName];

        /*

        {
         'isOK':true
         'responseType':'sendData'
         'sendData':{
                'success':true
            }


        }
        */

        QJsonObject sendDataObj;
        QJsonObject jObject;
        jObject["isOK"]=true;
        jObject["responseType"]="sendData";
        sendDataObj["success"]=true;
        jObject["sendData"]=sendDataObj;
        QJsonDocument jdoc=QJsonDocument(jObject);
        //if other user also connected to data socket
        if(otherWebSocket)
        {

            QWebSocket*firstPeersck=clNameToCmdSocket[msg];
            QWebSocket*secondPeerSck=clNameToCmdSocket[peerName];
            firstPeersck->sendBinaryMessage(jdoc.toBinaryData());
            secondPeerSck->sendBinaryMessage(jdoc.toBinaryData());
        }
    }
    else
    {

        mClient->abort();
    }

}

void Server::onCmdServerClientDisconnected()
{

    QWebSocket*mClient=qobject_cast<QWebSocket*>(sender());

    qDebug()<<"command server client disconnected:"<<mClient->localAddress()<<" port:"<<mClient->localPort();
    if(clCmdSocketToName.contains(mClient))
    {
        QString clientName  = clCmdSocketToName[mClient];
        QString otherClientName=peers[clientName];
        QWebSocket*otherPeerSck=clNameToCmdSocket[otherClientName];

        //remove from command server hashmap
        clNameToCmdSocket.remove(clientName);
        clCmdSocketToName.remove(mClient);
        clNameToCmdSocket.remove(otherClientName);
        clCmdSocketToName.remove(otherPeerSck);

        //remove form data server hashMap
        if(clNameToDataSocket[clientName])
            clNameToDataSocket[clientName]->close();
        if(clNameToDataSocket[otherClientName])
            clNameToDataSocket[otherClientName]->close();
        clDataSocketToName.remove(clNameToDataSocket[otherClientName]);
        clDataSocketToName.remove(clNameToDataSocket[clientName]);
        clNameToDataSocket.remove(clientName);
        clNameToDataSocket.remove(otherClientName);




        peers.remove(otherClientName);
        peers.remove(clientName);

        mClient->deleteLater();
        if(otherPeerSck)
        {
            otherPeerSck->close();
            otherPeerSck->deleteLater();
        }


        qDebug()<<clNameToCmdSocket.size();
        qDebug()<<clNameToDataSocket.size();
        qDebug()<<clDataSocketToName.size();
        qDebug()<<clCmdSocketToName.size();
        qDebug()<<peers.size();
    }
}



void Server::authClient(QWebSocket *client, QString userName)
{
    //if userName not entered
    if(userName.isEmpty())
    {

        QJsonObject authObject;
        QJsonObject jObject;
        jObject["isOK"]=true;
        jObject["responseType"]="auth";
        authObject["success"]=false;
        authObject["msg"]="username not entered";
        jObject["auth"]=authObject;

        QJsonDocument jdoc=QJsonDocument(jObject);

        client->sendBinaryMessage(jdoc.toBinaryData());

        return;
    }
    // user already exist
    if(clNameToCmdSocket.contains(userName))
    {
        QJsonObject authObject;
        QJsonObject jObject;
        jObject["isOK"]=true;
        jObject["responseType"]="auth";
        authObject["success"]=false;
        authObject["msg"]="username is already exist!";
        jObject["auth"]=authObject;

        QJsonDocument jdoc=QJsonDocument(jObject);
        client->sendBinaryMessage(jdoc.toBinaryData());

        return;

    }

    // current user(socket) already authenticated
    bool isWebSocketExist=clCmdSocketToName.contains(client);
    if(isWebSocketExist)
    {
        QJsonObject authObject;
        QJsonObject jObject;
        jObject["isOK"]=true;
        jObject["responseType"]="auth";
        authObject["success"]=false;
        authObject["msg"]="You have already authenticated!";
        jObject["auth"]=authObject;

        QJsonDocument jdoc=QJsonDocument(jObject);
        client->sendBinaryMessage(jdoc.toBinaryData());

        return;
    }
    clNameToCmdSocket.insert(userName,client);
    clCmdSocketToName.insert(client,userName);

    QJsonObject authObject;
    QJsonObject jObject;
    jObject["isOK"]=true;
    jObject["responseType"]="auth";
    authObject["success"]=true;
    jObject["auth"]=authObject;

    QJsonDocument jdoc=QJsonDocument(jObject);
    client->sendBinaryMessage(jdoc.toBinaryData());



    //send user list


    QJsonObject listUsers;
    listUsers["responseType"]="getUsers";
    QJsonArray userNames;
    for (QString mUserName:clCmdSocketToName)
    {
        userNames.append(mUserName);

    }
    QJsonObject getUsersObj;
    getUsersObj["success"]=true;
    getUsersObj["users"]=userNames;
    listUsers["getUsers"]=getUsersObj;
    QJsonDocument jdocm=QJsonDocument(listUsers);
    client->sendBinaryMessage(jdocm.toBinaryData());


}

void Server::joinClient(QWebSocket *client, QString peerUserName)
{
    //user not authenticated
    if(!isUserAuth(client))
    {
        QJsonObject peerObj;
        QJsonObject jObject;
        jObject["isOK"]=true;
        jObject["responseType"]="joinPeer";
        peerObj["success"]=false;
        peerObj["msg"]="You have not authenticated!";
        jObject["peer"]=peerObj;
        QJsonDocument jdoc=QJsonDocument(jObject);
        client->sendBinaryMessage(jdoc.toBinaryData());
        return;
    }

    bool isPeerExist=this->clNameToCmdSocket.contains(peerUserName);
    //peer not in server
    if(!isPeerExist)
    {
        QJsonObject peerObj;
        QJsonObject jObject;
        jObject["isOK"]=true;
        jObject["responseType"]="joinPeer";
        peerObj["success"]=false;
        peerObj["msg"]="Peer doesnt exist or connected to server!";
        jObject["peer"]=peerObj;
        QJsonDocument jdoc=QJsonDocument(jObject);
        client->sendBinaryMessage(jdoc.toBinaryData());
        return;
    }

    // Not to join twice
    QString currentClientName= clCmdSocketToName[client];
    if((currentClientName==peerUserName)||peers.contains(currentClientName))
    {
        QJsonObject peerObj;
        QJsonObject jObject;
        jObject["isOK"]=true;
        jObject["responseType"]="joinPeer";
        peerObj["success"]=false;
        peerObj["msg"]="Join peer not valid";
        jObject["peer"]=peerObj;
        QJsonDocument jdoc=QJsonDocument(jObject);
        client->sendBinaryMessage(jdoc.toBinaryData());
        return;
    }


    QJsonObject peerObj;
    QJsonObject jObject;
    jObject["isOK"]=true;
    jObject["responseType"]="joinPeer";
    peerObj["success"]=true;
    peerObj["userName"]=clCmdSocketToName[client];
    jObject["peer"]=peerObj;
    QJsonDocument jdoc=QJsonDocument(jObject);

    //1. tell peer to connect to data socket  server
    QWebSocket*peerSck=clNameToCmdSocket[peerUserName];
    if(peerSck)
        peerSck->sendBinaryMessage(jdoc.toBinaryData());
    //2. tell first to connect to data socket server
    peerObj["userName"]=peerUserName;
    client->sendBinaryMessage(jdoc.toBinaryData());

    peers.insert(peerUserName,clCmdSocketToName[client]);
    peers.insert(clCmdSocketToName[client],peerUserName);

}

void Server::setKeepAliveTimedOut(int timedOut,int packetCount,int intrvl)
{
    int enableKeepAlive = 1;
    int fd = commandServer->socketDescriptor();

    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &enableKeepAlive, sizeof(enableKeepAlive));

    int maxIdle = timedOut; /* seconds */
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &maxIdle, sizeof(maxIdle));

    int count =packetCount;  // send up to 3 keepalive packets out, then disconnect if no response
    setsockopt(fd, SOL_TCP, TCP_KEEPCNT, &count, sizeof(count));

    int interval = intrvl;   // send a keepalive packet out every 2 seconds (after the 5 second idle period)
    setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));
}

bool Server::isUserAuth(QString userName)
{
    return clNameToCmdSocket.contains(userName);
}

bool Server::isUserAuth(QWebSocket *websocket)
{
    return this->clCmdSocketToName.contains(websocket);
}
