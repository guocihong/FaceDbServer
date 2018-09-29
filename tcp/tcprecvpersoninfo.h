#ifndef TCPRECVPERSONINFO_H
#define TCPRECVPERSONINFO_H

#include "globalconfig.h"

//本类专门用来接收人工访客机和自助访客机的消息（包含人员信息和其他查询类消息），使用tcp长连接
class tcpRecvPersonInfo : public QObject
{
    Q_OBJECT
public:
    explicit tcpRecvPersonInfo(QObject *parent = 0);

    static tcpRecvPersonInfo *newInstance() {
        static QMutex mutex;

        if (!instance) {
            QMutexLocker locker(&mutex);

            if (!instance) {
                instance = new tcpRecvPersonInfo();
            }
        }

        return instance;
    }

    void Listen(void);

    void SendData(TcpHelper *tcpHelper,const QString &Body);
    void SendDeviceHeart(TcpHelper *tcpHelper);//返回心跳


    void AddPersonInfo(QString PersonID, const QString &PersonBuild, const QString &PersonUnit, const QString &PersonLevel, const QString &PersonRoom, const QString &PersonName, const QString &PersonSex, const QString &PersonType, const QString &IDCardNumber, const QString &PhoneNumber, const QString &RegisterTime, const QString &ExpiryTime, const QString &Blacklist, const QString &isActivate, const QString &FeatureValue, const QString &PersonImageBase64, const QString &IDCardImageBase64, const QString &isUploadMainEntrance, const QString &isUploadSubEntrance, const QString &isUploadPlatformCenter);

    void SelectPersonInfo(TcpHelper *tcpHelper, const QString &PersonID, const QString &PersonBuild, const QString &PersonUnit, const QString &PersonLevel, const QString &PersonRoom, const QString &PersonName);

    void DeletePersonInfo(const QString &PersonID);

    void UpdatePersonInfo(const QString &PersonID, const QString &PersonBuild, const QString &PersonUnit, const QString &PersonLevel, const QString &PersonRoom, const QString &PersonName, const QString &PersonSex, const QString &PersonType, const QString &IDCardNumber, const QString &PhoneNumber, const QString &StartTime, const QString &EndTime, const QString &FeatureValue, const QString &PersonImageBase64, const QString &IDCardImageBase64);

    void ClearPersonInfo();


    void AddDeviceInfo(const QString &DeviceBuild, const QString &DeviceUnit, const QString &DeviceIP, const QString &MainStreamRtspAddr, const QString &SubStreamRtspAddr);
    void SelectDeviceInfo(TcpHelper *tcpHelper, const QString &DeviceIP);
    void DelectDeviceInfo(const QString &DeviceID);
    void UpdateDeviceInfo(const QString &DeviceID, const QString &DeviceBuild, const QString &DeviceUnit, const QString &DeviceIP, const QString &MainStreamRtspAddr, const QString &SubStreamRtspAddr);
    void ClearDeviceInfo();



    void SelectCompareRecordInfo(TcpHelper *tcpHelper, const QString &CompareRecordID, const QString &CompareResult, const QString &PersonBuild, const QString &PersonUnit, const QString &PersonLevel, const QString &PersonRoom, const QString &PersonName, const QString &PersonType, const QString &TriggerTime);
    void ClearCompareRecordInfo();

    void GetFaceServerIP (TcpHelper *tcpHelper);
    void ACKToPlatformCenter(TcpHelper *tcpHelper, QStringList PersonIDList);


    void AddAreaInfo(const QString &AreaBuild, const QString &AreaUnit, const QString &AreaLevel, const QString &AreaRoom);
    void SelectAreaInfo(TcpHelper *tcpHelper);
    void DeleteAreaInfo(const QString &AreaID);
    void UpdateAreaInfo(const QString &AreaID, const QString &AreaBuild, const QString &AreaUnit, const QString &AreaLevel, const QString &AreaRoom);
    void ClearAreaInfo();

public slots:
    void slotProcessVisitorDeviceConnection();
    void slotVisitorDeviceDisconnect();

    void slotRecvMsgFromVisitorDevice();
    void slotParseMsgFromVisitorDevice();
    void slotParseVaildMsgFromVisitorDevice();

private:
    static tcpRecvPersonInfo *instance;

    //监听人工访客机和自助访客机的tcp连接
    QTcpServer *ConnectionListener;
    QTimer *ParseMsgFromVisitorDeviceTimer;//解析前端设备发送过来的数据包
    QTimer *ParseVaildMsgFromVisitorDeviceTimer;//解析前端设备发送过来的数据包

    //保存前端设备的所有tcp连接对象
    QList<TcpHelper *> TcpHelperBuffer;
};

#endif // TCPRECVPERSONINFO_H
