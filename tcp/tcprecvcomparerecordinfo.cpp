#include "tcprecvcomparerecordinfo.h"

tcpRecvCompareRecordInfo *tcpRecvCompareRecordInfo::instance = NULL;

tcpRecvCompareRecordInfo::tcpRecvCompareRecordInfo(QObject *parent) : QObject(parent)
{

}

void tcpRecvCompareRecordInfo::Listen()
{
    //监听前端设备的tcp连接
    ConnectionListener = new QTcpServer(this);
    connect(ConnectionListener,SIGNAL(newConnection()),this,SLOT(slotProcessRemoteDeviceConnection()));
#if (QT_VERSION > QT_VERSION_CHECK(5,0,0))
    ConnectionListener->listen(QHostAddress::AnyIPv4,GlobalConfig::RecvCompareRecordInfoPort);
#else
    ConnectionListener->listen(QHostAddress::Any,GlobalConfig::RecvCompareRecordInfoPort);
#endif

    //解析前端设备发送过来的数据包
    ParseMsgFromRemoteDeviceTimer = new QTimer(this);
    ParseMsgFromRemoteDeviceTimer->setInterval(100);
    connect(ParseMsgFromRemoteDeviceTimer,SIGNAL(timeout()),this,SLOT(slotParseMsgFromRemoteDevice()));
    ParseMsgFromRemoteDeviceTimer->start();

    //解析前端设备发送过来的数据包
    ParseVaildMsgFromRemoteDeviceTimer = new QTimer(this);
    ParseVaildMsgFromRemoteDeviceTimer->setInterval(100);
    connect(ParseVaildMsgFromRemoteDeviceTimer,SIGNAL(timeout()),this,SLOT(slotParseVaildMsgFromRemoteDevice()));
    ParseVaildMsgFromRemoteDeviceTimer->start();
}

void tcpRecvCompareRecordInfo::slotProcessRemoteDeviceConnection()
{
    QTcpSocket *RecvMsgFromRemoteDeviceSocket = ConnectionListener->nextPendingConnection();

    TcpHelper *tcpHelper = new TcpHelper();
    tcpHelper->Socket = RecvMsgFromRemoteDeviceSocket;
    TcpHelperBuffer.append(tcpHelper);

    if (!RecvMsgFromRemoteDeviceSocket->peerAddress().toString().isEmpty()) {
        connect(RecvMsgFromRemoteDeviceSocket, SIGNAL(readyRead()), this, SLOT(slotRecvMsgFromRemoteDevice()));
        connect(RecvMsgFromRemoteDeviceSocket, SIGNAL(disconnected()), this, SLOT(slotRemoteDeviceDisconnect()));

        qDebug() << QString("RemoteDevice Connect:\n\tIP = ") +
                    RecvMsgFromRemoteDeviceSocket->peerAddress().toString() +
                    QString("\n\tPort = ") + QString::number(RecvMsgFromRemoteDeviceSocket->peerPort());
    }
}

void tcpRecvCompareRecordInfo::slotRemoteDeviceDisconnect()
{
    QTcpSocket *RecvMsgFromRemoteDeviceSocket = (QTcpSocket *)sender();

    foreach (TcpHelper *tcpHelper, TcpHelperBuffer) {
        if (tcpHelper->Socket == RecvMsgFromRemoteDeviceSocket) {
            TcpHelperBuffer.removeAll(tcpHelper);
            delete tcpHelper;
        }
    }

    qDebug() << QString("RemoteDevice Disconnect:\n\tIP = ") +
                RecvMsgFromRemoteDeviceSocket->peerAddress().toString() +
                QString("\n\tPort = ") + QString::number(RecvMsgFromRemoteDeviceSocket->peerPort());
}

void tcpRecvCompareRecordInfo::slotRecvMsgFromRemoteDevice()
{
    QTcpSocket *RecvMsgFromRemoteDeviceSocket = (QTcpSocket *)sender();

    if (RecvMsgFromRemoteDeviceSocket->bytesAvailable() <= 0) {
        return;
    }


    foreach (TcpHelper *tcpHelper, TcpHelperBuffer) {
        if (tcpHelper->Socket == RecvMsgFromRemoteDeviceSocket) {
            tcpHelper->RecvOriginalMsgBuffer.append(RecvMsgFromRemoteDeviceSocket->readAll());
        }
    }
}

