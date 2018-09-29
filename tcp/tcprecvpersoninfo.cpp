#include "tcprecvpersoninfo.h"
#include "tcp/tcpsendpersoninfo.h"

tcpRecvPersonInfo *tcpRecvPersonInfo::instance = NULL;

tcpRecvPersonInfo::tcpRecvPersonInfo(QObject *parent) : QObject(parent)
{

}

void tcpRecvPersonInfo::Listen()
{
    //监听前端设备的tcp连接
    ConnectionListener = new QTcpServer(this);
    connect(ConnectionListener,SIGNAL(newConnection()),this,SLOT(slotProcessVisitorDeviceConnection()));

#if (QT_VERSION > QT_VERSION_CHECK(5,0,0))
    ConnectionListener->listen(QHostAddress::AnyIPv4,GlobalConfig::RecvPersonInfoPort);
#else
    ConnectionListener->listen(QHostAddress::Any,GlobalConfig::RecvPersonInfoPort);
#endif

    //解析前端设备发送过来的数据包
    ParseMsgFromVisitorDeviceTimer = new QTimer(this);
    ParseMsgFromVisitorDeviceTimer->setInterval(100);
    connect(ParseMsgFromVisitorDeviceTimer,SIGNAL(timeout()),this,SLOT(slotParseMsgFromVisitorDevice()));
    ParseMsgFromVisitorDeviceTimer->start();

    //解析前端设备发送过来的数据包
    ParseVaildMsgFromVisitorDeviceTimer = new QTimer(this);
    ParseVaildMsgFromVisitorDeviceTimer->setInterval(100);
    connect(ParseVaildMsgFromVisitorDeviceTimer,SIGNAL(timeout()),this,SLOT(slotParseVaildMsgFromVisitorDevice()));
    ParseVaildMsgFromVisitorDeviceTimer->start();
}

void tcpRecvPersonInfo::slotProcessVisitorDeviceConnection()
{
    QTcpSocket *RecvMsgFromVisitorDeviceSocket = ConnectionListener->nextPendingConnection();

    TcpHelper *tcpHelper = new TcpHelper();
    tcpHelper->Socket = RecvMsgFromVisitorDeviceSocket;
    TcpHelperBuffer.append(tcpHelper);

    if (!RecvMsgFromVisitorDeviceSocket->peerAddress().toString().isEmpty()) {
        connect(RecvMsgFromVisitorDeviceSocket, SIGNAL(readyRead()), this, SLOT(slotRecvMsgFromVisitorDevice()));
        connect(RecvMsgFromVisitorDeviceSocket, SIGNAL(disconnected()), this, SLOT(slotVisitorDeviceDisconnect()));

        qDebug() << QString("VisitorServer Connect:\n\tIP = ") +
                    RecvMsgFromVisitorDeviceSocket->peerAddress().toString() +
                    QString("\n\tPort = ") + QString::number(RecvMsgFromVisitorDeviceSocket->peerPort());
    }
}

void tcpRecvPersonInfo::slotVisitorDeviceDisconnect()
{
    QTcpSocket *RecvMsgFromVisitorDeviceSocket = (QTcpSocket *)sender();

    foreach (TcpHelper *tcpHelper, TcpHelperBuffer) {
        if (tcpHelper->Socket == RecvMsgFromVisitorDeviceSocket) {
            TcpHelperBuffer.removeAll(tcpHelper);
//            delete tcpHelper;
        }
    }

    qDebug() << QString("VisitorServer Disconnect:\n\tIP = ") +
                RecvMsgFromVisitorDeviceSocket->peerAddress().toString() +
                QString("\n\tPort = ") + QString::number(RecvMsgFromVisitorDeviceSocket->peerPort());
}

void tcpRecvPersonInfo::slotRecvMsgFromVisitorDevice()
{
    QTcpSocket *RecvMsgFromVisitorDeviceSocket = (QTcpSocket *)sender();

    if (RecvMsgFromVisitorDeviceSocket->bytesAvailable() <= 0) {
        return;
    }


    foreach (TcpHelper *tcpHelper, TcpHelperBuffer) {
        if (tcpHelper->Socket == RecvMsgFromVisitorDeviceSocket) {
            tcpHelper->RecvOriginalMsgBuffer.append(RecvMsgFromVisitorDeviceSocket->readAll());
        }
    }
}

void tcpRecvPersonInfo::slotParseMsgFromVisitorDevice()
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

