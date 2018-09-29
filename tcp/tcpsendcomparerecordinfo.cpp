#include "tcpsendcomparerecordinfo.h"

tcpSendCompareRecordInfo *tcpSendCompareRecordInfo::instance = NULL;

tcpSendCompareRecordInfo::tcpSendCompareRecordInfo(QObject *parent) : QObject(parent)
{
}

void tcpSendCompareRecordInfo::init()
{
    ConnectStateFlag = tcpSendCompareRecordInfo::DisConnectedState;

    SendCompareRecordInfoToPlatformCenterSocket = new QTcpSocket(this);
    connect(SendCompareRecordInfoToPlatformCenterSocket,SIGNAL(connected()),this,SLOT(slotEstablishConnection()));
    connect(SendCompareRecordInfoToPlatformCenterSocket,SIGNAL(disconnected()),this,SLOT(slotCloseConnection()));
    connect(SendCompareRecordInfoToPlatformCenterSocket,SIGNAL(error(QAbstractSocket::SocketError)),this,SLOT(slotDisplayError(QAbstractSocket::SocketError)));
    connect(SendCompareRecordInfoToPlatformCenterSocket,SIGNAL(readyRead()),this,SLOT(slotRecvMsgFromPlatformCenter()));

    SendCompareRecordInfoToPlatformCenterTimer = new QTimer(this);
    SendCompareRecordInfoToPlatformCenterTimer->setInterval(5000);
    connect(SendCompareRecordInfoToPlatformCenterTimer,SIGNAL(timeout()),this,SLOT(slotSendCompareRecordInfoToPlatformCenter()));
//    SendCompareRecordInfoToPlatformCenterTimer->start();

    ParseMsgFromPlatformCenterTimer = new QTimer(this);
    ParseMsgFromPlatformCenterTimer->setInterval(100);
    connect(ParseMsgFromPlatformCenterTimer,SIGNAL(timeout()),this,SLOT(slotParseMsgFromPlatformCenter()));
    ParseMsgFromPlatformCenterTimer->start();

    ParseVaildMsgFromPlatformCenterTimer = new QTimer(this);
    ParseVaildMsgFromPlatformCenterTimer->setInterval(100);
    connect(ParseVaildMsgFromPlatformCenterTimer,SIGNAL(timeout()),this,SLOT(slotParseVaildMsgFromPlatformCenter()));
    ParseVaildMsgFromPlatformCenterTimer->start();
}

