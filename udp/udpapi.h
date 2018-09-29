#ifndef UDPAPI_H
#define UDPAPI_H

#include "globalconfig.h"

class UdpApi : public QObject
{
    Q_OBJECT
public:
    explicit UdpApi(QObject *parent = 0);

    static UdpApi *newInstance() {
        static QMutex mutex;

        if (!instance) {
            QMutexLocker locker(&mutex);

            if (!instance) {
                instance = new UdpApi();
            }
        }

        return instance;
    }

    //开启组播监听
    void Listen(void);

public slots:
    void slotProcessPendingDatagrams();
    void slotParseGroupMsg();
    void slotParseVaildGroupMsg();

private:
    static UdpApi *instance;

    QUdpSocket *udp_socket;

    QTimer *ParseGroupMsgTimer;
    QTimer *ParseVaildGroupMsgTimer;
    QByteArray GroupMsgBuffer;
    QList<QByteArray> VaildGroupMsgBuffer;
};

#endif // UDPAPI_H
