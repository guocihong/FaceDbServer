#include "tcpclientapi.h"

using namespace Client;

TcpClientApi::TcpClientApi(OperatorType type, QObject *parent) : QThread(parent)
{
    this->type = type;

    //启动线程
    this->start();
}

TcpClientApi::~TcpClientApi()
{
}

void TcpClientApi::run()
{
    Worker *worker = new Worker(type);

    Q_UNUSED(worker);

    exec();

    CommonSetting::print("TcpClientApi work thread quit");
}

QMutex *Worker::GlobalLock = new QMutex();

Worker::Worker(TcpClientApi::OperatorType type, QObject *parent) : QObject(parent)
{
    this->type = type;

    SendMsgTimer = new QTimer(this);
    SendMsgTimer->setInterval(5000);
    connect(SendMsgTimer,SIGNAL(timeout()),this,SLOT(slotSendMsg()));
    SendMsgTimer->start();

    connectionName = QString("connectionName%1").arg(type);
    CommonSetting::createConnection(connectionName);
}

Worker::~Worker()
{
    CommonSetting::closeConnection(connectionName);
}

void Worker::SendPersonInfo()
{
    QMutexLocker lock(&GlobalConfig::GlobalLock);

    static quint8 loop = 0;
    static quint64 TotalPersonCount = 0;//没有同步成功的总人员数量
    static quint64 PageSize = 5;//页大小
    static quint64 PageNumber = 0;//总页数
    static quint64 PageOffset = 0;//当前第几页

    if (loop == 0) {//统计没有同步成功的总人员数量
        loop++;

        QString SelectSql = QString("SELECT COUNT(*) FROM person_info_table WHERE FeatureValue != '' AND InsignItemIds != '' AND instr(InsignItemSyncStatus, 'N') > 0");

        QSqlQuery query(QSqlDatabase::database(connectionName));
        query.exec(SelectSql);

        while (query.next()) {
            TotalPersonCount = query.value(0).toULongLong();
        }

        //总页数
        PageNumber = TotalPersonCount / PageSize;

        if ((TotalPersonCount % PageSize) != 0) {
            PageNumber++;
        }
    }

    QString SelectSql = QString("SELECT * FROM person_info_table WHERE FeatureValue != '' AND InsignItemIds != '' AND instr(InsignItemSyncStatus, 'N') > 0  ORDER BY RegisterTime DESC LIMIT %1 OFFSET %2").arg(PageSize).arg(PageOffset * PageSize);

//    QString SelectSql = QString("SELECT * FROM person_info_table WHERE FeatureValue != '' AND InsignItemIds != '' AND instr(InsignItemSyncStatus, 'N') > 0  ORDER BY RegisterTime DESC LIMIT 5 OFFSET 0");

    QSqlQuery query(QSqlDatabase::database(connectionName));
    query.exec(SelectSql);

    while (query.next()) {
        QString PersonID = query.value(0).toString();
        QString PersonBuild = CommonSetting::toBase64(query.value(1).toString());
        QString PersonUnit = CommonSetting::toBase64(query.value(2).toString());
        QString PersonFloor = CommonSetting::toBase64(query.value(3).toString());
        QString PersonRoom = CommonSetting::toBase64(query.value(4).toString());
        QString PersonName = CommonSetting::toBase64(query.value(5).toString());
        QString PersonSex = CommonSetting::toBase64(query.value(6).toString());
        QString PersonType = CommonSetting::toBase64(query.value(7).toString());
        QString IDCardNumber = CommonSetting::toBase64(query.value(8).toString());
        QString ICCardNumber = query.value(9).toString();
        QString PhoneNumber = query.value(10).toString();
        QString PhoneNumber2 = query.value(11).toString();
        QString RegisterTime = query.value(12).toString();
        QString ExpiryTime = query.value(13).toString();
        QString Blacklist = query.value(14).toString();
        QString isActivate = query.value(15).toString();
        QStringList InsignItemIdsList = query.value(16).toString().split("|");
        QStringList InsignItemSyncStatusList = query.value(17).toString().split("|");
        QString FeatureValue = query.value(18).toString();
        QString PersonImageUrl = query.value(19).toString();
        QString IDCardImageUrl = query.value(20).toString();

        QString PersonImageBase64;
        if (!PersonImageUrl.isEmpty()) {
            PersonImageBase64 = CommonSetting::QImage_To_Base64(QImage(PersonImageUrl));
        }

        QString IDCardImageBase64;
        if (!IDCardImageUrl.isEmpty()) {
            IDCardImageBase64 = CommonSetting::QImage_To_Base64(QImage(IDCardImageUrl));
        }

        //xml声明
        QString Msg;
        Msg.append("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
        Msg.append(QString("<DbServer NowTime=\"%1\" AgentID=\"%2\">").arg(CommonSetting::GetCurrentDateTime()).arg(GlobalConfig::AgentID));
        Msg.append(QString("<PersonInfo PersonID=\"%1\" PersonBuild=\"%2\" PersonUnit=\"%3\" PersonFloor=\"%4\" PersonRoom=\"%5\" PersonName=\"%6\" PersonSex=\"%7\" PersonType=\"%8\" IDCardNumber=\"%9\" ICCardNumber=\"%10\" PhoneNumber=\"%11\" PhoneNumber2=\"%12\" RegisterTime=\"%13\" ExpiryTime=\"%14\" Blacklist=\"%15\" isActivate=\"%16\">").arg(PersonID).arg(PersonBuild).arg(PersonUnit).arg(PersonFloor).arg(PersonRoom).arg(PersonName).arg(PersonSex).arg(PersonType).arg(IDCardNumber).arg(ICCardNumber).arg(PhoneNumber).arg(PhoneNumber2).arg(RegisterTime).arg(ExpiryTime).arg(Blacklist).arg(isActivate));

        Msg.append(QString("<FeatureValue>%1</FeatureValue>").arg(FeatureValue));
        Msg.append(QString("<PersonImage>%1</PersonImage>").arg(PersonImageBase64));
        Msg.append(QString("<IDCardImage>%1</IDCardImage>").arg(IDCardImageBase64));
        Msg.append(QString("</PersonInfo>"));
        Msg.append("</DbServer>");

        int length = Msg.toLocal8Bit().size();
        Msg = QString("IDOOR:") + QString("%1").arg(length,14,10,QLatin1Char('0')) + Msg;

        //待同步的执行器id和ip列表
        QStringList IdList;
        QStringList IpList;

        //找出需要待同步的执行器IP地址
        int size = InsignItemIdsList.size();

        for (int i = 0; i < size; i++) {
            if (InsignItemSyncStatusList.at(i) == "N") {//没有同步成功
                IdList << InsignItemIdsList.at(i);

                QString SelectSql = QString("SELECT DeviceIP FROM device_info_table WHERE DeviceID = '%1'").arg(InsignItemIdsList.at(i));

                QSqlQuery query(QSqlDatabase::database(connectionName));
                query.exec(SelectSql);

                while (query.next()) {
                    IpList << query.value(0).toString();
                }
            }
        }

        //发送数据包
        size = IdList.size();

        for (int i = 0; i < size; i++) {
            TcpSocketApi *socket = new TcpSocketApi(connectionName,IdList.at(i),IpList.at(i),Msg);

            Q_UNUSED(socket);
        }
    }


    //自增页游标
    PageOffset++;

    if (PageOffset >= PageNumber) {
        loop = 0;

        PageOffset = 0;
    }
}

void Worker::SendCompareRecordInfo()
{
    QMutexLocker lock(&GlobalConfig::GlobalLock);

    //判断是否添加了平台设备,如果没有添加,则不上传比对记录信息;否则,需要上传比对记录信息
    QString PlatformCenterIP;
    PlatformCenterIP.clear();

    QString SelectSql = QString("SELECT DeviceIP FROM device_info_table WHERE DeviceBuild = '平台'");

    QSqlQuery query(QSqlDatabase::database(connectionName));
    query.exec(SelectSql);

    while (query.next()) {
        PlatformCenterIP = query.value(0).toString();
    }

    if (!PlatformCenterIP.isEmpty()) {
        QString SelectSql = QString("SELECT * FROM compare_record_info_table WHERE isUploadPlatformCenter = 'N' ORDER BY TriggerTime DESC LIMIT 5 OFFSET 0");

        QSqlQuery query(QSqlDatabase::database(connectionName));
        query.exec(SelectSql);

        while (query.next()) {
            QString CompareRecordID = query.value(0).toString();
            QString CompareResult = CommonSetting::toBase64(query.value(1).toString());

            QString PersonBuild = CommonSetting::toBase64(query.value(2).toString());
            QString PersonUnit = CommonSetting::toBase64(query.value(3).toString());
            QString PersonFloor = CommonSetting::toBase64(query.value(4).toString());
            QString PersonRoom = CommonSetting::toBase64(query.value(5).toString());
            QString PersonName = CommonSetting::toBase64(query.value(6).toString());
            QString PersonSex = CommonSetting::toBase64(query.value(7).toString());
            QString PersonType = CommonSetting::toBase64(query.value(8).toString());
            QString IDCardNumber = CommonSetting::toBase64(query.value(9).toString());
            QString ICCardNumber = query.value(10).toString();
            QString PhoneNumber = query.value(11).toString();
            QString ExpiryTime = query.value(12).toString();
            QString Blacklist = query.value(13).toString();
            QString isActivate = query.value(14).toString();
            QString FaceSimilarity = query.value(15).toString();
            QString UseTime = query.value(16).toString();
            QString TriggerTime = query.value(17).toString();
            QString EnterSnapPicUrl = query.value(18).toString();
            QString OriginalSnapPicUrl = query.value(19).toString();

            QString EnterSnapPicBase64;
            if (!EnterSnapPicUrl.isEmpty()) {
                EnterSnapPicBase64 = CommonSetting::QImage_To_Base64(QImage(EnterSnapPicUrl));
            }

            QString OriginalSnapPicBase64;
            if (!OriginalSnapPicUrl.isEmpty()) {
                OriginalSnapPicBase64 = CommonSetting::QImage_To_Base64(QImage(OriginalSnapPicUrl));
            }

            //xml声明
            QString Msg;
            Msg.append("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
            Msg.append(QString("<DbServer AgentID=\"%1\">").arg(GlobalConfig::AgentID));
            Msg.append(QString("<CompareRecordInfo CompareRecordID=\"%1\" CompareResult=\"%2\" PersonBuild=\"%3\" PersonUnit=\"%4\" PersonFloor=\"%5\" PersonRoom=\"%6\" PersonName=\"%7\" PersonSex=\"%8\" PersonType=\"%9\" IDCardNumber=\"%10\" ICCardNumber=\"%11\" PhoneNumber=\"%12\" ExpiryTime=\"%13\" Blacklist=\"%14\" isActivate=\"%15\" FaceSimilarity=\"%16\" UseTime=\"%17\" TriggerTime=\"%18\">").arg(CompareRecordID).arg(CompareResult).arg(PersonBuild).arg(PersonUnit)
                       .arg(PersonFloor).arg(PersonRoom).arg(PersonName).arg(PersonSex)
                       .arg(PersonType).arg(IDCardNumber).arg(ICCardNumber).arg(PhoneNumber)
                       .arg(ExpiryTime).arg(Blacklist).arg(isActivate).arg(FaceSimilarity)
                       .arg(UseTime).arg(TriggerTime));
            Msg.append(QString("<EnterSnapPic>%1</EnterSnapPic>").arg(EnterSnapPicBase64));
            Msg.append(QString("<OriginalSnapPic>%1</OriginalSnapPic>").arg(OriginalSnapPicBase64));
            Msg.append(QString("</CompareRecordInfo>"));
            Msg.append("</DbServer>");

            int length = Msg.toLocal8Bit().size();
            Msg = QString("IDOOR:") + QString("%1").arg(length,14,10,QLatin1Char('0')) + Msg;

            //发送数据包
            TcpSocketApi *socket = new TcpSocketApi(connectionName,"",PlatformCenterIP,Msg);

            Q_UNUSED(socket);
        }
    }
}

void Worker::SendDeviceInfo()
{
    QMutexLocker lock(&GlobalConfig::GlobalLock);

    //判断是否添加了平台设备,如果没有添加,则不上传设备信息;否则,需要上传设备信息
    QString PlatformCenterIP;
    PlatformCenterIP.clear();

    QString SelectSql = QString("SELECT DeviceIP FROM device_info_table WHERE DeviceBuild = '平台'");

    QSqlQuery query(QSqlDatabase::database(connectionName));
    query.exec(SelectSql);

    while (query.next()) {
        PlatformCenterIP = query.value(0).toString();
    }

    if (!PlatformCenterIP.isEmpty()) {
        //xml声明
        QString Msg;
        Msg.append("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
        Msg.append(QString("<DbServer AgentID=\"%1\">").arg(GlobalConfig::AgentID));

        bool flag = false;

        QString SelectSql = QString("SELECT * FROM device_info_table WHERE isUploadPlatformCenter = 'N'");

        QSqlQuery query(QSqlDatabase::database(connectionName));
        query.exec(SelectSql);

        while (query.next()) {
            flag = true;

            QString DeviceID = query.value(0).toString();
            QString DeviceBuild = CommonSetting::toBase64(query.value(1).toString());
            QString DeviceUnit = CommonSetting::toBase64(query.value(2).toString());
            QString DeviceIP = query.value(3).toString();
            QString Longitude = query.value(4).toString();
            QString Latitude = query.value(5).toString();
            QString Altitude = query.value(6).toString();
            QString MainStreamRtspAddr = query.value(7).toString();
            QString SubStreamRtspAddr = query.value(8).toString();

            Msg.append(QString("<DeviceInfo DeviceID=\"%1\" DeviceBuild=\"%2\" DeviceUnit=\"%3\" DeviceIP=\"%4\" Longitude=\"%5\" Latitude=\"%6\" Altitude=\"%7\" MainStreamRtspAddr=\"%8\" SubStreamRtspAddr=\"%9\" />").arg(DeviceID).arg(DeviceBuild).arg(DeviceUnit).arg(DeviceIP)
                       .arg(Longitude).arg(Latitude).arg(Altitude).arg(MainStreamRtspAddr)
                       .arg(SubStreamRtspAddr));
        }

        if (flag) {
            Msg.append("</DbServer>");

            int length = Msg.toLocal8Bit().size();
            Msg = QString("IDOOR:") + QString("%1").arg(length,14,10,QLatin1Char('0')) + Msg;

            //发送数据包
            TcpSocketApi *socket = new TcpSocketApi(connectionName,"",PlatformCenterIP,Msg);

            Q_UNUSED(socket);
        }
    }
}

void Worker::slotSendMsg()
{
    QMutexLocker lock(GlobalLock);

    SendMsgTimer->stop();

    if (type == TcpClientApi::sendPersonInfo) {//发送人员信息
        SendPersonInfo();
    } else if (type == TcpClientApi::sendCompareRecordInfo) {//发送比对记录信息
        SendCompareRecordInfo();
    } else if (type == TcpClientApi::sendDeviceInfo) {//发送执行器设备信息
        SendDeviceInfo();
    }

    SendMsgTimer->start();
}

TcpSocketApi::TcpSocketApi(const QString &connectionName, const QString &InsignItemId, const QString &InsignItemIp, const QString &WaitSendMsg, QObject *parent) : QTcpSocket(parent)
{
    this->connectionName = connectionName;
    this->InsignItemId = InsignItemId;
    this->InsignItemIp = InsignItemIp;
    this->WaitSendMsg = WaitSendMsg;

    connect(this,SIGNAL(connected()),this,SLOT(slotEstablishConnection()));
    connect(this,SIGNAL(disconnected()),this,SLOT(slotCloseConnection()));
    connect(this,SIGNAL(error(QAbstractSocket::SocketError)),this,SLOT(slotDisplayError(QAbstractSocket::SocketError)));
    connect(this,SIGNAL(readyRead()),this,SLOT(slotRecvMsg()));

    ParseOriginalMsgTimer = new QTimer(this);
    ParseOriginalMsgTimer->setInterval(100);
    connect(ParseOriginalMsgTimer,SIGNAL(timeout()),this,SLOT(slotParseOriginalMsg()));
    ParseOriginalMsgTimer->start();

    ParseVaildMsgTimer = new QTimer(this);
    ParseVaildMsgTimer->setInterval(100);
    connect(ParseVaildMsgTimer,SIGNAL(timeout()),this,SLOT(slotParseVaildMsg()));
    ParseVaildMsgTimer->start();

    connectToHost(InsignItemIp,GlobalConfig::SendPort);
}

void TcpSocketApi::UpdatePersonSyncStatus(const QString &PersonID)
{
    QMutexLocker lock(&GlobalConfig::GlobalLock);

    QStringList InsignItemIdsList;
    QStringList InsignItemSyncStatusList;

    QString SelectSql = QString("SELECT InsignItemIds,InsignItemSyncStatus FROM person_info_table WHERE PersonID = '%1'").arg(PersonID);

    QSqlQuery query(QSqlDatabase::database(connectionName));
    query.exec(SelectSql);

    while (query.next()) {
        InsignItemIdsList = query.value(0).toString().split("|");
        InsignItemSyncStatusList = query.value(1).toString().split("|");
    }

    int size = InsignItemIdsList.size();

    for (int i = 0; i < size; i++) {
        if (InsignItemIdsList.at(i) == InsignItemId) {
            InsignItemSyncStatusList.replace(i,"Y");
            break;
        }
    }

    QString InsignItemSyncStatus = InsignItemSyncStatusList.join("|");

    QString UpdateSql = QString("UPDATE person_info_table SET InsignItemSyncStatus = '%1' WHERE PersonID = '%2'").arg(InsignItemSyncStatus).arg(PersonID);

    query.exec(UpdateSql);

    if (query.lastError().type() == QSqlError::NoError) {
        CommonSetting::print(QString("执行器[%1] = 人员信息同步成功").arg(InsignItemIp));
    } else {
        CommonSetting::print(QString("执行器[%1] = 人员信息同步失败 %2").arg(InsignItemIp).arg(query.lastError().text()));
    }

    this->disconnectFromHost();
    this->abort();
}

void TcpSocketApi::UpdateCompareRecordSyncStatus(const QString &CompareRecordID)
{
    QMutexLocker lock(&GlobalConfig::GlobalLock);

    QString UpdateSql = QString("UPDATE compare_record_info_table SET isUploadPlatformCenter = 'Y' WHERE CompareRecordID = '%1'").arg(CompareRecordID);

    QSqlQuery query(QSqlDatabase::database(connectionName));
    query.exec(UpdateSql);

    if (query.lastError().type() == QSqlError::NoError) {
        CommonSetting::print("比对记录信息同步成功");
    } else {
        CommonSetting::print(QString("比对记录信息同步失败 = %1").arg(query.lastError().text()));
    }

    this->disconnectFromHost();
    this->abort();
}

void TcpSocketApi::UpdateDeviceSyncStatus(const QString &DeviceID)
{
    QMutexLocker lock(&GlobalConfig::GlobalLock);

    QStringList DeviceIDList = DeviceID.split(",");

    foreach (QString id, DeviceIDList) {
        QString UpdateSql = QString("UPDATE device_info_table SET isUploadPlatformCenter = 'Y' WHERE DeviceID = '%1'").arg(id);

        QSqlQuery query(QSqlDatabase::database(connectionName));
        query.exec(UpdateSql);

        if (query.lastError().type() == QSqlError::NoError) {
            CommonSetting::print("设备信息同步成功");
        } else {
            CommonSetting::print(QString("设备信息同步失败 = %1").arg(query.lastError().text()));
        }
    }

    this->disconnectFromHost();
    this->abort();
}

void TcpSocketApi::slotEstablishConnection()
{
    this->write(this->WaitSendMsg.toLatin1());
}

void TcpSocketApi::slotCloseConnection()
{
    this->deleteLater();
}

void TcpSocketApi::slotDisplayError(QAbstractSocket::SocketError socketError)
{
    QString ErrorMsg;

    switch(socketError){
    case QAbstractSocket::ConnectionRefusedError:
        ErrorMsg = "QAbstractSocket::ConnectionRefusedError";
        break;
    case QAbstractSocket::RemoteHostClosedError:
        ErrorMsg = "QAbstractSocket::RemoteHostClosedError";
        break;
    case QAbstractSocket::HostNotFoundError:
        ErrorMsg = "QAbstractSocket::HostNotFoundError";
        break;
    default:
        ErrorMsg =  "The following error occurred:" + this->errorString();
        break;
    }

    CommonSetting::print(QString("设备[%1] = ").arg(InsignItemIp) + ErrorMsg);

    this->deleteLater();
}

void TcpSocketApi::slotRecvMsg()
{
    if (this->bytesAvailable() <= 0) {
        return;
    }

    QByteArray data = this->readAll();

    RecvOriginalMsgBuffer.append(data);
}

void TcpSocketApi::slotParseOriginalMsg()
{
    while (this->RecvOriginalMsgBuffer.size() > 0) {
        int size = this->RecvOriginalMsgBuffer.size();

        //寻找帧头的索引
        int FrameHeadIndex = this->RecvOriginalMsgBuffer.indexOf("IDOOR");

        if (FrameHeadIndex < 0) {
            break;
        }

        if (size < (FrameHeadIndex + 20)) {
            break;
        }

        //取出xml数据包的长度，不包括帧头的20个字节
        int length = this->RecvOriginalMsgBuffer.mid(FrameHeadIndex + 6,14).toUInt();

        //没有收到一个完整的数据包
        if (size < (FrameHeadIndex + 20 + length)) {
            break;
        }

        //取出一个完整的xml数据包,不包括帧头20个字节
        QByteArray VaildCompletePackage = this->RecvOriginalMsgBuffer.mid(FrameHeadIndex + 20,length);

        //更新Buffer内容
        this->RecvOriginalMsgBuffer = this->RecvOriginalMsgBuffer.mid(FrameHeadIndex + 20 + length);

        //保存完整数据包
        this->RecvVaildMsgBuffer.append(VaildCompletePackage);
    }
}

void TcpSocketApi::slotParseVaildMsg()
{
    while (this->RecvVaildMsgBuffer.size() > 0) {
        QByteArray data = this->RecvVaildMsgBuffer.takeFirst();

        QDomDocument dom;
        QString errorMsg;
        int errorLine, errorColumn;

        if (!dom.setContent(data, &errorMsg, &errorLine, &errorColumn)) {
            qDebug() << "Parse error: " +  errorMsg << data;
            continue;
        }

        QDomElement RootElement = dom.documentElement();//获取根元素

        if (RootElement.tagName() == "PlatformCenter") {//来自敏达平台的数据包
            if (RootElement.hasAttribute("NowTime")) {
                QDateTime now = QDateTime::currentDateTime();
                if (GlobalConfig::LastUpdateDateTime.secsTo(now) >= (30 * 60)) {
                    QString NowTime = RootElement.attribute("NowTime");
                    CommonSetting::SettingSystemDateTime(NowTime);

                    GlobalConfig::LastUpdateDateTime = now;
                }
            }

            if (RootElement.hasAttribute("AgentID")) {
                QString AgentID = RootElement.attribute("AgentID");
                if (AgentID == GlobalConfig::AgentID) {
                    if (RootElement.hasAttribute("PersonID")) {
                        QString PersonID = RootElement.attribute("PersonID");

                        UpdatePersonSyncStatus(PersonID);
                    }

                    if (RootElement.hasAttribute("CompareRecordID")) {
                        QString CompareRecordID = RootElement.attribute("CompareRecordID");

                        UpdateCompareRecordSyncStatus(CompareRecordID);
                    }

                    if (RootElement.hasAttribute("DeviceID")) {
                        QString DeviceID = RootElement.attribute("DeviceID");

                        UpdateDeviceSyncStatus(DeviceID);
                    }
                }
            }
        }

        if (RootElement.tagName() == "DoorDevice") {//来自执行器的数据包
            if (RootElement.hasAttribute("PersonID")) {
                QString PersonID = RootElement.attribute("PersonID");

                UpdatePersonSyncStatus(PersonID);
            }
        }
    }
}