void tcpRecvPersonInfo::slotParseVaildMsgFromVisitorDevice()
{
    foreach (TcpHelper *tcpHelper, TcpHelperBuffer) {
        if (tcpHelper->RecvVaildCompleteMsgBuffer.size() == 0) {
            continue;
        }

        while(tcpHelper->RecvVaildCompleteMsgBuffer.size() > 0) {
            QByteArray data = tcpHelper->RecvVaildCompleteMsgBuffer.takeFirst();

            qDebug() << data;

            QDomDocument dom;
            QString errorMsg;
            int errorLine, errorColumn;

            if (!dom.setContent(data, &errorMsg, &errorLine, &errorColumn)) {
                qDebug() << "Parse error: " +  errorMsg << data;
                continue;
            }

            bool isAddPersonInfo = false;//是否是添加人员信息
            bool isPersonInfo = false;
            bool isSelectPersonInfo = false;//是否是查询人员信息
            bool isDeletePersonInfo  = false;//是否是删除人员信息
            bool isUpdatePersonInfo = false;//是否是更新人员信息
            bool isClearPersonInfo  = false;//是否是清空人员信息

            bool isDeviceHeart = false;

            bool isSelectDeviceInfo = false;//是否是查询设备信息
            bool isAddDeviceInfo = false;//是否是增加设备信息
            bool isDelectDeviceInfo = false;//是否是删除设备信息
            bool isUpdateDeviceInfo = false;//是否是更新设备信息
            bool isClearDeviceInfo = false;//是否是清空设备信息

            bool isSelectCompareRecordInfo = false;//是否是进出记录查询
            bool isClearCompareRecordInfo = false;//是否是清空比对记录信息

            bool isGetFaceServerIP   = false;

            bool isSelectAreaInfo = false;//是否是查询区域列表
            bool isAddAreaInfo = false;//是否是添加区域
            bool isDeleteAreaInfo = false;//是否是删除区域
            bool isUpdateAreaInfo = false;//是否是更新区域信息
            bool isClearAreaInfo = false;//是否是情况区域

            QString PersonID,PersonBuild,PersonUnit,PersonLevel,PersonRoom,PersonName,
                    PersonSex,PersonType,IDCardNumber,PhoneNumber,RegisterTime,ExpiryTime,
                    Blacklist,isActivate, FeatureValue,PersonImagBase64,IDCardImageBase64;

            QString DeviceID,DeviceIP,DeviceBuild,DeviceUnit,
                    MainStreamRtspAddr,SubStreamRtspAddr;

            QStringList PersonIDList;

            QString CompareRecordID,TriggerTime,CompareResult;

            QString AreaID,AreaBuild,AreaUnit,AreaLevel,AreaRoom;

            QDomElement RootElement = dom.documentElement();//获取根元素
            if (RootElement.tagName() == "VisitorServer") { //根元素名称
                QDomNode firstChildNode = RootElement.firstChild();//第一个子节点
                while(!firstChildNode.isNull()){
                    //心跳
                    if(firstChildNode.nodeName() == "DeviceHeart"){
                        isDeviceHeart = true;
                    }

                    //人员信息相关
                    if(firstChildNode.nodeName() == "AddPersonInfo"){//人工访客机
                        isAddPersonInfo = true;
                        QDomElement AddPersonInfo = firstChildNode.toElement();

                        PersonBuild  = GlobalConfig::fromBase64(AddPersonInfo.attributeNode("PersonBuild").value());
                        PersonUnit   = GlobalConfig::fromBase64(AddPersonInfo.attributeNode("PersonUnit").value());
                        PersonLevel  = GlobalConfig::fromBase64(AddPersonInfo.attributeNode("PersonLevel").value());
                        PersonRoom   = GlobalConfig::fromBase64(AddPersonInfo.attributeNode("PersonRoom").value());
                        PersonName   = GlobalConfig::fromBase64(AddPersonInfo.attributeNode("PersonName").value());
                        PersonSex    = GlobalConfig::fromBase64(AddPersonInfo.attributeNode("PersonSex").value());
                        PersonType   = GlobalConfig::fromBase64(AddPersonInfo.attributeNode("PersonType").value());
                        IDCardNumber = GlobalConfig::fromBase64(AddPersonInfo.attributeNode("IDCardNumber").value());
                        PhoneNumber  = AddPersonInfo.attributeNode("PhoneNumber").value();
                        RegisterTime = AddPersonInfo.attributeNode("RegisterTime").value();
                        ExpiryTime   = AddPersonInfo.attributeNode("ExpiryTime").value();

                        QDomNode firstChildNode = AddPersonInfo.firstChild();//第一个子节点
                        while(!firstChildNode.isNull()){
                            if(firstChildNode.nodeName() == "FeatureValue"){
                                QDomElement firstChildElement = firstChildNode.toElement();
                                FeatureValue = firstChildElement.text();
                            }

                            if(firstChildNode.nodeName() == "PersonImage"){
                                QDomElement firstChildElement = firstChildNode.toElement();
                                PersonImagBase64 = firstChildElement.text();
                            }

                            if(firstChildNode.nodeName() == "IDCardImage"){
                                QDomElement firstChildElement = firstChildNode.toElement();
                                IDCardImageBase64 = firstChildElement.text();
                            }

                            firstChildNode = firstChildNode.nextSibling();//下一个节点
                        }
                    }

                    if(firstChildNode.nodeName() == "PersonInfo"){//自助访客机
                        isPersonInfo = true;
                        QDomElement PersonInfo = firstChildNode.toElement();

                        PersonBuild  = GlobalConfig::fromBase64(PersonInfo.attributeNode("PersonBuild").value());
                        PersonUnit   = GlobalConfig::fromBase64(PersonInfo.attributeNode("PersonUnit").value());
                        PersonLevel  = GlobalConfig::fromBase64(PersonInfo.attributeNode("PersonLevel").value());
                        PersonRoom   = GlobalConfig::fromBase64(PersonInfo.attributeNode("PersonRoom").value());
                        PersonName   = GlobalConfig::fromBase64(PersonInfo.attributeNode("PersonName").value());
                        PersonSex    = GlobalConfig::fromBase64(PersonInfo.attributeNode("PersonSex").value());
                        PersonType   = GlobalConfig::fromBase64(PersonInfo.attributeNode("PersonType").value());
                        IDCardNumber = GlobalConfig::fromBase64(PersonInfo.attributeNode("IDCardNumber").value());
                        PhoneNumber  = PersonInfo.attributeNode("PhoneNumber").value();
                        RegisterTime = PersonInfo.attributeNode("RegisterTime").value();
                        ExpiryTime   = PersonInfo.attributeNode("ExpiryTime").value();

                        QDomNode firstChildNode = PersonInfo.firstChild();//第一个子节点
                        while(!firstChildNode.isNull()){
                            if(firstChildNode.nodeName() == "FeatureValue"){
                                QDomElement firstChildElement = firstChildNode.toElement();
                                FeatureValue = firstChildElement.text();
                            }

                            if(firstChildNode.nodeName() == "PersonImage"){
                                QDomElement firstChildElement = firstChildNode.toElement();
                                PersonImagBase64 = firstChildElement.text();
                            }

                            if(firstChildNode.nodeName() == "IDCardImage"){
                                QDomElement firstChildElement = firstChildNode.toElement();
                                IDCardImageBase64 = firstChildElement.text();
                            }

                            firstChildNode = firstChildNode.nextSibling();//下一个节点
                        }
                    }

                    if(firstChildNode.nodeName() == "SelectPersonInfo"){
                        isSelectPersonInfo = true;

                        QDomElement SelectPersonInfo = firstChildNode.toElement();

                        PersonID = SelectPersonInfo.attributeNode("PersonID").value();
                        PersonBuild  = GlobalConfig::fromBase64(SelectPersonInfo.attributeNode("PersonBuild").value());
                        PersonUnit   = GlobalConfig::fromBase64(SelectPersonInfo.attributeNode("PersonUnit").value());
                        PersonLevel  = GlobalConfig::fromBase64(SelectPersonInfo.attributeNode("PersonLevel").value());
                        PersonRoom   = GlobalConfig::fromBase64(SelectPersonInfo.attributeNode("PersonRoom").value());
                        PersonName   = GlobalConfig::fromBase64(SelectPersonInfo.attributeNode("PersonName").value());
                    }

                    if(firstChildNode.nodeName() == "DeletePersonInfo"){
                        isDeletePersonInfo = true;

                        QDomElement DeletePersonInfo = firstChildNode.toElement();

                        PersonID = DeletePersonInfo.attributeNode("PersonID").value();
                    }

                    if(firstChildNode.nodeName() == "UpdatePersonInfo"){
                        isUpdatePersonInfo = true;

                        QDomElement UpdatePersonInfo = firstChildNode.toElement();

                        PersonID     = UpdatePersonInfo.attributeNode("PersonID").value();
                        PersonBuild  = GlobalConfig::fromBase64(UpdatePersonInfo.attributeNode("PersonBuild").value());
                        PersonUnit   = GlobalConfig::fromBase64(UpdatePersonInfo.attributeNode("PersonUnit").value());
                        PersonLevel  = GlobalConfig::fromBase64(UpdatePersonInfo.attributeNode("PersonLevel").value());
                        PersonRoom   = GlobalConfig::fromBase64(UpdatePersonInfo.attributeNode("PersonRoom").value());
                        PersonName   = GlobalConfig::fromBase64(UpdatePersonInfo.attributeNode("PersonName").value());
                        PersonSex    = GlobalConfig::fromBase64(UpdatePersonInfo.attributeNode("PersonSex").value());
                        PersonType   = GlobalConfig::fromBase64(UpdatePersonInfo.attributeNode("PersonType").value());
                        IDCardNumber = GlobalConfig::fromBase64(UpdatePersonInfo.attributeNode("IDCardNumber").value());
                        PhoneNumber  = UpdatePersonInfo.attributeNode("PhoneNumber").value();
                        RegisterTime = UpdatePersonInfo.attributeNode("RegisterTime").value();
                        ExpiryTime   = UpdatePersonInfo.attributeNode("ExpiryTime").value();

                        QDomNode firstChildNode = UpdatePersonInfo.firstChild();//第一个子节点
                        while(!firstChildNode.isNull()){
                            if(firstChildNode.nodeName() == "FeatureValue"){
                                QDomElement firstChildElement = firstChildNode.toElement();
                                FeatureValue = firstChildElement.text();
                            }

                            if(firstChildNode.nodeName() == "PersonImage"){
                                QDomElement firstChildElement = firstChildNode.toElement();
                                PersonImagBase64 = firstChildElement.text();
                            }

                            if(firstChildNode.nodeName() == "IDCardImage"){
                                QDomElement firstChildElement = firstChildNode.toElement();
                                IDCardImageBase64 = firstChildElement.text();
                            }

                            firstChildNode = firstChildNode.nextSibling();//下一个节点
                        }
                    }

                    if(firstChildNode.nodeName() == "ClearPersonInfo"){
                        isClearPersonInfo = true;
                    }

                    //设备信息相关
                    if(firstChildNode.nodeName() == "AddDeviceInfo"){
                        isAddDeviceInfo = true;

                        QDomElement AddDeviceInfo = firstChildNode.toElement();

                        DeviceIP = AddDeviceInfo.attributeNode("DeviceIP").value();
                        DeviceBuild  = GlobalConfig::fromBase64(AddDeviceInfo.attributeNode("DeviceBuild").value());
                        DeviceUnit  = GlobalConfig::fromBase64(AddDeviceInfo.attributeNode("DeviceUnit").value());
                        MainStreamRtspAddr = AddDeviceInfo.attributeNode("MainStreamRtspAddr").value();
                        SubStreamRtspAddr = AddDeviceInfo.attributeNode("SubStreamRtspAddr").value();
                    }

                    if(firstChildNode.nodeName() == "SelectDeviceInfo"){
                        isSelectDeviceInfo = true;

                        QDomElement SelectDeviceInfo = firstChildNode.toElement();

                        DeviceIP = SelectDeviceInfo.attributeNode("DeviceIP").value();
                    }

                    if(firstChildNode.nodeName() == "DeleteDeviceInfo"){
                        isDelectDeviceInfo = true;

                        QDomElement DelectDeviceInfo = firstChildNode.toElement();
                        DeviceID = DelectDeviceInfo.attributeNode("DeviceID").value();
                    }

                    if(firstChildNode.nodeName() == "UpdateDeviceInfo"){
                        isUpdateDeviceInfo = true;

                        QDomElement UpdateDeviceInfo = firstChildNode.toElement();

                        DeviceID = UpdateDeviceInfo.attributeNode("DeviceID").value();
                        DeviceBuild  = GlobalConfig::fromBase64(UpdateDeviceInfo.attributeNode("DeviceBuild").value());
                        DeviceUnit  = GlobalConfig::fromBase64(UpdateDeviceInfo.attributeNode("DeviceUnit").value());
                        DeviceIP = UpdateDeviceInfo.attributeNode("DeviceIP").value();
                        MainStreamRtspAddr = UpdateDeviceInfo.attributeNode("MainStreamRtspAddr").value();
                        SubStreamRtspAddr = UpdateDeviceInfo.attributeNode("SubStreamRtspAddr").value();
                    }

                    if(firstChildNode.nodeName() == "ClearDeviceInfo"){
                        isClearDeviceInfo = true;
                    }

                    //比对记录相关
                    if(firstChildNode.nodeName() == "SelectCompareRecordInfo"){
                        isSelectCompareRecordInfo = true;

                        QDomElement SelectCompareRecordInfo = firstChildNode.toElement();

                        CompareRecordID = SelectCompareRecordInfo.attributeNode("CompareRecordID").value();
                        CompareResult = GlobalConfig::fromBase64(SelectCompareRecordInfo.attributeNode("CompareResult").value());
                        PersonBuild  = GlobalConfig::fromBase64(SelectCompareRecordInfo.attributeNode("PersonBuild").value());
                        PersonUnit   = GlobalConfig::fromBase64(SelectCompareRecordInfo.attributeNode("PersonUnit").value());
                        PersonLevel  = GlobalConfig::fromBase64(SelectCompareRecordInfo.attributeNode("PersonLevel").value());
                        PersonRoom   = GlobalConfig::fromBase64(SelectCompareRecordInfo.attributeNode("PersonRoom").value());
                        PersonName   = GlobalConfig::fromBase64(SelectCompareRecordInfo.attributeNode("PersonName").value());
                        PersonType   = GlobalConfig::fromBase64(SelectCompareRecordInfo.attributeNode("PersonType").value());

                        TriggerTime = SelectCompareRecordInfo.attributeNode("TriggerTime").value();
                    }

                    if(firstChildNode.nodeName() == "ClearCompareRecordInfo"){
                        isClearCompareRecordInfo = true;
                    }

                    //区域信息相关
                    if(firstChildNode.nodeName() == "AddAreaInfo"){
                        isAddAreaInfo = true;

                        QDomElement AddAreaInfo = firstChildNode.toElement();

                        AreaBuild = GlobalConfig::fromBase64(
                                    AddAreaInfo.attributeNode("AreaBuild").value());

                        AreaUnit = GlobalConfig::fromBase64(
                                    AddAreaInfo.attributeNode("AreaUnit").value());

                        AreaLevel  = GlobalConfig::fromBase64(
                                    AddAreaInfo.attributeNode("AreaLevel").value());

                        AreaRoom   = GlobalConfig::fromBase64(
                                    AddAreaInfo.attributeNode("AreaRoom").value());
                    }

                    if(firstChildNode.nodeName() == "SelectAreaInfo"){
                        isSelectAreaInfo = true;
                    }

                    if(firstChildNode.nodeName() == "DeleteAreaInfo"){
                        isDeleteAreaInfo = true;

                        QDomElement DeleteAreaInfo = firstChildNode.toElement();

                        AreaID = DeleteAreaInfo.attributeNode("AreaID").value();
                    }

                    if(firstChildNode.nodeName() == "UpdateAreaInfo"){
                        isUpdateAreaInfo = true;

                        QDomElement UpdateAreaInfo = firstChildNode.toElement();

                        AreaID = UpdateAreaInfo.attributeNode("AreaID").value();

                        AreaBuild = GlobalConfig::fromBase64(
                                    UpdateAreaInfo.attributeNode("AreaBuild").value());

                        AreaUnit = GlobalConfig::fromBase64(
                                    UpdateAreaInfo.attributeNode("AreaUnit").value());

                        AreaLevel  = GlobalConfig::fromBase64(
                                    UpdateAreaInfo.attributeNode("AreaLevel").value());

                        AreaRoom   = GlobalConfig::fromBase64(
                                    UpdateAreaInfo.attributeNode("AreaRoom").value());
                    }

                    if(firstChildNode.nodeName() == "ClearAreaInfo"){
                        isClearAreaInfo = true;
                    }

                    firstChildNode = firstChildNode.nextSibling();//下一个节点
                }

                if (isDeviceHeart) {
                    SendDeviceHeart(tcpHelper);
                }

                if (isAddPersonInfo) {//人工访客机
                    AddPersonInfo(QString(""),PersonBuild,PersonUnit,PersonLevel,PersonRoom,PersonName,PersonSex,PersonType,IDCardNumber,PhoneNumber,RegisterTime,ExpiryTime,"NO","YES",FeatureValue,PersonImagBase64,IDCardImageBase64,"NO","NO","NO");
                }

                if (isPersonInfo) {//自助访客机
                    AddPersonInfo(QString(""),PersonBuild,PersonUnit,PersonLevel,PersonRoom,PersonName,PersonSex,PersonType,IDCardNumber,PhoneNumber,RegisterTime,ExpiryTime,"NO","YES",FeatureValue,PersonImagBase64,IDCardImageBase64,"NO","NO","NO");
                }

                if (isSelectPersonInfo) {
                    SelectPersonInfo(tcpHelper,PersonID,PersonBuild,PersonUnit,PersonLevel,PersonRoom,PersonName);
                }

                if (isDeletePersonInfo) {
                    DeletePersonInfo(PersonID);
                }

                if (isUpdatePersonInfo) {
                    UpdatePersonInfo(PersonID, PersonBuild, PersonUnit, PersonLevel, PersonRoom, PersonName, PersonSex, PersonType, IDCardNumber, PhoneNumber, RegisterTime, ExpiryTime, FeatureValue,PersonImagBase64,IDCardImageBase64);
                }

                if (isClearPersonInfo) {
                    ClearPersonInfo();
                }

                if (isAddDeviceInfo) {
                    AddDeviceInfo(DeviceBuild,DeviceUnit,DeviceIP,MainStreamRtspAddr,SubStreamRtspAddr);
                }

                if (isSelectDeviceInfo) {
                    SelectDeviceInfo(tcpHelper,DeviceIP);
                }

                if (isDelectDeviceInfo) {
                    DelectDeviceInfo(DeviceID);
                }

                if (isUpdateDeviceInfo) {
                    UpdateDeviceInfo(DeviceID,DeviceBuild,DeviceUnit,DeviceIP,MainStreamRtspAddr,SubStreamRtspAddr);
                }

                if (isClearDeviceInfo) {
                    ClearDeviceInfo();
                }

                if (isSelectCompareRecordInfo) {
                    SelectCompareRecordInfo(tcpHelper,CompareRecordID,CompareResult,PersonBuild,PersonUnit,PersonLevel,PersonRoom,PersonName,PersonType,TriggerTime);
                }

                if (isClearCompareRecordInfo) {
                    ClearCompareRecordInfo();
                }

                if (isAddAreaInfo) {
                    AddAreaInfo(AreaBuild,AreaUnit,AreaLevel,AreaRoom);
                }

                if (isSelectAreaInfo) {
                    SelectAreaInfo(tcpHelper);
                }

                if (isDeleteAreaInfo) {
                    DeleteAreaInfo(AreaID);
                }

                if (isUpdateAreaInfo) {
                    UpdateAreaInfo(AreaID,AreaBuild,AreaUnit,AreaLevel,AreaRoom);
                }

                if (isClearAreaInfo) {
                    ClearAreaInfo();
                }
            }

            if(RootElement.tagName() == "PlatformCenter"){ //根元素名称
                if(RootElement.hasAttribute("NowTime")){
                    QString NowTime = RootElement.attributeNode("NowTime").value();
                    CommonSetting::SettingSystemDateTime(NowTime);
                }

                if(RootElement.hasAttribute("AgentID")){
                    QString AgentID = RootElement.attributeNode("AgentID").value();
                    if (AgentID == GlobalConfig::AgentID) {
                        QDomNode firstChildNode = RootElement.firstChild();//第一个子节点
                        while(!firstChildNode.isNull()){
                            if(firstChildNode.nodeName() == "GetFaceServerIP "){
                                isGetFaceServerIP  = true;
                            }

                            if(firstChildNode.nodeName() == "AddPersonInfo"){
                                isAddPersonInfo = true;
                                QDomElement AddPersonInfoElement = firstChildNode.toElement();

                                PersonID     =
                                        AddPersonInfoElement.attributeNode("PersonID").value();
                                PersonBuild  =
                                        GlobalConfig::fromBase64(AddPersonInfoElement.attributeNode("PersonBuild").value());
                                PersonUnit   = GlobalConfig::fromBase64(AddPersonInfoElement.attributeNode("PersonUnit").value());
                                PersonLevel  =
                                        GlobalConfig::fromBase64(AddPersonInfoElement.attributeNode("PersonLevel").value());
                                PersonRoom   = GlobalConfig::fromBase64(AddPersonInfoElement.attributeNode("PersonRoom").value());
                                PersonName   = GlobalConfig::fromBase64(AddPersonInfoElement.attributeNode("PersonName").value());
                                PersonSex    = GlobalConfig::fromBase64(AddPersonInfoElement.attributeNode("PersonSex").value());
                                PersonType   = GlobalConfig::fromBase64(AddPersonInfoElement.attributeNode("PersonType").value());
                                IDCardNumber =
                                        GlobalConfig::fromBase64(AddPersonInfoElement.attributeNode("IDCardNumber").value());
                                PhoneNumber  = AddPersonInfoElement.attributeNode("PhoneNumber").value();
                                RegisterTime = AddPersonInfoElement.attributeNode("RegisterTime").value();
                                ExpiryTime   = AddPersonInfoElement.attributeNode("ExpiryTime").value();
                                Blacklist    = AddPersonInfoElement.attributeNode("Blacklist").value();
                                isActivate   = AddPersonInfoElement.attributeNode("isActivate").value();

                                QDomNode firstChildNode = AddPersonInfoElement.firstChild();//第一个子节点
                                while(!firstChildNode.isNull()){
                                    if(firstChildNode.nodeName() == "FeatureValue"){
                                        QDomElement firstChildElement = firstChildNode.toElement();
                                        FeatureValue = firstChildElement.text();
                                    }

                                    if(firstChildNode.nodeName() == "PersonImage"){
                                        QDomElement firstChildElement = firstChildNode.toElement();
                                        PersonImagBase64 = firstChildElement.text();
                                    }

                                    if(firstChildNode.nodeName() == "IDCardImage"){
                                        QDomElement firstChildElement = firstChildNode.toElement();
                                        IDCardImageBase64 = firstChildElement.text();
                                    }

                                    firstChildNode = firstChildNode.nextSibling();//下一个节点
                                }

                                if (!FeatureValue.isEmpty() &&
                                        (FeatureValue.split("|").size() == 256)) {//特征值不能为空
                                    AddPersonInfo(PersonID,PersonBuild,PersonUnit,PersonLevel,PersonRoom,PersonName,PersonSex,PersonType,IDCardNumber,PhoneNumber,RegisterTime,ExpiryTime,Blacklist,isActivate,FeatureValue,PersonImagBase64,IDCardImageBase64,"NO","NO","YES");
                                    PersonIDList << PersonID;
                                }
                            }

                            firstChildNode = firstChildNode.nextSibling();//下一个节点
                        }

                        if (isGetFaceServerIP ) {
                            GetFaceServerIP(tcpHelper);
                        }

                        if (isAddPersonInfo) {//发送反馈信息给平台中心
                            ACKToPlatformCenter(tcpHelper,PersonIDList);
                        }
                    }
                }
            }
        }
    }
}

