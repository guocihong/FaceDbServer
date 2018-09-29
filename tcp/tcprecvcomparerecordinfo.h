#ifndef TCPRECVCOMPARERECORDINFO_H
#define TCPRECVCOMPARERECORDINFO_H

#include "globalconfig.h"

//本类专门用来接收双目人脸比对分析设备的比对记录信息，使用tcp长连接
class tcpRecvCompareRecordInfo : public QObject
{
    Q_OBJECT
public:
    explicit tcpRecvCompareRecordInfo(QObject *parent = 0);

    static tcpRecvCompareRecordInfo *newInstance() {
        static QMutex mutex;

        if (!instance) {
            QMutexLocker locker(&mutex);

            if (!instance) {
                instance = new tcpRecvCompareRecordInfo();
            }
        }

        return instance;
    }

    void Listen(void);
    void ACK(TcpHelper *tcpHelper, QString CompareRecordID);

public slots:
    void slotProcessRemoteDeviceConnection();
    void slotRemoteDeviceDisconnect();

    void slotRecvMsgFromRemoteDevice();
    void slotParseMsgFromRemoteDevice();
    void slotParseVaildMsgFromRemoteDevice();

private:
    static tcpRecvCompareRecordInfo *instance;

    //监听前端设备的tcp连接
    QTcpServer *ConnectionListener;
    QTimer *ParseMsgFromRemoteDeviceTimer;//解析前端设备发送过来的数据包
    QTimer *ParseVaildMsgFromRemoteDeviceTimer;//解析前端设备发送过来的数据包

    //保存前端设备的所有tcp连接对象
    QList<TcpHelper *> TcpHelperBuffer;
};

#endif // TCPRECVCOMPARERECORDINFO_H
