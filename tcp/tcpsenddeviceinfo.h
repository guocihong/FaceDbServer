#ifndef TCPSENDDEVICEINFO_H
#define TCPSENDDEVICEINFO_H

#include "globalconfig.h"

//本类专门用来将设备信息发送给平台中心，使用tcp短连接，小区人脸识别服务器主动断开tcp连接，使用端口5555
class tcpSendDeviceInfo : public QObject
{
    Q_OBJECT
public:
    explicit tcpSendDeviceInfo(QObject *parent = 0);

    static tcpSendDeviceInfo *newInstance() {
        static QMutex mutex;

        if (!instance) {
            QMutexLocker locker(&mutex);

            if (!instance) {
                instance = new tcpSendDeviceInfo();
            }
        }

        return instance;
    }

    void init(void);

public slots:
    void slotSendDeviceInfoToPlatformCenter();

    void slotEstablishConnection();
    void slotCloseConnection();
    void slotDisplayError(QAbstractSocket::SocketError socketError);
    void slotRecvMsgFromPlatformCenter();
    void slotParseMsgFromPlatformCenter();
    void slotParseVaildMsgFromPlatformCenter();

public:
    static tcpSendDeviceInfo *instance;

    QTimer *SendDeviceInfoToPlatformCenterTimer;

    QTcpSocket *SendDeviceInfoToPlatformCenterSocket;
    QTimer *ParseMsgFromPlatformCenterTimer;
    QTimer *ParseVaildMsgFromPlatformCenterTimer;
    QByteArray RecvMsgBuffer;
    QList<QByteArray> RecvVaildMsgBuffer;

    QString SendMsg;//等待发送的比对记录信息
};

#endif // TCPSENDDEVICEINFO_H