void tcpSendCompareRecordInfo::slotSendCompareRecordInfoToPlatformCenter()
{
    SendCompareRecordInfoToPlatformCenterTimer->stop();

    //xml声明
    QString Msg;
    Msg.append("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
    Msg.append(QString("<DbServer AgentID=\"%1\">").arg(GlobalConfig::AgentID));

    //把数据库中所有没有上传成功的比对记录全部上传到平台中心
    bool flag = false;
    QString SelectSql = QString("SELECT * FROM compare_record_info_table WHERE isUploadPlatformCenter = 'NO'");

    QSqlQuery query;
    query.exec(SelectSql);
    qDebug() << SelectSql << query.lastError();

    while (query.next()) {
        flag = true;

        QString CompareRecordID = query.value(0).toString();
        QString CompareResult = query.value(1).toString().toLocal8Bit().toBase64();
        QString PersonBuild = query.value(2).toString().toLocal8Bit().toBase64();
        QString PersonUnit = query.value(3).toString().toLocal8Bit().toBase64();
        QString PersonLevel = query.value(4).toString().toLocal8Bit().toBase64();
        QString PersonRoom = query.value(5).toString().toLocal8Bit().toBase64();
        QString PersonName = query.value(6).toString().toLocal8Bit().toBase64();
        QString PersonSex = query.value(7).toString().toLocal8Bit().toBase64();
        QString PersonType = query.value(8).toString().toLocal8Bit().toBase64();
        QString IDCardNumber = query.value(9).toString().toLocal8Bit().toBase64();
        QString PhoneNumber = query.value(10).toString();
        QString ExpiryTime = query.value(11).toString();
        QString Blacklist = query.value(12).toString();
        QString isActivate = query.value(13).toString();
        QString FaceSimilarity = query.value(14).toString();
        QString UseTime = query.value(15).toString();
        QString TriggerTime = query.value(16).toString();
        QString EnterSnapPicUrl = query.value(17).toString();
        QString OriginalSnapPicUrl = query.value(18).toString();

        QString EnterSnapPicBase64;
        if (!EnterSnapPicUrl.isEmpty()) {
            EnterSnapPicBase64 = CommonSetting::QImage_To_Base64(QImage(EnterSnapPicUrl));
        }

        QString OriginalSnapPicBase64;
        if (!OriginalSnapPicUrl.isEmpty()) {
            OriginalSnapPicBase64 = CommonSetting::QImage_To_Base64(QImage(OriginalSnapPicUrl));
        }

        Msg.append(QString("<CompareRecordInfo CompareRecordID=\"%1\" CompareResult=\"%2\" PersonBuild=\"%3\" PersonUnit=\"%4\" PersonLevel=\"%5\" PersonRoom=\"%6\" PersonName=\"%7\" PersonSex=\"%8\" PersonType=\"%9\" IDCardNumber=\"%10\" PhoneNumber=\"%11\" ExpiryTime=\"%12\" Blacklist=\"%13\" isActivate=\"%14\" FaceSimilarity=\"%15\" UseTime=\"%16\" TriggerTime=\"%17\">").arg(CompareRecordID).arg(CompareResult).arg(PersonBuild).arg(PersonUnit)
                   .arg(PersonLevel).arg(PersonRoom).arg(PersonName).arg(PersonSex)
                   .arg(PersonType).arg(IDCardNumber).arg(PhoneNumber).arg(ExpiryTime)
                   .arg(Blacklist).arg(isActivate).arg(FaceSimilarity).arg(UseTime)
                   .arg(TriggerTime));
        Msg.append(QString("<EnterSnapPic>%1</EnterSnapPic>").arg(EnterSnapPicBase64));
        Msg.append(QString("<OriginalSnapPic>%1</OriginalSnapPic>").arg(OriginalSnapPicBase64));
        Msg.append(QString("</CompareRecordInfo>"));
    }

    if (flag) {
        Msg.append("</DbServer>");

        int length = Msg.toLocal8Bit().size();
        Msg = QString("IDOOR:") + QString("%1").arg(length,14,10,QLatin1Char('0')) + Msg;

        SendMsg = Msg;

        //使用tcp长连接
        if(ConnectStateFlag == tcpSendCompareRecordInfo::ConnectedState){
            SendCompareRecordInfoToPlatformCenterSocket->write(SendMsg.toLatin1());
        }else if(ConnectStateFlag == tcpSendCompareRecordInfo::DisConnectedState){
            SendCompareRecordInfoToPlatformCenterSocket->disconnectFromHost();
            SendCompareRecordInfoToPlatformCenterSocket->abort();

            SendCompareRecordInfoToPlatformCenterSocket->connectToHost(
                        GlobalConfig::PlatformCenterIP,GlobalConfig::PlatformCenterPort);
        }
    }

    SendCompareRecordInfoToPlatformCenterTimer->start();
}

void tcpSendCompareRecordInfo::slotEstablishConnection()
{
    qDebug() << "connect to PlatformCenter succeed";

    ConnectStateFlag = tcpSendCompareRecordInfo::ConnectedState;

    SendCompareRecordInfoToPlatformCenterSocket->write(SendMsg.toLatin1());
}

void tcpSendCompareRecordInfo::slotCloseConnection()
{
    qDebug() << "close connection to PlatformCenter";
    ConnectStateFlag = tcpSendCompareRecordInfo::DisConnectedState;
}

void tcpSendCompareRecordInfo::slotDisplayError(QAbstractSocket::SocketError socketError)
{
    switch(socketError){
    case QAbstractSocket::ConnectionRefusedError:
        qDebug() << "tcpSendCompareRecordInfo = " << "QAbstractSocket::ConnectionRefusedError";
        break;

    case QAbstractSocket::RemoteHostClosedError:
        qDebug() << "tcpSendCompareRecordInfo = " << "QAbstractSocket::RemoteHostClosedError";
        break;

    case QAbstractSocket::HostNotFoundError:
        qDebug() << "tcpSendCompareRecordInfo = " << "QAbstractSocket::HostNotFoundError";
        break;

    default:
        qDebug() << "tcpSendCompareRecordInfo = " << "The following error occurred:"
                    + SendCompareRecordInfoToPlatformCenterSocket->errorString();
        break;
    }

    ConnectStateFlag = tcpSendCompareRecordInfo::DisConnectedState;

    //断开tcp连接
    SendCompareRecordInfoToPlatformCenterSocket->disconnectFromHost();
    SendCompareRecordInfoToPlatformCenterSocket->abort();
}

void tcpSendCompareRecordInfo::slotRecvMsgFromPlatformCenter()
{
    if (SendCompareRecordInfoToPlatformCenterSocket->bytesAvailable() <= 0) {
        return;
    }

    QByteArray msg = SendCompareRecordInfoToPlatformCenterSocket->readAll();
    RecvMsgBuffer.append(msg);
}

void tcpSendCompareRecordInfo::slotParseMsgFromPlatformCenter()
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

void tcpSendCompareRecordInfo::slotParseVaildMsgFromPlatformCenter()
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
                    if(RootElement.hasAttribute("CompareRecordID")){
                        QString CompareRecordID =
                                RootElement.attributeNode("CompareRecordID").value();

                        QSqlDatabase::database().transaction();

                        QStringList CompareRecordIDList = CompareRecordID.split(",");
                        int size = CompareRecordIDList.size();
                        for (int i = 0; i < size; i++) {
                            QString UpdateSql = QString("UPDATE compare_record_info_table SET isUploadPlatformCenter = '%1' WHERE CompareRecordID IN ('%2')").arg("YES").arg(CompareRecordIDList.at(i));
                            QSqlQuery query;
                            query.exec(UpdateSql);
                            qDebug() << UpdateSql << query.lastError();
                        }

                        QSqlDatabase::database().commit();
                    }
                }
            }
        }
    }
}
