#include "tcpsendpersoninfo.h"

tcpSendPersonInfo::tcpSendPersonInfo(QObject *parent) : QObject(parent)
{
    this->SendStateFlag = tcpSendPersonInfo::Fail;

    SendPersonInfoToRemoteDeviceSocket = new QTcpSocket(this);
    connect(SendPersonInfoToRemoteDeviceSocket,SIGNAL(connected()),this,SLOT(slotEstablishConnection()));
    connect(SendPersonInfoToRemoteDeviceSocket,SIGNAL(disconnected()),this,SLOT(slotCloseConnection()));
    connect(SendPersonInfoToRemoteDeviceSocket,SIGNAL(error(QAbstractSocket::SocketError)),this,SLOT(slotDisplayError(QAbstractSocket::SocketError)));
    connect(SendPersonInfoToRemoteDeviceSocket,SIGNAL(readyRead()),this,SLOT(slotRecvMsgFromRemoteDevice()));

    //解析双目人脸比对分析设备发送过来的所有的数据包
    ParseMsgFromRemoteDeviceTimer = new QTimer(this);
    ParseMsgFromRemoteDeviceTimer->setInterval(100);
    connect(ParseMsgFromRemoteDeviceTimer,SIGNAL(timeout()),this,SLOT(slotParseMsgFromRemoteDevice()));
    ParseMsgFromRemoteDeviceTimer->start();

    //解析双目人脸比对分析设备发送过来的完整的数据包
    ParseVaildMsgFromRemoteDeviceTimer = new QTimer(this);
    ParseVaildMsgFromRemoteDeviceTimer->setInterval(100);
    connect(ParseVaildMsgFromRemoteDeviceTimer,SIGNAL(timeout()),this,SLOT(slotParseVaildMsgFromRemoteDevice()));
    ParseVaildMsgFromRemoteDeviceTimer->start();
}

void tcpSendPersonInfo::ConnectToHost(QString ip, quint16 port)
{
    SendPersonInfoToRemoteDeviceSocket->connectToHost(ip,port);
}

void tcpSendPersonInfo::slotEstablishConnection()
{
    qDebug() << "connect to RemoteDevice succeed";

    SendPersonInfoToRemoteDeviceSocket->write(WaitSendMsg.toLatin1());
}

void tcpSendPersonInfo::slotCloseConnection()
{
    qDebug() << "close connection to RemoteDevice";

//    ParseMsgFromRemoteDeviceTimer->stop();
//    ParseVaildMsgFromRemoteDeviceTimer->stop();

//    this->deleteLater();//memory si lou
}

void tcpSendPersonInfo::slotDisplayError(QAbstractSocket::SocketError socketError)
{
    switch(socketError){
    case QAbstractSocket::ConnectionRefusedError:
        qDebug() << "tcpSendPersonInfo = " << "QAbstractSocket::ConnectionRefusedError";
        break;

    case QAbstractSocket::RemoteHostClosedError:
        qDebug() << "tcpSendPersonInfo = " << "QAbstractSocket::RemoteHostClosedError";
        break;

    case QAbstractSocket::HostNotFoundError:
        qDebug() << "tcpSendPersonInfo = " << "QAbstractSocket::HostNotFoundError";
        break;

    default:
        qDebug() << "tcpSendPersonInfo = " << "The following error occurred:"
                    + SendPersonInfoToRemoteDeviceSocket->errorString();
        break;
    }

    //断开tcp连接
    SendPersonInfoToRemoteDeviceSocket->disconnectFromHost();
    SendPersonInfoToRemoteDeviceSocket->abort();
}

void tcpSendPersonInfo::slotRecvMsgFromRemoteDevice()
{
    if (SendPersonInfoToRemoteDeviceSocket->bytesAvailable() <= 0) {
        return;
    }

    QByteArray msg = SendPersonInfoToRemoteDeviceSocket->readAll();
    RecvMsgBuffer.append(msg);
}