void tcpRecvCompareRecordInfo::slotParseMsgFromRemoteDevice()
{
    foreach (TcpHelper *tcpHelper, TcpHelperBuffer) {
        //没有数据包等待解析
        if (tcpHelper->RecvOriginalMsgBuffer.size() == 0) {
            continue;
        }

        while(tcpHelper->RecvOriginalMsgBuffer.size() > 0) {
            int size = tcpHelper->RecvOriginalMsgBuffer.size();

            //寻找帧头的索引
            int FrameHeadIndex = tcpHelper->RecvOriginalMsgBuffer.indexOf("IDOOR");
            if (FrameHeadIndex < 0) {
                break;
            }


            if (size < (FrameHeadIndex + 20)) {
                break;
            }

            //取出xml数据包的长度，不包括帧头的20个字节
            int length = tcpHelper->RecvOriginalMsgBuffer.mid(FrameHeadIndex + 6,14).toUInt();

            //没有收到一个完整的数据包
            if (size < (FrameHeadIndex + 20 + length)) {
                break;
            }

            //取出一个完整的xml数据包,不包括帧头20个字节
            QByteArray VaildCompletePackage = tcpHelper->RecvOriginalMsgBuffer.mid(FrameHeadIndex + 20,length);

            //更新Buffer内容
            tcpHelper->RecvOriginalMsgBuffer = tcpHelper->RecvOriginalMsgBuffer.mid(FrameHeadIndex + 20 + length);

            //保存完整数据包
            tcpHelper->RecvVaildCompleteMsgBuffer.append(VaildCompletePackage);
        }
    }
}