void tcpRecvPersonInfo::SendData(TcpHelper *tcpHelper, const QString &Body)
{
    QString Msg;
    Msg.append("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
    Msg.append(QString("<DbServer TargetIP=\"%1\" NowTime=\"%2\">").arg(tcpHelper->Socket->peerAddress().toString()).arg(CommonSetting::GetCurrentDateTime()));
    Msg.append(Body);
    Msg.append("</DbServer>");

    int length = Msg.toLocal8Bit().size();
    Msg = QString("IDOOR:") + QString("%1").arg(length,14,10,QLatin1Char('0')) + Msg;
    tcpHelper->Socket->write(Msg.toLatin1());

    qDebug() << Msg;
}

void tcpRecvPersonInfo::SendDeviceHeart(TcpHelper *tcpHelper)
{
    SendData(tcpHelper,QString(""));
}

void tcpRecvPersonInfo::AddPersonInfo(QString PersonID, const QString &PersonBuild, const QString &PersonUnit, const QString &PersonLevel, const QString &PersonRoom, const QString &PersonName, const QString &PersonSex, const QString &PersonType,const QString &IDCardNumber, const QString &PhoneNumber, const QString &RegisterTime, const QString &ExpiryTime, const QString &Blacklist, const QString &isActivate, const QString &FeatureValue, const QString &PersonImageBase64, const QString &IDCardImageBase64 ,const QString &isUploadMainEntrance, const QString &isUploadSubEntrance, const QString &isUploadPlatformCenter)
{
    if (PersonID.isEmpty()) {
        //生成UUID
        QString strId = QUuid::createUuid().toString();
        //"{b5eddbaf-984f-418e-88eb-cf0b8ff3e775}"

        PersonID = strId.remove("{").remove("}").remove("-");
        // "b5eddbaf984f418e88ebcf0b8ff3e775"
    }

    QString PersonImageUrl;
    if (!PersonImageBase64.isEmpty()) {
        QImage PersonImage = CommonSetting::Base64_To_QImage(PersonImageBase64.toLatin1());
        PersonImageUrl = CommonSetting::GetCurrentPath() + QString("images/") + QString("PersonImage_") + PersonID + QString(".jpg");
        PersonImage.save(PersonImageUrl,"JPG");
    }

    QString IDCardImageUrl;
    if (!IDCardImageBase64.isEmpty()) {
        QImage IDCardImage = CommonSetting::Base64_To_QImage(IDCardImageBase64.toLatin1());
        IDCardImageUrl = CommonSetting::GetCurrentPath() + QString("images/") + QString("IDCardImage_") + PersonID + QString(".jpg");
        IDCardImage.save(IDCardImageUrl,"JPG");
    }

    QString InsertSql = QString("INSERT INTO person_info_table(PersonID,PersonBuild,PersonUnit,PersonLevel,PersonRoom,PersonName,PersonSex,PersonType,IDCardNumber,PhoneNumber,RegisterTime,ExpiryTime,FeatureValue,PersonImageUrl,IDCardImageUrl,Blacklist,isActivate,isUploadMainEntrance,isUploadSubEntrance,isUploadPlatformCenter) VALUES('%1','%2','%3','%4','%5','%6','%7','%8','%9','%10','%11','%12','%13','%14','%15','%16','%17','%18','%19','%20')").arg(PersonID).arg(PersonBuild).arg(PersonUnit).arg(PersonLevel).arg(PersonRoom).arg(PersonName).arg(PersonSex).arg(PersonType).arg(IDCardNumber).arg(PhoneNumber).arg(RegisterTime).arg(ExpiryTime).arg(FeatureValue).arg(PersonImageUrl).arg(IDCardImageUrl).arg(Blacklist).arg(isActivate).arg(isUploadMainEntrance).arg(isUploadSubEntrance).arg(isUploadPlatformCenter);

    QSqlQuery InsertQuery;
    InsertQuery.exec(InsertSql);
    qDebug() << InsertSql << InsertQuery.lastError();
}

void tcpRecvPersonInfo::SelectPersonInfo(TcpHelper *tcpHelper,const QString &PersonID,const QString &PersonBuild,const QString &PersonUnit,const QString &PersonLevel,const QString &PersonRoom,const QString &PersonName)
{
    QString SelectSql = QString("SELECT * FROM person_info_table WHERE 1=1");

    if (!PersonID.isEmpty()) {
        SelectSql += QString(" AND PersonID = '%1'").arg(PersonID);
    }

    if (!PersonBuild.isEmpty()) {
        SelectSql += QString(" AND PersonBuild = '%1'").arg(PersonBuild);
    }

    if (!PersonUnit.isEmpty()) {
        SelectSql += QString(" AND PersonUnit = '%1'").arg(PersonUnit);
    }

    if (!PersonLevel.isEmpty()) {
        SelectSql += QString(" AND PersonLevel = '%1'").arg(PersonLevel);
    }

    if (!PersonRoom.isEmpty()) {
        SelectSql += QString(" AND PersonRoom = '%1'").arg(PersonRoom);
    }

    if (!PersonName.isEmpty()) {
        SelectSql += QString(" AND PersonName = '%1'").arg(PersonName);
    }

    SelectSql += QString(" ORDER BY RegisterTime DESC");

    QSqlQuery query;
    query.exec(SelectSql);

    qDebug() << "tcpRecvPersonInfo::SelectPersonInfo" << SelectSql << query.lastError();

    QString Body;
    if (PersonID.isEmpty()) {//获取人员的文字信息
        while (query.next()) {
            QString PersonID = query.value(0).toString();
            QString PersonBuild = query.value(1).toString().toLocal8Bit().toBase64();
            QString PersonUnit = query.value(2).toString().toLocal8Bit().toBase64();
            QString PersonLevel = query.value(3).toString().toLocal8Bit().toBase64();
            QString PersonRoom = query.value(4).toString().toLocal8Bit().toBase64();
            QString PersonName = query.value(5).toString().toLocal8Bit().toBase64();
            QString PersonSex = query.value(6).toString().toLocal8Bit().toBase64();
            QString PersonType = query.value(7).toString().toLocal8Bit().toBase64();

            QString IDCardNumber = query.value(8).toString().toLocal8Bit().toBase64();
            QString PhoneNumber = query.value(9).toString();

            QString RegisterTime = query.value(10).toString();
            QString ExpiryTime = query.value(11).toString();

            QString Blacklist = query.value(15).toString();
            QString isActivate = query.value(16).toString();

            QString isUploadMainEntrance = query.value(17).toString();
            QString isUploadSubEntrance = query.value(18).toString();
            QString isUploadPlatformCenter = query.value(19).toString();

            Body.append(QString("<PersonInfo PersonID=\"%1\" PersonBuild=\"%2\" PersonUnit=\"%3\" PersonLevel=\"%4\" PersonRoom=\"%5\" PersonName=\"%6\" PersonSex=\"%7\" PersonType=\"%8\" IDCardNumber=\"%9\" PhoneNumber=\"%10\"  RegisterTime=\"%11\" ExpiryTime=\"%12\" Blacklist=\"%13\" isActivate=\"%14\" isUploadMainEntrance=\"%15\" isUploadSubEntrance=\"%16\" isUploadPlatformCenter=\"%17\"/>").arg(PersonID).arg(PersonBuild).arg(PersonUnit).arg(PersonLevel).arg(PersonRoom).arg(PersonName).arg(PersonSex).arg(PersonType).arg(IDCardNumber).arg(PhoneNumber).arg(RegisterTime).arg(ExpiryTime).arg(Blacklist).arg(isActivate).arg(isUploadMainEntrance).arg(isUploadSubEntrance).arg(isUploadPlatformCenter));
        }
    } else {//获取人员的特征值和图片信息
        while (query.next()) {
            QString PersonID = query.value(0).toString();
            QString FeatureValue = query.value(12).toString();
            QString PersonImageUrl = query.value(13).toString();
            QString IDCardImageUrl = query.value(14).toString();

            QImage PersonImage(PersonImageUrl);
            QString PersonImageBase64 = CommonSetting::QImage_To_Base64(PersonImage);

            QImage IDCardImage(IDCardImageUrl);
            QString IDCardImageBase64 = CommonSetting::QImage_To_Base64(IDCardImage);

            Body.append(QString("<PersonImage PersonID=\"%1\">").arg(PersonID));
            Body.append(QString("<FeatureValue>%1</FeatureValue>").arg(FeatureValue));
            Body.append(QString("<PersonImage>%1</PersonImage>").arg(PersonImageBase64));
            Body.append(QString("<IDCardImage>%1</IDCardImage>").arg(IDCardImageBase64));
            Body.append(QString("</PersonImage>"));
        }
    }

    SendData(tcpHelper,Body);
}

void tcpRecvPersonInfo::DeletePersonInfo(const QString &PersonID)
{
    //发送命令通知前端设备删除本条人员信息
    if (!PersonID.isEmpty()) {
        //待发送的数据包
        QString Msg;
        Msg.append("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
        Msg.append(QString("<DbServer NowTime=\"%1\">").arg(CommonSetting::GetCurrentDateTime()));
        Msg.append(QString("<DeletePersonInfo PersonID=\"%1\" />").arg(PersonID));
        Msg.append("</DbServer>");

        int length = Msg.toLocal8Bit().size();
        Msg = QString("IDOOR:") + QString("%1").arg(length,14,10,QLatin1Char('0')) + Msg;

        //查询人员所属楼号和单元
        QString SelectSql = QString("SELECT PersonBuild,PersonUnit FROM person_info_table WHERE PersonID = '%1'").arg(PersonID);
        QSqlQuery query;
        query.exec(SelectSql);

        while(query.next()) {
            QString PersonBuild = query.value(0).toString();
            QString PersonUnit = query.value(1).toString();

            QString SelectSql = QString("SELECT DeviceIP FROM device_info_table WHERE DeviceBuild = '%1' AND DeviceUnit = '%2' OR DeviceBuild = '小区大门'").arg(PersonBuild).arg(PersonUnit);
            QSqlQuery query;
            query.exec(SelectSql);

            qDebug() << SelectSql << query.lastError();

            while(query.next()) {
                QString DeviceIP = query.value(0).toString();

                tcpSendPersonInfo *obj = new tcpSendPersonInfo();

                obj->WaitSendMsg = Msg;
                obj->ConnectToHost(DeviceIP,GlobalConfig::SendPersonInfoPort);
            }
        }

        //删除本地记录
        QString DeleteSql = QString("DELETE FROM person_info_table WHERE PersonID = '%1'").arg(PersonID);

        query.exec(DeleteSql);

        qDebug() << "tcpRecvPersonInfo::DeletePersonInfo" << DeleteSql << query.lastError();
    }
}

void tcpRecvPersonInfo::UpdatePersonInfo(const QString &PersonID, const QString &PersonBuild, const QString &PersonUnit, const QString &PersonLevel, const QString &PersonRoom, const QString &PersonName, const QString &PersonSex, const QString &PersonType, const QString &IDCardNumber, const QString &PhoneNumber, const QString &RegisterTime, const QString &ExpiryTime, const QString &FeatureValue, const QString &PersonImageBase64, const QString &IDCardImageBase64)
{
    if (!PersonID.isEmpty()) {
        QString PersonImageUrl;
        if (!PersonImageBase64.isEmpty()) {
            QImage PersonImage = CommonSetting::Base64_To_QImage(PersonImageBase64.toLatin1());
            PersonImageUrl = CommonSetting::GetCurrentPath() + QString("images/") + QString("PersonImage_") + PersonID + QString(".jpg");
            PersonImage.save(PersonImageUrl,"JPG");
        }

        QString IDCardImageUrl;
        if (!IDCardImageBase64.isEmpty()) {
            QImage IDCardImage = CommonSetting::Base64_To_QImage(IDCardImageBase64.toLatin1());
            IDCardImageUrl = CommonSetting::GetCurrentPath() + QString("images/") + QString("IDCardImage_") + PersonID + QString(".jpg");
            IDCardImage.save(IDCardImageUrl,"JPG");
        }

        QString UpdateSql;

        if (FeatureValue.isEmpty()) {
            UpdateSql = QString("UPDATE person_info_table SET PersonBuild = '%1',PersonUnit = '%2',PersonLevel = '%3',PersonRoom = '%4',PersonName = '%5',PersonSex = '%6',PersonType = '%7',IDCardNumber = '%8',PhoneNumber = '%9',RegisterTime = '%10',ExpiryTime = '%11',Blacklist = 'NO',isActivate = 'YES',isUploadMainEntrance = 'NO',isUploadSubEntrance = 'NO',isUploadPlatformCenter = 'NO' WHERE PersonID = '%12'").arg(PersonBuild).arg(PersonUnit).arg(PersonLevel).arg(PersonRoom).arg(PersonName).arg(PersonSex).arg(PersonType).arg(IDCardNumber).arg(PhoneNumber).arg(RegisterTime).arg(ExpiryTime).arg(PersonID);
        } else {
            UpdateSql = QString("UPDATE person_info_table SET PersonBuild = '%1',PersonUnit = '%2',PersonLevel = '%3',PersonRoom = '%4',PersonName = '%5',PersonSex = '%6',PersonType = '%7',IDCardNumber = '%8',PhoneNumber = '%9',RegisterTime = '%10',ExpiryTime = '%11',FeatureValue = '%12',Blacklist = 'NO',isActivate = 'YES',isUploadMainEntrance = 'NO',isUploadSubEntrance = 'NO',isUploadPlatformCenter = 'NO' WHERE PersonID = '%13'").arg(PersonBuild).arg(PersonUnit).arg(PersonLevel).arg(PersonRoom).arg(PersonName).arg(PersonSex).arg(PersonType).arg(IDCardNumber).arg(PhoneNumber).arg(RegisterTime).arg(ExpiryTime).arg(FeatureValue).arg(PersonID);
        }

        QSqlQuery query;
        query.exec(UpdateSql);

        qDebug() << "tcpRecvPersonInfo::UpdatePersonInfo" << UpdateSql << query.lastError();
    }
}

void tcpRecvPersonInfo::ClearPersonInfo()
{
    QString DeleteSql = QString("DELETE FROM person_info_table");

    QSqlQuery query;
    query.exec(DeleteSql);

    qDebug() << "tcpRecvPersonInfo::ClearPersonInfo" << DeleteSql << query.lastError();

    //发送命令通知前端所有设备删除所有人员信息
    //xml声明
    QString Msg;
    Msg.append("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
    Msg.append(QString("<DbServer NowTime=\"%1\">").arg(CommonSetting::GetCurrentDateTime()));
    Msg.append("<ClearPersonInfo />");
    Msg.append("</DbServer>");

    int length = Msg.toLocal8Bit().size();
    Msg = QString("IDOOR:") + QString("%1").arg(length,14,10,QLatin1Char('0')) + Msg;

    QString SelectSql = QString("SELECT DeviceIP FROM device_info_table");
    query.exec(SelectSql);
    qDebug() << "tcpRecvPersonInfo::ClearPersonInfo" << SelectSql << query.lastError();

    while(query.next()) {
        QString DeviceIP = query.value(0).toString();

        tcpSendPersonInfo *obj = new tcpSendPersonInfo();

        obj->WaitSendMsg = Msg;
        obj->ConnectToHost(DeviceIP,GlobalConfig::SendPersonInfoPort);
    }
}

void tcpRecvPersonInfo::AddDeviceInfo(const QString &DeviceBuild,const QString &DeviceUnit,const QString &DeviceIP,const QString &MainStreamRtspAddr,const QString &SubStreamRtspAddr)
{
    if (DeviceBuild.isEmpty() || DeviceUnit.isEmpty() || DeviceIP.isEmpty()) {
        //do nothing
    } else {
        //生成UUID
        QString strId = QUuid::createUuid().toString();
        //"{b5eddbaf-984f-418e-88eb-cf0b8ff3e775}"

        QString DeviceID = strId.remove("{").remove("}").remove("-");
        // "b5eddbaf984f418e88ebcf0b8ff3e775"

        QString InsertSql = QString("INSERT INTO device_info_table(DeviceID,DeviceBuild,DeviceUnit,DeviceIP,MainStreamRtspAddr,SubStreamRtspAddr,isUploadPlatformCenter) VALUES('%1','%2','%3','%4','%5','%6','%7')").arg(DeviceID).arg(DeviceBuild).arg(DeviceUnit).arg(DeviceIP).arg(MainStreamRtspAddr).arg(SubStreamRtspAddr).arg("NO");

        QSqlQuery query;
        query.exec(InsertSql);

        qDebug() << "tcpRecvPersonInfo::AddDeviceInfo" << InsertSql << query.lastError();
    }
}

void tcpRecvPersonInfo::SelectDeviceInfo(TcpHelper *tcpHelper, const QString &DeviceIP)
{
    QString SelectSql;

    if (DeviceIP.isEmpty()) {
        SelectSql = QString("SELECT * FROM device_info_table");
    } else {
        SelectSql = QString("SELECT * FROM device_info_table WHERE DeviceIP = '%1'").arg(DeviceIP);
    }

    QSqlQuery query;
    query.exec(SelectSql);

    qDebug() << "tcpRecvPersonInfo::SelectDeviceInfo = " << SelectSql << query.lastError();

    QStringList DeviceIDList,DeviceBuildList,DeviceUnitList,DeviceIPList,
            MainStreamRtspAddrList,SubStreamRtspAddrList;

    while(query.next()) {
        QString DeviceID = query.value(0).toString();
        QString DeviceBuild = query.value(1).toString().toLocal8Bit().toBase64();
        QString DeviceUnit = query.value(2).toString().toLocal8Bit().toBase64();
        QString DeviceIP = query.value(3).toString();
        QString MainStreamRtspAddr = query.value(4).toString();
        QString SubStreamRtspAddr = query.value(5).toString();

        DeviceIDList << DeviceID;
        DeviceBuildList << DeviceBuild;
        DeviceUnitList << DeviceUnit;
        DeviceIPList << DeviceIP;
        MainStreamRtspAddrList << MainStreamRtspAddr;
        SubStreamRtspAddrList << SubStreamRtspAddr;
    }

    QString Msg;
    int size = DeviceIDList.size();
    for (int i = 0; i < size; i++) {
        Msg.append(QString("<DeviceInfo DeviceID=\"%1\" DeviceBuild=\"%2\" DeviceUnit=\"%3\" DeviceIP=\"%4\" MainStreamRtspAddr=\"%5\" SubStreamRtspAddr=\"%6\" />").arg(DeviceIDList.at(i)).arg(DeviceBuildList.at(i)).arg(DeviceUnitList.at(i)).arg(DeviceIPList.at(i)).arg(MainStreamRtspAddrList.at(i)).arg(SubStreamRtspAddrList.at(i)));
    }

    SendData(tcpHelper,Msg);
}

void tcpRecvPersonInfo::DelectDeviceInfo(const QString &DeviceID)
{
    if (!DeviceID.isEmpty()) {
        QString DeleteSql = QString("DELETE FROM device_info_table WHERE DeviceID = '%1'").arg(DeviceID);

        QSqlQuery query;
        query.exec(DeleteSql);

        qDebug() << "tcpRecvPersonInfo::DelectDeviceInfo" << DeleteSql << query.lastError();
    }
}

void tcpRecvPersonInfo::UpdateDeviceInfo(const QString &DeviceID, const QString &DeviceBuild, const QString &DeviceUnit, const QString &DeviceIP, const QString &MainStreamRtspAddr, const QString &SubStreamRtspAddr)
{
    if (!DeviceID.isEmpty()) {
        QString UpdateSql = QString("UPDATE device_info_table SET DeviceBuild = '%1',DeviceUnit = '%2',DeviceIP = '%3',MainStreamRtspAddr = '%4',SubStreamRtspAddr = '%5',isUploadPlatformCenter = 'NO' WHERE DeviceID = '%6'").arg(DeviceBuild).arg(DeviceUnit).arg(DeviceIP).arg(MainStreamRtspAddr).arg(SubStreamRtspAddr).arg(DeviceID);

        QSqlQuery query;
        query.exec(UpdateSql);

        qDebug() << "tcpRecvPersonInfo::UpdateDeviceInfo" << UpdateSql << query.lastError();
    }
}

void tcpRecvPersonInfo::ClearDeviceInfo()
{
    QString DeleteSql = QString("DELETE FROM device_info_table");

    QSqlQuery query;
    query.exec(DeleteSql);

    qDebug() << "tcpRecvPersonInfo::DelectDeviceInfo" << DeleteSql << query.lastError();
}

void tcpRecvPersonInfo::SelectCompareRecordInfo(TcpHelper *tcpHelper, const QString &CompareRecordID, const QString &CompareResult, const QString &PersonBuild, const QString &PersonUnit, const QString &PersonLevel, const QString &PersonRoom, const QString &PersonName, const QString &PersonType, const QString &TriggerTime)
{
    QString SelectSql = QString("SELECT * FROM compare_record_info_table WHERE 1=1");

    if (!CompareRecordID.isEmpty()) {
        SelectSql += QString(" AND CompareRecordID = '%1'").arg(CompareRecordID);
    }

    if (!CompareResult.isEmpty()) {
        SelectSql += QString(" AND CompareResult = '%1'").arg(CompareResult);
    }

    if (!PersonBuild.isEmpty()) {
        SelectSql += QString(" AND PersonBuild = '%1'").arg(PersonBuild);
    }

    if (!PersonUnit.isEmpty()) {
        SelectSql += QString(" AND PersonUnit = '%1'").arg(PersonUnit);
    }

    if (!PersonLevel.isEmpty()) {
        SelectSql += QString(" AND PersonLevel = '%1'").arg(PersonLevel);
    }

    if (!PersonRoom.isEmpty()) {
        SelectSql += QString(" AND PersonRoom = '%1'").arg(PersonRoom);
    }

    if (!PersonName.isEmpty()) {
        SelectSql += QString(" AND PersonName = '%1'").arg(PersonName);
    }

    if (!PersonType.isEmpty()) {
        SelectSql += QString(" AND PersonType = '%1'").arg(PersonType);
    }

    if (!TriggerTime.isEmpty()) {
        QString StartTime = TriggerTime.split("|").at(0);
        QString EndTime = TriggerTime.split("|").at(1);

        SelectSql += QString(" AND TriggerTime >= '%1' AND TriggerTime <= '%2'").arg(StartTime).arg(EndTime);
    }

    SelectSql += QString(" ORDER BY TriggerTime DESC LIMIT 100 OFFSET 0");

    QSqlQuery query;
    query.exec(SelectSql);

    qDebug() << SelectSql << query.lastError();

    QString Body;
    if (CompareRecordID.isEmpty()) {//获取比对记录文字信息
        while (query.next()) {
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
            QString isUploadPlatformCenter = query.value(19).toString();


            Body.append(QString("<CompareRecordInfo CompareRecordID=\"%1\" CompareResult=\"%2\" PersonBuild=\"%3\" PersonUnit=\"%4\" PersonLevel=\"%5\" PersonRoom=\"%6\" PersonName=\"%7\" PersonSex=\"%8\" PersonType=\"%9\" IDCardNumber=\"%10\" PhoneNumber=\"%11\"  ExpiryTime=\"%12\" Blacklist=\"%13\" isActivate=\"%14\" FaceSimilarity=\"%15\" UseTime=\"%16\" TriggerTime=\"%17\" isUploadPlatformCenter=\"%18\"/>").arg(CompareRecordID).arg(CompareResult).arg(PersonBuild).arg(PersonUnit).arg(PersonLevel).arg(PersonRoom).arg(PersonName).arg(PersonSex).arg(PersonType).arg(IDCardNumber).arg(PhoneNumber).arg(ExpiryTime).arg(Blacklist).arg(isActivate).arg(FaceSimilarity).arg(UseTime).arg(TriggerTime).arg(isUploadPlatformCenter));
        }
    } else {//获取比对记录图片信息
        while (query.next()) {
            QString CompareRecordID = query.value(0).toString();
            QString EnterSnapPicUrl = query.value(17).toString();
            QString OriginalSnapPicUrl = query.value(18).toString();

            QImage EnterSnapImage(EnterSnapPicUrl);
            QString EnterSnapImageBase64 = CommonSetting::QImage_To_Base64(EnterSnapImage);

            QImage OriginalSnapImage(OriginalSnapPicUrl);
            QString OriginalSnapImageBase64 = CommonSetting::QImage_To_Base64(OriginalSnapImage);

            Body.append(QString("<CompareRecordImage CompareRecordID=\"%1\">").arg(CompareRecordID));
            Body.append(QString("<EnterSnapPic>%1</EnterSnapPic>").arg(EnterSnapImageBase64));
            Body.append(QString("<OriginalSnapPic>%1</OriginalSnapPic>").arg(OriginalSnapImageBase64));
            Body.append(QString("</CompareRecordImage>"));
        }
    }

    SendData(tcpHelper,Body);
}

void tcpRecvPersonInfo::ClearCompareRecordInfo()
{
    QString DeleteSql = QString("DELETE FROM compare_record_info_table");

    QSqlQuery query;
    query.exec(DeleteSql);

    qDebug() << "tcpRecvPersonInfo::ClearCompareRecordInfo" << DeleteSql << query.lastError();
}

void tcpRecvPersonInfo::GetFaceServerIP (TcpHelper *tcpHelper)
{
    QString Msg;
    Msg.append("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
    Msg.append(QString("<DbServer AgentID=\"%1\">").arg(GlobalConfig::AgentID));
    Msg.append(QString("<FaceServerIP>%1</FaceServerIP>").arg(GlobalConfig::FaceServerIP));
    Msg.append("</DbServer>");

    int length = Msg.toLocal8Bit().size();
    Msg = QString("IDOOR:") + QString("%1").arg(length,14,10,QLatin1Char('0')) + Msg;
    tcpHelper->Socket->write(Msg.toLatin1());

    qDebug() << Msg;
}

void tcpRecvPersonInfo::ACKToPlatformCenter(TcpHelper *tcpHelper, QStringList PersonIDList)
{
    QString Msg;
    Msg.append("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
    Msg.append(QString("<DbServer AgentID=\"%1\" PersonID=\"%2\" />")
               .arg(GlobalConfig::AgentID).arg(PersonIDList.join(",")));
    int length = Msg.toLocal8Bit().size();
    Msg = QString("IDOOR:") + QString("%1").arg(length,14,10,QLatin1Char('0')) + Msg;
    tcpHelper->Socket->write(Msg.toLatin1());

    qDebug() << Msg;
}

void tcpRecvPersonInfo::AddAreaInfo(const QString &AreaBuild, const QString &AreaUnit, const QString &AreaLevel, const QString &AreaRoom)
{
    if (AreaBuild.isEmpty() || AreaUnit.isEmpty() || AreaLevel.isEmpty() || AreaRoom.isEmpty()) {
        //do nothing
    } else {
        //生成UUID
        QString strId = QUuid::createUuid().toString();
        //"{b5eddbaf-984f-418e-88eb-cf0b8ff3e775}"

        QString AreaID = strId.remove("{").remove("}").remove("-");
        // "b5eddbaf984f418e88ebcf0b8ff3e775"

        QString InsertSql = QString("INSERT INTO area_info_table(AreaID,AreaBuild,AreaUnit,AreaLevel,AreaRoom) VALUES('%1','%2','%3','%4','%5')").arg(AreaID).arg(AreaBuild).arg(AreaUnit).arg(AreaLevel).arg(AreaRoom);

        QSqlQuery query;
        query.exec(InsertSql);

        qDebug() << "添加区域信息";

        if (query.lastError().type() != QSqlError::NoError) {
            qDebug() << query.lastError();
        }
    }
}

void tcpRecvPersonInfo::SelectAreaInfo(TcpHelper *tcpHelper)
{
    QString SelectSql = QString("SELECT * FROM area_info_table");

    QStringList AreaIDList,AreaBuildList,AreaUnitList,AreaLevelList,AreaRoomList;

    QSqlQuery query;
    query.exec(SelectSql);

    qDebug() << "查询区域信息";

    while(query.next()) {
        QString AreaID = query.value(0).toString();
        QString AreaBuild = query.value(1).toString().toLocal8Bit().toBase64();
        QString AreaUnit = query.value(2).toString().toLocal8Bit().toBase64();
        QString AreaLevel = query.value(3).toString().toLocal8Bit().toBase64();
        QString AreaRoom = query.value(4).toString().toLocal8Bit().toBase64();

        AreaIDList << AreaID;
        AreaBuildList << AreaBuild;
        AreaUnitList << AreaUnit;
        AreaLevelList << AreaLevel;
        AreaRoomList << AreaRoom;
    }

    QString Msg;
    int size = AreaIDList.size();
    for (int i = 0; i < size; i++) {
        Msg.append(QString("<AreaInfo AreaID=\"%1\" AreaBuild=\"%2\" AreaUnit=\"%3\" AreaLevel=\"%4\" AreaRoom=\"%5\" />").arg(AreaIDList.at(i)).arg(AreaBuildList.at(i)).arg(AreaUnitList.at(i))
                   .arg(AreaLevelList.at(i)).arg(AreaRoomList.at(i)));
    }

    SendData(tcpHelper,Msg);
}

void tcpRecvPersonInfo::DeleteAreaInfo(const QString &AreaID)
{
    if (!AreaID.isEmpty()) {
        QString DeleteSql = QString("DELETE FROM area_info_table WHERE AreaID = '%1'").arg(AreaID);

        QSqlQuery query;
        query.exec(DeleteSql);

        qDebug() << "删除区域信息";

        if (query.lastError().type() != QSqlError::NoError) {
            qDebug() << query.lastError();
        }
    }
}

void tcpRecvPersonInfo::UpdateAreaInfo(const QString &AreaID, const QString &AreaBuild, const QString &AreaUnit, const QString &AreaLevel, const QString &AreaRoom)
{
    if (!AreaID.isEmpty()) {
        QString UpdateSql = QString("UPDATE area_info_table SET AreaBuild = '%1',AreaUnit = '%2',AreaLevel = '%3',AreaRoom = '%4' WHERE AreaID = '%6'")
                .arg(AreaBuild).arg(AreaUnit).arg(AreaLevel).arg(AreaRoom).arg(AreaID);

        QSqlQuery query;
        query.exec(UpdateSql);

        qDebug() << "更新区域信息";

        if (query.lastError().type() != QSqlError::NoError) {
            qDebug() << query.lastError();
        }
    }
}

void tcpRecvPersonInfo::ClearAreaInfo()
{
    QString DeleteSql = QString("DELETE FROM area_info_table");

    QSqlQuery query;
    query.exec(DeleteSql);

    qDebug() << "清空区域信息表";

    if (query.lastError().type() != QSqlError::NoError) {
        qDebug() << query.lastError();
    }
}
