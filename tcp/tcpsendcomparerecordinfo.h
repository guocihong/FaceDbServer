#ifndef TCPSENDCOMPARERECORDINFO_H
#define TCPSENDCOMPARERECORDINFO_H

#include "globalconfig.h"

//本类专门用来将比对记录信息发送给平台中心，使用tcp长连接，使用端口5555

class tcpSendCompareRecordInfo : public QObject
{
    Q_OBJECT
public:
    explicit tcpSendCompareRecordInfo(QObject *parent = 0);

    static tcpSendCompareRecordInfo *newInstance() {
        static QMutex mutex;

        if (!instance) {
            QMutexLocker locker(&mutex);

            if (!instance) {
                instance = new tcpSendCompareRecordInfo();
            }
        }

        return instance;
    }

    void init(void);

    enum ConnectState{
        ConnectedState,
        DisConnectedState
    };

public slots:
    void slotSendCompareRecordInfoToPlatformCenter();

    void slotEstablishConnection();
    void slotCloseConnection();
    void slotDisplayError(QAbstractSocket::SocketError socketError);
    void slotRecvMsgFromPlatformCenter();
    void slotParseMsgFromPlatformCenter();
    void slotParseVaildMsgFromPlatformCenter();

public:
    static tcpSendCompareRecordInfo *instance;

    QTimer *SendCompareRecordInfoToPlatformCenterTimer;

    QTcpSocket *SendCompareRecordInfoToPlatformCenterSocket;
    QTimer *ParseMsgFromPlatformCenterTimer;
    QTimer *ParseVaildMsgFromPlatformCenterTimer;
    QByteArray RecvMsgBuffer;
    QList<QByteArray> RecvVaildMsgBuffer;

    volatile enum ConnectState ConnectStateFlag;
    QString SendMsg;//等待发送的比对记录信息
};

#endif // TCPSENDCOMPARERECORDINFO_H
