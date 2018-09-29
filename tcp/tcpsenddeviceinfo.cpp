#include "tcpsenddeviceinfo.h"

tcpSendDeviceInfo *tcpSendDeviceInfo::instance = NULL;

tcpSendDeviceInfo::tcpSendDeviceInfo(QObject *parent) : QObject(parent)
{

}

void tcpSendDeviceInfo::init()
{
    SendDeviceInfoToPlatformCenterSocket = new QTcpSocket(this);
    connect(SendDeviceInfoToPlatformCenterSocket,SIGNAL(connected()),this,SLOT(slotEstablishConnection()));
    connect(SendDeviceInfoToPlatformCenterSocket,SIGNAL(disconnected()),this,SLOT(slotCloseConnection()));
    connect(SendDeviceInfoToPlatformCenterSocket,SIGNAL(error(QAbstractSocket::SocketError)),this,SLOT(slotDisplayError(QAbstractSocket::SocketError)));
    connect(SendDeviceInfoToPlatformCenterSocket,SIGNAL(readyRead()),this,SLOT(slotRecvMsgFromPlatformCenter()));

    SendDeviceInfoToPlatformCenterTimer = new QTimer(this);
    SendDeviceInfoToPlatformCenterTimer->setInterval(5000);
    connect(SendDeviceInfoToPlatformCenterTimer,SIGNAL(timeout()),this,SLOT(slotSendDeviceInfoToPlatformCenter()));
//    SendDeviceInfoToPlatformCenterTimer->start();

    ParseMsgFromPlatformCenterTimer = new QTimer(this);
    ParseMsgFromPlatformCenterTimer->setInterval(100);
    connect(ParseMsgFromPlatformCenterTimer,SIGNAL(timeout()),this,SLOT(slotParseMsgFromPlatformCenter()));
    ParseMsgFromPlatformCenterTimer->start();

    ParseVaildMsgFromPlatformCenterTimer = new QTimer(this);
    ParseVaildMsgFromPlatformCenterTimer->setInterval(100);
    connect(ParseVaildMsgFromPlatformCenterTimer,SIGNAL(timeout()),this,SLOT(slotParseVaildMsgFromPlatformCenter()));
    ParseVaildMsgFromPlatformCenterTimer->start();
}

void tcpSendDeviceInfo::slotSendDeviceInfoToPlatformCenter()
{
    SendDeviceInfoToPlatformCenterTimer->stop();

    //xml声明
    QString Msg;
    Msg.append("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
    Msg.append(QString("<DbServer AgentID=\"%1\">").arg(GlobalConfig::AgentID));

    //把数据库中所有没有上传成功的设备信息全部上传到平台中心
    bool flag = false;
    QString SelectSql = QString("SELECT * FROM device_info_table WHERE isUploadPlatformCenter = 'NO'");

    QSqlQuery query;
    query.exec(SelectSql);
    qDebug() << SelectSql << query.lastError();

    while (query.next()) {
        flag = true;

        QString DeviceID = query.value(0).toString();
        QString DeviceBuild = query.value(1).toString().toLocal8Bit().toBase64();
        QString DeviceUnit = query.value(2).toString().toLocal8Bit().toBase64();
        QString DeviceIP = query.value(3).toString();
        QString MainStreamRtspAddr = query.value(4).toString();
        QString SubStreamRtspAddr = query.value(5).toString();

        Msg.append(QString("<DeviceInfo DeviceID=\"%1\" DeviceBuild=\"%2\" DeviceUnit=\"%3\" DeviceIP=\"%4\" MainStreamRtspAddr=\"%5\" SubStreamRtspAddr=\"%6\" />")
                   .arg(DeviceID).arg(DeviceBuild).arg(DeviceUnit).arg(DeviceIP)
                   .arg(MainStreamRtspAddr).arg(SubStreamRtspAddr));
    }

    if (flag) {
        Msg.append("</DbServer>");

        int length = Msg.toLocal8Bit().size();
        Msg = QString("IDOOR:") + QString("%1").arg(length,14,10,QLatin1Char('0')) + Msg;

        SendMsg = Msg;

        SendDeviceInfoToPlatformCenterSocket->disconnectFromHost();
        SendDeviceInfoToPlatformCenterSocket->abort();

        SendDeviceInfoToPlatformCenterSocket->connectToHost(
                    GlobalConfig::PlatformCenterIP,GlobalConfig::PlatformCenterPort);
    }

    SendDeviceInfoToPlatformCenterTimer->start();
}

void tcpSendDeviceInfo::slotEstablishConnection()
{
    qDebug() << "connect to PlatformCenter succeed";

    SendDeviceInfoToPlatformCenterSocket->write(SendMsg.toLatin1());
}

void tcpSendDeviceInfo::slotCloseConnection()
{
    qDebug() << "close connection to PlatformCenter";
}

void tcpSendDeviceInfo::slotDisplayError(QAbstractSocket::SocketError socketError)
{
    switch(socketError){
    case QAbstractSocket::ConnectionRefusedError:
        qDebug() << "tcpSendDeviceInfo = " << "QAbstractSocket::ConnectionRefusedError";
        break;

    case QAbstractSocket::RemoteHostClosedError:
        qDebug() << "tcpSendDeviceInfo = " << "QAbstractSocket::RemoteHostClosedError";
        break;

    case QAbstractSocket::HostNotFoundError:
        qDebug() << "tcpSendDeviceInfo = " << "QAbstractSocket::HostNotFoundError";
        break;

    default:
        qDebug() << "tcpSendDeviceInfo = " << "The following error occurred:"
                    + SendDeviceInfoToPlatformCenterSocket->errorString();
        break;
    }

    //断开tcp连接
    SendDeviceInfoToPlatformCenterSocket->disconnectFromHost();
    SendDeviceInfoToPlatformCenterSocket->abort();
}

void tcpSendDeviceInfo::slotRecvMsgFromPlatformCenter()
{
    if (SendDeviceInfoToPlatformCenterSocket->bytesAvailable() <= 0) {
        return;
    }

    QByteArray msg = SendDeviceInfoToPlatformCenterSocket->readAll();
    RecvMsgBuffer.append(msg);
}

void tcpSendDeviceInfo::slotParseMsgFromPlatformCenter()
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

void tcpSendDeviceInfo::slotParseVaildMsgFromPlatformCenter()
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
                    if(RootElement.hasAttribute("DeviceID")){
                        QString DeviceID = RootElement.attributeNode("DeviceID").value();

                        QSqlDatabase::database().transaction();

                        QStringList DeviceIDList = DeviceID.split(",");
                        int size = DeviceIDList.size();
                        for (int i = 0; i < size; i++) {
                            QString UpdateSql = QString("UPDATE device_info_table SET isUploadPlatformCenter = '%1' WHERE DeviceID IN ('%2')").arg("YES").arg(DeviceIDList.at(i));
                            QSqlQuery query;
                            query.exec(UpdateSql);
                            qDebug() << UpdateSql << query.lastError();
                        }

                        QSqlDatabase::database().commit();

                        SendDeviceInfoToPlatformCenterSocket->disconnectFromHost();
                        SendDeviceInfoToPlatformCenterSocket->abort();
                    }
                }
            }
        }
    }
}