void tcpSendPersonInfo::slotParseMsgFromRemoteDevice()
{
    int size = RecvMsgBuffer.size();

    while(size > 0) {
        //寻找帧头的索引
        int FrameHeadIndex = RecvMsgBuffer.indexOf("IDOOR");
        if (FrameHeadIndex < 0) {
            break;
        }

        if (size < (FrameHeadIndex + 20)) {
            break;
        }

        //取出xml数据包的长度，不包括帧头的20个字节
        int length = RecvMsgBuffer.mid(FrameHeadIndex + 6,14).toUInt();

        //没有收到一个完整的数据包
        if (size < (FrameHeadIndex + 20 + length)) {
            break;
        }

        //取出一个完整的xml数据包,不包括帧头20个字节
        QByteArray VaildCompletePackage = RecvMsgBuffer.mid(FrameHeadIndex + 20,length);

        //更新RecvMsgBuffer内容
        RecvMsgBuffer = RecvMsgBuffer.mid(FrameHeadIndex + 20 + length);

        //保存完整数据包
        RecvVaildMsgBuffer.append(VaildCompletePackage);
    }
}

void tcpSendPersonInfo::slotParseVaildMsgFromRemoteDevice()
{
    while(RecvVaildMsgBuffer.size() > 0) {
        QByteArray data = RecvVaildMsgBuffer.takeFirst();

        QDomDocument dom;
        QString errorMsg;
        int errorLine, errorColumn;

        if(!dom.setContent(data, &errorMsg, &errorLine, &errorColumn)) {
            qDebug()<< "slotParseVaildMsgFromRemoteDevice= " << "Parse error: " +  errorMsg;
            qDebug() << data;
            continue;
        }

        qDebug() << data;

        QDomElement RootElement = dom.documentElement();//获取根元素
        if(RootElement.tagName() == "RemoteDevice"){ //根元素名称
            //判断根元素是否有这个属性
            if(RootElement.hasAttribute("PersonID")){
                //获得这个属性对应的值
                QString PersonID = RootElement.attributeNode("PersonID").value();

                //更新对应obj的发送状态
                if (this->Type == tcpSendPersonInfo::MainEntrance) {//主出入口:小区大门
                    //更新对应obj的发送状态
                    foreach (tcpSendPersonInfo *obj, GlobalConfig::MainEntranceList) {
                        if ((obj == this) && (obj->WaitSendPersonID == PersonID)) {
                            obj->SendStateFlag = tcpSendPersonInfo::Success;
                        }
                    }

                    //统计具有相同UUID的obj个数
                    int SameUUIDObjCount = 0;
                    foreach (tcpSendPersonInfo *obj, GlobalConfig::MainEntranceList) {
                        if (obj->WaitSendPersonID == PersonID) {
                            SameUUIDObjCount++;
                        }
                    }


                    //统计具有相同WaitSendPersonID的obj对象msg发送成功的个数
                    int MsgSendSucceedCount = 0;
                    foreach (tcpSendPersonInfo *obj, GlobalConfig::MainEntranceList) {
                        if ((obj->WaitSendPersonID == PersonID) && (obj->SendStateFlag == tcpSendPersonInfo::Success)) {
                            MsgSendSucceedCount++;
                        }
                    }

                    if (SameUUIDObjCount == MsgSendSucceedCount) {//所有设备都发送成功
                        QString UpdateSql = QString("UPDATE person_info_table SET isUploadMainEntrance = '%1' WHERE PersonID IN ('%2')").arg("YES").arg(PersonID);
                        QSqlQuery query;
                        query.exec(UpdateSql);
                        qDebug() << UpdateSql << query.lastError();
                    }
                } else if (this->Type == tcpSendPersonInfo::SubEntrance) {//次出入口:小区单元楼
                    //更新对应obj的发送状态
                    foreach (tcpSendPersonInfo *obj, GlobalConfig::SubEntranceList) {
                        if ((obj == this) && (obj->WaitSendPersonID == PersonID)) {
                            obj->SendStateFlag = tcpSendPersonInfo::Success;
                        }
                    }

                    //统计具有相同UUID的obj个数
                    int SameUUIDObjCount = 0;
                    foreach (tcpSendPersonInfo *obj, GlobalConfig::SubEntranceList) {
                        if (obj->WaitSendPersonID == PersonID) {
                            SameUUIDObjCount++;
                        }
                    }


                    //统计具有相同WaitSendPersonID的obj对象msg发送成功的个数
                    int MsgSendSucceedCount = 0;
                    foreach (tcpSendPersonInfo *obj, GlobalConfig::SubEntranceList) {
                        if ((obj->WaitSendPersonID == PersonID) && (obj->SendStateFlag == tcpSendPersonInfo::Success)) {
                            MsgSendSucceedCount++;
                        }
                    }

                    if (SameUUIDObjCount == MsgSendSucceedCount) {//所有设备都发送成功
                        QString UpdateSql = QString("UPDATE person_info_table SET isUploadSubEntrance = '%1' WHERE PersonID IN ('%2')").arg("YES").arg(PersonID);
                        QSqlQuery query;
                        query.exec(UpdateSql);
                        qDebug() << UpdateSql << query.lastError();
                    }
                }

                //断开tcp连接
                SendPersonInfoToRemoteDeviceSocket->disconnectFromHost();
                SendPersonInfoToRemoteDeviceSocket->abort();
            }
        }

        if(RootElement.tagName() == "PlatformCenter"){ //根元素名称
            if(RootElement.hasAttribute("NowTime")){
                QDateTime now = QDateTime::currentDateTime();
                if (GlobalConfig::LastUpdateDateTime.secsTo(now) >= (30 * 60)) {
                    QString NowTime = RootElement.attributeNode("NowTime").value();
                    CommonSetting::SettingSystemDateTime(NowTime);

                    GlobalConfig::LastUpdateDateTime = now;
                }
            }

            if(RootElement.hasAttribute("AgentID")){
                QString AgentID = RootElement.attributeNode("AgentID").value();
                if (AgentID == GlobalConfig::AgentID) {
                    if(RootElement.hasAttribute("PersonID")){
                        QString PersonID = RootElement.attributeNode("PersonID").value();

                        if (this->Type == tcpSendPersonInfo::PlatformCenter) {//平台中心
                            //更新对应obj的发送状态
                            foreach (tcpSendPersonInfo *obj, GlobalConfig::PlatformCenterList) {
                                if ((obj == this) && (obj->WaitSendPersonID == PersonID)) {
                                    obj->SendStateFlag = tcpSendPersonInfo::Success;
                                }
                            }

                            //统计具有相同UUID的obj个数
                            int SameUUIDObjCount = 0;
                            foreach (tcpSendPersonInfo *obj, GlobalConfig::PlatformCenterList) {
                                if (obj->WaitSendPersonID == PersonID) {
                                    SameUUIDObjCount++;
                                }
                            }


                            //统计具有相同WaitSendPersonID的obj对象msg发送成功的个数
                            int MsgSendSucceedCount = 0;
                            foreach (tcpSendPersonInfo *obj, GlobalConfig::PlatformCenterList) {
                                if ((obj->WaitSendPersonID == PersonID) && (obj->SendStateFlag == tcpSendPersonInfo::Success)) {
                                    MsgSendSucceedCount++;
                                }
                            }

                            if (SameUUIDObjCount == MsgSendSucceedCount) {//所有设备都发送成功
                                QString UpdateSql = QString("UPDATE person_info_table SET isUploadPlatformCenter = '%1' WHERE PersonID IN ('%2')").arg("YES").arg(PersonID);
                                QSqlQuery query;
                                query.exec(UpdateSql);
                                qDebug() << UpdateSql << query.lastError();
                            }
                        }

                        //断开tcp连接
                        SendPersonInfoToRemoteDeviceSocket->disconnectFromHost();
                        SendPersonInfoToRemoteDeviceSocket->abort();
                    }
                }
            }
        }
    }
}
