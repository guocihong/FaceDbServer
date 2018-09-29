#ifndef TCPSERVERAPI_H
#define TCPSERVERAPI_H

#include "globalconfig.h"

/*1、本类专门用来接收人工/自主访客机的所有消息、前端设备的比对记录、敏达平台的人员信息、PC机的配置工具信息
  2、使用tcp长连接，端口6666
*/

namespace Server {
//人员信息
typedef struct PersonInfo_t {
    QString PersonID;                   //人员id
    QString PersonBuild;                //楼栋
    QString PersonUnit;                 //单元
    QString PersonFloor;                //楼层
    QString PersonRoom;                 //房号
    QString PersonName;                 //姓名
    QString PersonSex;                  //性别
    QString PersonType;                 //人员类型
    QString IDCardNumber;               //身份证号码
    QString ICCardNumber;               //IC卡号
    QString PhoneNumber;                //手机号码1
    QString PhoneNumber2;               //手机号码2
    QString RegisterTime;               //注册时间
    QString ExpiryTime;                 //过期时间
    QString Blacklist;                  //是否拉黑人员
    QString isActivate;                 //是否激活/注销
    QString InsignItemIds;              //执行器id列表，使用|做为分隔符
    QString InsignItemSyncStatus;       //执行器同步状态，使用|做为分隔符
    QString FeatureValue;               //特征值，使用|做为分隔符
    QString PersonImageUrl;             //注册图片路径
    QString IDCardImageUrl;             //身份证图片路径
}PersonInfo;

//人员页信息
typedef struct PersonPageInfo_t {
    quint64 ResultCount;                //总记录数
    quint64 PageCount;                  //总页数
    quint64 ResultCurrent;              //每页多少条记录
    quint64 PageCurrent;                //当前第几页
}PersonPageInfo;

//执行器信息
typedef struct DeviceInfo_t {
    QString DeviceID;                   //执行器id
    QString DeviceBuild;                //楼栋
    QString DeviceUnit;                 //单元
    QString DeviceIP;                   //执行器ip地址
    QString Longitude;                  //经度
    QString Latitude;                   //纬度
    QString Altitude;                   //海拔
    QString MainStreamRtspAddr;         //主码流rtsp地址
    QString SubStreamRtspAddr;          //子码流rtsp地址
    QString isUploadPlatformCenter;     //是否上传敏达平台
}DeviceInfo;

//比对记录信息
typedef struct CompareRecordInfo_t {
    QString CompareRecordID;            //比对记录id
    QString CompareResult;              //比对结果
    QString PersonBuild;                //楼栋
    QString PersonUnit;                 //单元
    QString PersonFloor;                //楼层
    QString PersonRoom;                 //房号
    QString PersonName;                 //姓名
    QString PersonSex;                  //性别
    QString PersonType;                 //人员类型
    QString IDCardNumber;               //身份证号码
    QString ICCardNumber;               //ic卡号
    QString PhoneNumber;                //手机号码
    QString ExpiryTime;                 //过期时间
    QString Blacklist;                  //是否拉黑人员
    QString isActivate;                 //是否激活/注销
    QString FaceSimilarity;             //相似度
    QString UseTime;                    //比对耗时
    QString TriggerTime;                //进入时间
    QString EnterSnapPicUrl;            //现场进出抓拍图片路径
    QString OriginalSnapPicUrl;         //注册抓拍图片路径
    QString isUploadPlatformCenter;     //是否上传敏达平台
}CompareRecordInfo;

//比对记录页信息
typedef struct CompareRecordPageInfo_t {
    quint64 ResultCount;                //总记录数
    quint64 PageCount;                  //总页数
    quint64 ResultCurrent;              //每页多少条记录
    quint64 PageCurrent;                //当前第几页
}CompareRecordPageInfo;

//区域信息
typedef struct AreaInfo_t {
    QString AreaID;                     //区域id
    QString AreaBuild;                  //楼栋
    QString AreaUnit;                   //单元
    QString AreaFloor;                  //楼层
    QString AreaRoom;                   //房号
    QString AreaTel;                    //固话
}AreaInfo;

class TcpServerApi : public QTcpServer
{
    Q_OBJECT
public:
    explicit TcpServerApi(QObject *parent = 0);

    static TcpServerApi *newInstance() {
        static QMutex mutex;

        if (!instance) {
            QMutexLocker locker(&mutex);

            if (!instance) {
                instance = new TcpServerApi();
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
    static TcpServerApi *instance;
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

    //通用发送函数
    void SendData(const QString &Body);

    //返回心跳
    void SendDeviceHeart();

    //添加人员信息(来自人工/自助访客机)
    void AddPersonInfo(PersonInfo &person);

    //添加人员信息(来自敏达平台)
    void AddPersonInfoFromPlatformCenter(PersonInfo &person);

    //查询人员信息
    void SelectPersonInfo(const PersonInfo &person, PersonPageInfo &person_page);

    //删除人员信息
    void DeletePersonInfo(const PersonInfo &person);

    //更新人员信息
    void UpdatePersonInfo(PersonInfo &person);

    //清空人员信息
    void ClearPersonInfo();


    //添加设备信息
    void AddDeviceInfo(const DeviceInfo &device);

    //查询设备信息
    void SelectDeviceInfo(const DeviceInfo &device);

    //删除设备信息
    void DelectDeviceInfo(DeviceInfo &device);

    //更新设备信息
    void UpdateDeviceInfo(const DeviceInfo &device);

    //清空设备信息
    void ClearDeviceInfo();

    //同步设备
    void ResetDeviceInfo(const DeviceInfo &device);

    //查询比对记录信息
    void SelectCompareRecordInfo(const CompareRecordInfo &record, CompareRecordPageInfo &page);

    //删除比对记录信息
    void DeleteCompareRecordInfo(const CompareRecordInfo &record);

    //清空比对记录信息
    void ClearCompareRecordInfo();


    //添加区域信息
    void AddAreaInfo(const AreaInfo &area);

    //查询区域信息
    void SelectAreaInfo();

    //删除区域信息
    void DeleteAreaInfo(const AreaInfo &area);

    //更新区域信息
    void UpdateAreaInfo(const AreaInfo &area);

    //清空区域信息
    void ClearAreaInfo();


    //获取人脸比对服务器的ip地址
    void GetFaceServerIP();

    //发送反馈信息给敏达平台
    void SendAckToPlatformCenter(const QString &PersonID,const QString &Status,const QString &Msg);

    //发送反馈给前端门禁执行器设备
    void SendAckToDoorDevice(const QString &CompareRecordID);

signals:
    void signalDisconnect();

private slots:
    void slotRecvMsg();
    void slotDisconnect();
    void slotParseOriginalMsg();
    void slotParseVaildMsg();
    void slotCheckTcpConnection();

private:
    QMutex mutex;

    //保存接收的所有原始数据
    QByteArray RecvOriginalMsgBuffer;

    //保存从接收到的所有原始数据中解析出一个个完整的数据包
    QList<QByteArray> RecvVaildMsgBuffer;

    QTimer *ParseOriginalMsgTimer;
    QTimer *ParseVaildMsgTimer;
    QTimer *CheckTcpConnectionTimer;

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

#endif // TCPSERVERAPI_H
