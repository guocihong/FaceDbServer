#ifndef TCPHELPER_H
#define TCPHELPER_H

#include <QObject>
#include <QTcpSocket>

class TcpHelper : public QObject
{
    Q_OBJECT
public:
    explicit TcpHelper(QObject *parent = 0);

    //保存服务器/PC客户端的tcp连接对象
    QTcpSocket *Socket;

    //保存从服务器/PC客户端接收的所有原始数据
    QByteArray RecvOriginalMsgBuffer;

    //从服务器/PC客户端接收的所有原始数据中解析出一个个完整的数据包
    QList<QByteArray> RecvVaildCompleteMsgBuffer;

    //返回给服务器/PC客户端的数据包
    QList<QByteArray> SendMsgBuffer;
};

#endif // TCPHELPER_H
