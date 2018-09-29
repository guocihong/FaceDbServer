#ifndef TCPSENDPERSONINFO_H
#define TCPSENDPERSONINFO_H

#include "globalconfig.h"

//本类专门用来将人员信息下发给小区大门人脸比对分析设备，小区单元楼人脸比对分析设备，平台中心，使用tcp短连接，服务器主动断开tcp连接
class tcpSendPersonInfo : public QObject
{
    Q_OBJECT
public:
    explicit tcpSendPersonInfo(QObject *parent = 0);

    void ConnectToHost(QString ip, quint16 port);

    enum SendState {
        Success,
        Fail
    };

    enum DeviceType {
        MainEntrance,//主出入口:小区大门
        SubEntrance,//次出入口:小区单元楼
        PlatformCenter//平台中心
    };

public slots:
    void slotEstablishConnection();
    void slotCloseConnection();
    void slotDisplayError(QAbstractSocket::SocketError socketError);
    void slotRecvMsgFromRemoteDevice();
    void slotParseMsgFromRemoteDevice();
    void slotParseVaildMsgFromRemoteDevice();

public:
    QTcpSocket *SendPersonInfoToRemoteDeviceSocket;
    QTimer *ParseMsgFromRemoteDeviceTimer;//解析双目人脸比对分析设备发送过来的所有的数据包
    QTimer *ParseVaildMsgFromRemoteDeviceTimer;//解析双目人脸比对分析设备发送过来的完整的数据包
    QByteArray RecvMsgBuffer;//存放双目人脸比对分析设备tcp发送过来的所有消息，包括头部20个字节
    QList<QByteArray> RecvVaildMsgBuffer;//存放双目人脸比对分析设备tcp发送过来的完整数据包，不包括头部20个字节

    QString WaitSendPersonID;//等待发送人员信息的PersonID
    QString WaitSendMsg;//等待发送的人员信息
    enum SendState SendStateFlag;//发送状态
    enum DeviceType Type;//设备类型
};

#endif // TCPSENDPERSONINFO_H