void tcpRecvCompareRecordInfo::slotParseVaildMsgFromRemoteDevice()
{
    foreach (TcpHelper *tcpHelper, TcpHelperBuffer) {
        if (tcpHelper->RecvVaildCompleteMsgBuffer.size() == 0) {
            continue;
        }

        while(tcpHelper->RecvVaildCompleteMsgBuffer.size() > 0) {
            QByteArray data = tcpHelper->RecvVaildCompleteMsgBuffer.takeFirst();

            QDomDocument dom;
            QString errorMsg;
            int errorLine, errorColumn;

            if (!dom.setContent(data, &errorMsg, &errorLine, &errorColumn)) {
                qDebug() << "Parse error: " +  errorMsg << data;
                continue;
            }

            bool isCompareRecordInfo = false;//是否是比对记录信息

            QString CompareRecordID,CompareResult,PersonBuild,PersonUnit,PersonLevel,
                    PersonRoom,PersonName,PersonSex,PersonType,IDCardNumber,PhoneNumber,
                    ExpiryTime,Blacklist,isActivate,FaceSimilarity,UseTime,TriggerTime,
                    EnterSnapPicBase64,OriginalSnapPicBase64;

            QStringList CompareRecordIDList;

            QDomElement RootElement = dom.documentElement();//获取根元素
            if (RootElement.tagName() == "RemoteDevice") { //根元素名称
                QDomNode firstChildNode = RootElement.firstChild();//第一个子节点
                while(!firstChildNode.isNull()){
                    if(firstChildNode.nodeName() == "CompareRecordInfo"){
                        isCompareRecordInfo = true;
                        QDomElement CompareRecordInfo = firstChildNode.toElement();

                        CompareRecordID = CompareRecordInfo.attributeNode("CompareRecordID").value();
                        CompareResult =
                                GlobalConfig::fromBase64(CompareRecordInfo.attributeNode("CompareResult").value());

                        PersonBuild =
                                GlobalConfig::fromBase64(CompareRecordInfo.attributeNode("PersonBuild").value());
                        PersonUnit =
                                GlobalConfig::fromBase64(CompareRecordInfo.attributeNode("PersonUnit").value());
                        PersonLevel =
                                GlobalConfig::fromBase64(CompareRecordInfo.attributeNode("PersonLevel").value());
                        PersonRoom =
                                GlobalConfig::fromBase64(CompareRecordInfo.attributeNode("PersonRoom").value());
                        PersonName =
                                GlobalConfig::fromBase64(CompareRecordInfo.attributeNode("PersonName").value());
                        PersonSex =
                                GlobalConfig::fromBase64(CompareRecordInfo.attributeNode("PersonSex").value());
                        PersonType =
                                GlobalConfig::fromBase64(CompareRecordInfo.attributeNode("PersonType").value());
                        IDCardNumber =
                                GlobalConfig::fromBase64(CompareRecordInfo.attributeNode("IDCardNumber").value());
                        PhoneNumber = CompareRecordInfo.attributeNode("PhoneNumber").value();
                        ExpiryTime = CompareRecordInfo.attributeNode("ExpiryTime").value();
                        Blacklist = CompareRecordInfo.attributeNode("Blacklist").value();
                        isActivate = CompareRecordInfo.attributeNode("isActivate").value();
                        FaceSimilarity = CompareRecordInfo.attributeNode("FaceSimilarity").value();
                        UseTime = CompareRecordInfo.attributeNode("UseTime").value();
                        TriggerTime = CompareRecordInfo.attributeNode("TriggerTime").value();

                        QDomNode firstChildNode = CompareRecordInfo.firstChild();//第一个子节点
                        while(!firstChildNode.isNull()){
                            if(firstChildNode.nodeName() == "EnterSnapPic"){
                                QDomElement firstChildElement = firstChildNode.toElement();
                                EnterSnapPicBase64 = firstChildElement.text();
                            }

                            if(firstChildNode.nodeName() == "OriginalSnapPic"){
                                QDomElement firstChildElement = firstChildNode.toElement();
                                OriginalSnapPicBase64 = firstChildElement.text();
                            }

                            firstChildNode = firstChildNode.nextSibling();//下一个节点
                        }

                        //将人脸比对记录信息保存到数据库中
                        QString EnterSnapPicUrl;
                        if (!EnterSnapPicBase64.isEmpty()) {
                            QImage EnterSnapPic =
                                    CommonSetting::Base64_To_QImage(EnterSnapPicBase64.toLatin1());
                            EnterSnapPicUrl = CommonSetting::GetCurrentPath() + QString("log/") + QString("EnterSnapPic_") + CompareRecordID + QString(".jpg");
                            EnterSnapPic.save(EnterSnapPicUrl,"JPG");
                        }

                        QString OriginalSnapPicUrl;
                        if (!OriginalSnapPicBase64.isEmpty()) {
                            QImage OriginalSnapPic =
                                    CommonSetting::Base64_To_QImage(OriginalSnapPicBase64.toLatin1());
                            OriginalSnapPicUrl = CommonSetting::GetCurrentPath() + QString("log/") + QString("OriginalSnapPic_") + CompareRecordID + QString(".jpg");
                            OriginalSnapPic.save(OriginalSnapPicUrl,"JPG");
                        }

                        QString InsertSql = QString("INSERT INTO compare_record_info_table(CompareRecordID,CompareResult,PersonBuild,PersonUnit,PersonLevel,PersonRoom,PersonName,PersonSex,PersonType,IDCardNumber,PhoneNumber,ExpiryTime,Blacklist,isActivate,FaceSimilarity,UseTime,TriggerTime,EnterSnapPicUrl,OriginalSnapPicUrl,isUploadPlatformCenter) VALUES('%1','%2','%3','%4','%5','%6','%7','%8','%9','%10','%11','%12','%13','%14','%15','%16','%17','%18','%19','%20')").arg(CompareRecordID).arg(CompareResult).arg(PersonBuild).arg(PersonUnit).arg(PersonLevel).arg(PersonRoom).arg(PersonName).arg(PersonSex).arg(PersonType).arg(IDCardNumber).arg(PhoneNumber).arg(ExpiryTime).arg(Blacklist).arg(isActivate).arg(FaceSimilarity).arg(UseTime).arg(TriggerTime).arg(EnterSnapPicUrl).arg(OriginalSnapPicUrl).arg("NO");


                        QSqlQuery query;
                        query.exec(InsertSql);
                        qDebug() << query.lastError();

                        CompareRecordIDList << CompareRecordID;
                    }

                    firstChildNode = firstChildNode.nextSibling();//下一个节点
                }

                if (isCompareRecordInfo) {
                    ACK(tcpHelper,CompareRecordIDList.join(","));
                }
            }
        }
    }
}

void tcpRecvCompareRecordInfo::ACK(TcpHelper *tcpHelper, QString CompareRecordID)
{
    //xml声明
    QString Msg;
    Msg.append("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
    Msg.append(QString("<DbServer NowTime=\"%1\" CompareRecordID=\"%2\" />").arg(CommonSetting::GetCurrentDateTime()).arg(CompareRecordID));

    int length = Msg.toLocal8Bit().size();
    Msg = QString("IDOOR:") + QString("%1").arg(length,14,10,QLatin1Char('0')) + Msg;

    tcpHelper->Socket->write(Msg.toLatin1());
}
