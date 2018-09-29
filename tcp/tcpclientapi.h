#ifndef TCPCLIENTAPI_H
#define TCPCLIENTAPI_H

#include "globalconfig.h"

/* 1、本类专门用来将人员消息发送给前端执行器;将人员信息发送给平台;将比对记录信息发送给平台;将执行器设备信息发送给平台
 * 2、使用tcp短连接，端口6667,主动断开tcp连接
*/

namespace Client {
class TcpClientApi : public QThread
{
    Q_OBJECT
public:
    enum OperatorType {
        sendPersonInfo = 0x01,//发送人员信息
        sendCompareRecordInfo = 0x02,//发送比对记录信息
        sendDeviceInfo = 0x03//发送执行器设备信息
    };

    explicit TcpClientApi(enum OperatorType type, QObject *parent = 0);
    ~TcpClientApi();

protected:
    void run();

private:
    enum OperatorType type;
};

class Worker : public QObject
{
    Q_OBJECT
public:
    explicit Worker(enum TcpClientApi::OperatorType type, QObject *parent = 0);
    ~Worker();

    void SendPersonInfo();
    void SendCompareRecordInfo();
    void SendDeviceInfo();

public slots:
    void slotSendMsg();

private:
    enum TcpClientApi::OperatorType type;

    QTimer *SendMsgTimer;

    static QMutex *GlobalLock;

    //数据库连接对象
    QSqlDatabase db;

    //数据库连接名称
    QString connectionName;
};

class TcpSocketApi : public QTcpSocket
{
    Q_OBJECT
public:
    explicit TcpSocketApi(const QString &connectionName,const QString &InsignItemId, const QString &InsignItemIp, const QString &WaitSendMsg, QObject *parent = 0);

    void UpdatePersonSyncStatus(const QString &PersonID);
    void UpdateCompareRecordSyncStatus(const QString &CompareRecordID);
    void UpdateDeviceSyncStatus(const QString &DeviceID);

private slots:
    void slotEstablishConnection();
    void slotCloseConnection();
    void slotDisplayError(QAbstractSocket::SocketError socketError);
    void slotRecvMsg();
    void slotParseOriginalMsg();
    void slotParseVaildMsg();

private:
    //保存接收的所有原始数据
    QByteArray RecvOriginalMsgBuffer;

    //保存从接收到的所有原始数据中解析出一个个完整的数据包
    QList<QByteArray> RecvVaildMsgBuffer;

    QTimer *ParseOriginalMsgTimer;
    QTimer *ParseVaildMsgTimer;

    //执行器id
    QString InsignItemId;

    //执行器IP地址
    QString InsignItemIp;

    //待发送的数据包
    QString WaitSendMsg;

    //数据库连接名称
    QString connectionName;
};
}

#endif // TCPCLIENTAPI_H
