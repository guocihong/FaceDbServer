#ifndef PHONEAPI_H
#define PHONEAPI_H

#include "globalconfig.h"

namespace Phone {
class PhoneApi : public QTcpServer
{
    Q_OBJECT
public:
    explicit PhoneApi(QObject *parent = 0);

    static PhoneApi *newInstance() {
        static QMutex mutex;

        if (!instance) {
            QMutexLocker locker(&mutex);

            if (!instance) {
                instance = new PhoneApi();
            }
        }

        return instance;
    }

    void Listen();

protected:
#if (QT_VERSION >= QT_VERSION_CHECK(5,0,0))
    void incomingConnection(qintptr socketDescriptor);
#else
    void incomingConnection(int socketDescriptor);
#endif

private:
    static PhoneApi *instance;
};

class WorkThread : public QThread
{
    Q_OBJECT
public:
#if (QT_VERSION >= QT_VERSION_CHECK(5,0,0))
    explicit WorkThread(qintptr socketDescriptor, QObject *parent = 0);
#else
    explicit WorkThread(int socketDescriptor, QObject *parent = 0);
#endif

protected:
    void run();

private:
#if (QT_VERSION >= QT_VERSION_CHECK(5,0,0))
    qintptr socketDescriptor;
#else
    int socketDescriptor;
#endif
};

class TcpSocketApi : public QTcpSocket
{
    Q_OBJECT
public:
#if (QT_VERSION >= QT_VERSION_CHECK(5,0,0))
    explicit TcpSocketApi(qintptr socketDescriptor, QObject *parent = 0);
#else
    explicit TcpSocketApi(int socketDescriptor, QObject *parent = 0);
#endif

    ~TcpSocketApi();

    void SendDeviceHeart();
    void UpdateInsignItemParm(const QString &NodeName,const QString &AttributeName,const QString &Value);
    void UpdatePicSyncStatus(const QString &PersonID);
    void StartService();
    void StopService();

signals:
    void signalDisconnect();

private slots:
    void slotRecvMsg();
    void slotDisconnect();
    void slotParseOriginalMsg();
    void slotParseVaildMsg();
    void slotCheckTcpConnection();

    void slotSendSnapPic();
    void slotSendRegisterPic();

    void slotUpdateInsignItemParm(const QString &InsignItemIp,const QString &StatusInfo);

private:
    QMutex mutex;

    //保存接收的所有原始数据
    QByteArray RecvOriginalMsgBuffer;

    //保存从接收到的所有原始数据中解析出一个个完整的数据包
    QList<QByteArray> RecvVaildMsgBuffer;

    QTimer *ParseOriginalMsgTimer;
    QTimer *ParseVaildMsgTimer;
    QTimer *CheckTcpConnectionTimer;
    QTimer *SendSnapPicTimer;
    QTimer *SendRegisterPicTimer;

    //保存最后一次接收数据包的时间
    QDateTime LastRecvMsgTime;

    //数据库连接名称
    QString connectionName;
};

class TcpClientApi : public QTcpSocket
{
    Q_OBJECT
public:
    explicit TcpClientApi(const QString &InsignItemIp, const QString &WaitSendMsg, QObject *parent = 0);

    ~TcpClientApi();

private slots:
    void slotEstablishConnection();
    void slotCloseConnection();
    void slotDisplayError(QAbstractSocket::SocketError socketError);
    void slotRecvMsg();
    void slotParseOriginalMsg();
    void slotParseVaildMsg();

signals:
    void signalUpdateInsignItemParm(const QString &InsignItemIp,const QString &StatusInfo);

private:
    //保存接收的所有原始数据
    QByteArray RecvOriginalMsgBuffer;

    //保存从接收到的所有原始数据中解析出一个个完整的数据包
    QList<QByteArray> RecvVaildMsgBuffer;

    QTimer *ParseOriginalMsgTimer;

    QTimer *ParseVaildMsgTimer;

    //执行器IP地址
    QString InsignItemIp;

    //待发送的数据包
    QString WaitSendMsg;
};
}
#endif // PHONEAPI_H
