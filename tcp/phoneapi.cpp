#include "phoneapi.h"

using namespace Phone;

PhoneApi *PhoneApi::instance = NULL;

PhoneApi::PhoneApi(QObject *parent) : QTcpServer(parent)
{

}

void PhoneApi::Listen()
{
#if (QT_VERSION >= QT_VERSION_CHECK(5,0,0))
    this->listen(QHostAddress::AnyIPv4, GlobalConfig::PhonePort);
#else
    this->listen(QHostAddress::Any, GlobalConfig::PhonePort);
#endif
}

#if (QT_VERSION >= QT_VERSION_CHECK(5,0,0))
void PhoneApi::incomingConnection(qintptr socketDescriptor)
#else
void PhoneApi::incomingConnection(int socketDescriptor)
#endif
{
    WorkThread *work_thread = new WorkThread(socketDescriptor,this);
    connect(work_thread,SIGNAL(finished()),work_thread,SLOT(deleteLater()));
    work_thread->start();
}

#if (QT_VERSION >= QT_VERSION_CHECK(5,0,0))
WorkThread::WorkThread(qintptr socketDescriptor, QObject *parent) : QThread(parent)
#else
WorkThread::WorkThread(int socketDescriptor, QObject *parent) : QThread(parent)
#endif
{
    this->socketDescriptor = socketDescriptor;
}

void WorkThread::run()
{
    //注意事项,这里不能传入this指针做为parent,因为this是由主线程创建的
    //而client是由子线程创建的,qt里面不支持跨线程设置parent
    //对象由哪个线程创建的,那么这个对象的槽函数就由该线程来执行
    TcpSocketApi *client = new TcpSocketApi(socketDescriptor);//正确写法

    connect(client,SIGNAL(signalDisconnect()),this,SLOT(quit()));//调用quit,通知线程退出事件循环

    exec();

    CommonSetting::print("tcp work thread quit");
}

#if (QT_VERSION >= QT_VERSION_CHECK(5,0,0))
TcpSocketApi::TcpSocketApi(qintptr socketDescriptor, QObject *parent) : QTcpSocket(parent)
#else
TcpSocketApi::TcpSocketApi(int socketDescriptor, QObject *parent) : QTcpSocket(parent)
#endif
{
    this->setSocketDescriptor(socketDescriptor);

    connect(this,SIGNAL(readyRead()),this,SLOT(slotRecvMsg()));
    connect(this,SIGNAL(disconnected()),this,SLOT(slotDisconnect()));

    ParseOriginalMsgTimer = new QTimer(this);
    ParseOriginalMsgTimer->setInterval(100);
    connect(ParseOriginalMsgTimer,SIGNAL(timeout()),this,SLOT(slotParseOriginalMsg()));
    ParseOriginalMsgTimer->start();

    ParseVaildMsgTimer = new QTimer(this);
    ParseVaildMsgTimer->setInterval(100);
    connect(ParseVaildMsgTimer,SIGNAL(timeout()),this,SLOT(slotParseVaildMsg()));
    ParseVaildMsgTimer->start();

    CheckTcpConnectionTimer = new QTimer(this);
    CheckTcpConnectionTimer->setInterval(30 * 1000);
    connect(CheckTcpConnectionTimer,SIGNAL(timeout()),this,SLOT(slotCheckTcpConnection()));
    CheckTcpConnectionTimer->start();

    SendSnapPicTimer = new QTimer(this);
    SendSnapPicTimer->setInterval(300);
    connect(SendSnapPicTimer,SIGNAL(timeout()),this,SLOT(slotSendSnapPic()));

    SendRegisterPicTimer = new QTimer(this);
    SendRegisterPicTimer->setInterval(300);
    connect(SendRegisterPicTimer,SIGNAL(timeout()),this,SLOT(slotSendRegisterPic()));

    LastRecvMsgTime = QDateTime::currentDateTime();

    connectionName = QString("connectionName%1").arg(socketDescriptor);

    CommonSetting::createConnection(connectionName);
}

TcpSocketApi::~TcpSocketApi()
{
    CommonSetting::closeConnection(connectionName);
}

void TcpSocketApi::SendDeviceHeart()
{
    QString Msg;
    Msg.append("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
    Msg.append(QString("<DbServer TargetIP=\"%1\" NowTime=\"%2\" />").arg(this->peerAddress().toString()).arg(CommonSetting::GetCurrentDateTime()));

    int length = Msg.toLocal8Bit().size();
    Msg = QString("ITEST:") + QString("%1").arg(length,14,10,QLatin1Char('0')) + Msg;
    this->write(Msg.toLatin1());

    CommonSetting::print("返回心跳");
}

void TcpSocketApi::UpdateInsignItemParm(const QString &NodeName, const QString &AttributeName, const QString &Value)
{
    QMutexLocker lock(&GlobalConfig::GlobalLock);

    //执行器IP地址列表
    QStringList InsignItemIPList;

    QString SelectSql = QString("SELECT DeviceIP FROM device_info_table WHERE DeviceBuild != '平台'");

    QSqlQuery query(QSqlDatabase::database(connectionName));
    query.exec(SelectSql);

    while (query.next()) {
        InsignItemIPList << query.value(0).toString();
    }

    QString Msg;

    if (!InsignItemIPList.isEmpty()) {
        //xml声明
        Msg.append("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
        Msg.append(QString("<DbServer NowTime=\"%1\">").arg(CommonSetting::GetCurrentDateTime()));
        Msg.append(QString("<%1 %2=\"%3\" />").arg(NodeName).arg(AttributeName).arg(Value));
        Msg.append("</DbServer>");

        int length = Msg.toLocal8Bit().size();
        Msg = QString("ITEST:") + QString("%1").arg(length,14,10,QLatin1Char('0')) + Msg;
    }

    foreach (QString InsignItemIP, InsignItemIPList) {
        TcpClientApi *client = new TcpClientApi(InsignItemIP,Msg);

        connect(client,SIGNAL(signalUpdateInsignItemParm(QString,QString)),this,SLOT(slotUpdateInsignItemParm(QString,QString)));

        Q_UNUSED(client);
    }
}

void TcpSocketApi::UpdatePicSyncStatus(const QString &PersonID)
{
    QMutexLocker lock(&GlobalConfig::GlobalLock);

    QString UpdateSql = QString("UPDATE pic_info_table SET sync_status = 'Y' WHERE id = '%1'").arg(PersonID);

    QSqlQuery query(QSqlDatabase::database(connectionName));
    query.exec(UpdateSql);

    if (query.lastError().type() != QSqlError::NoError) {
        CommonSetting::print("更新图片同步状态失败 = " + query.lastError().text());
    } else {
        CommonSetting::print("更新图片同步状态成功");
    }
}

void TcpSocketApi::StartService()
{
    QMutexLocker lock(&GlobalConfig::GlobalLock);

    //执行器IP地址列表
    QStringList InsignItemIPList;

    QString SelectSql = QString("SELECT DeviceIP FROM device_info_table WHERE DeviceBuild != '平台'");

    QSqlQuery query(QSqlDatabase::database(connectionName));
    query.exec(SelectSql);

    while (query.next()) {
        InsignItemIPList << query.value(0).toString();
    }

    QString Msg;

    if (!InsignItemIPList.isEmpty()) {
        //xml声明
        Msg.append("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
        Msg.append(QString("<DbServer NowTime=\"%1\">").arg(CommonSetting::GetCurrentDateTime()));
        Msg.append("<StartService />");
        Msg.append("</DbServer>");

        int length = Msg.toLocal8Bit().size();
        Msg = QString("ITEST:") + QString("%1").arg(length,14,10,QLatin1Char('0')) + Msg;
    }

    foreach (QString InsignItemIP, InsignItemIPList) {
        TcpClientApi *client = new TcpClientApi(InsignItemIP,Msg);

        Q_UNUSED(client);
    }
}

void TcpSocketApi::StopService()
{
    QMutexLocker lock(&GlobalConfig::GlobalLock);

    //执行器IP地址列表
    QStringList InsignItemIPList;

    QString SelectSql = QString("SELECT DeviceIP FROM device_info_table WHERE DeviceBuild != '平台'");

    QSqlQuery query(QSqlDatabase::database(connectionName));
    query.exec(SelectSql);

    while (query.next()) {
        InsignItemIPList << query.value(0).toString();
    }

    QString Msg;

    if (!InsignItemIPList.isEmpty()) {
        //xml声明
        Msg.append("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
        Msg.append(QString("<DbServer NowTime=\"%1\">").arg(CommonSetting::GetCurrentDateTime()));
        Msg.append(QString("<StopService />"));
        Msg.append("</DbServer>");

        int length = Msg.toLocal8Bit().size();
        Msg = QString("ITEST:") + QString("%1").arg(length,14,10,QLatin1Char('0')) + Msg;
    }

    foreach (QString InsignItemIP, InsignItemIPList) {
        TcpClientApi *client = new TcpClientApi(InsignItemIP,Msg);

        Q_UNUSED(client);
    }
}

void TcpSocketApi::slotRecvMsg()
{
    if (this->bytesAvailable() <= 0) {
        return;
    }

    QByteArray data = this->readAll();

    QMutexLocker lock(&mutex);

    RecvOriginalMsgBuffer.append(data);

    LastRecvMsgTime = QDateTime::currentDateTime();
}

void TcpSocketApi::slotDisconnect()
{
    emit signalDisconnect();

    this->deleteLater();
}

void TcpSocketApi::slotParseOriginalMsg()
{
    QMutexLocker lock(&mutex);

    while (this->RecvOriginalMsgBuffer.size() > 0) {
        int size = this->RecvOriginalMsgBuffer.size();

        //寻找帧头的索引
        int FrameHeadIndex = this->RecvOriginalMsgBuffer.indexOf("ITEST");

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
    QMutexLocker lock(&mutex);

    while (this->RecvVaildMsgBuffer.size() > 0) {
        QByteArray data = this->RecvVaildMsgBuffer.takeFirst();

        if (data.contains("VisitorClient")) {
            CommonSetting::print("接收数据包:" + data);
        }

        QDomDocument dom;
        QString errorMsg;
        int errorLine, errorColumn;

        if (!dom.setContent(data, &errorMsg, &errorLine, &errorColumn)) {
            qDebug() << "Parse error: " +  errorMsg << data;
            continue;
        }

        QDomElement RootElement = dom.documentElement();//获取根元素

        if (RootElement.tagName() == "VisitorClient") {//来自手机客户端的数据包
            QDomNode firstChildNode = RootElement.firstChild();//第一个子节点

            while (!firstChildNode.isNull()) {
                //心跳
                if (firstChildNode.nodeName() == "DeviceHeart") {
                    CommonSetting::print("接收心跳");

                    SendDeviceHeart();
                }

                //设置执行器比对阀值
                if (firstChildNode.nodeName() == "UpdateThreshold") {
                    QDomElement UpdateThreshold = firstChildNode.toElement();

                    QString Threshold = UpdateThreshold.attribute("Threshold");

                    //遍历数据库中所有的执行器,然后一一更新所有执行器的比对阀值
                    UpdateInsignItemParm("UpdateThreshold","Threshold",Threshold);
                }

                //切换活体检测模式
                if (firstChildNode.nodeName() == "UpdateMode") {
                    QDomElement UpdateMode = firstChildNode.toElement();

                    //0表示正常模式，1表示活体检测
                    QString Mode = UpdateMode.attribute("Mode");

                    //遍历数据库中所有的执行器,然后一一更新所有执行器的比对阀值
                    UpdateInsignItemParm("UpdateMode","Mode",Mode);
                }

                //切换分辨率
                if (firstChildNode.nodeName() == "UpdateRatio") {
                    QDomElement UpdateRatio = firstChildNode.toElement();

                    //Ratio="1080/720/640"
                    QString Ratio = UpdateRatio.attribute("Ratio");
                    UpdateInsignItemParm("UpdateRatio","Ratio",Ratio);
                }

                //更新采集时间
                if (firstChildNode.nodeName() == "UpdateInterval") {
                    QDomElement UpdateInterval = firstChildNode.toElement();

                    //Interval="100/200/300"
                    QString Interval = UpdateInterval.attribute("Interval");
                    UpdateInsignItemParm("UpdateInterval","Interval",Interval);
                }

                //更新最小人脸
                if (firstChildNode.nodeName() == "UpdateMinFace") {
                    QDomElement UpdateMinFace = firstChildNode.toElement();

                    //Interval="100/200/300"
                    QString MinFace = UpdateMinFace.attribute("MinFace");
                    UpdateInsignItemParm("UpdateMinFace","MinFace",MinFace);
                }

                //启动发送图片
                if (firstChildNode.nodeName() == "StartService") {
                    QDomElement startService = firstChildNode.toElement();

                    //register表示录入人员人脸照片，snap表示现场抓拍照片
                    QString Type = startService.attribute("Type");

                    if (Type == "snap") {
                        SendSnapPicTimer->start();
                        StartService();
                    } else if (Type == "register") {
                        SendRegisterPicTimer->start();
                    }
                }

                //停止发送图片
                if (firstChildNode.nodeName() == "StopService") {
                    QDomElement stopService = firstChildNode.toElement();

                    //register表示录入人员人脸照片，snap表示现场抓拍照片
                    QString Type = stopService.attribute("Type");

                    if (Type == "snap") {
                        SendSnapPicTimer->stop();
                        StopService();
                    } else if (Type == "register") {
                        SendRegisterPicTimer->stop();
                    }
                }

                //图片发送成功
                if (firstChildNode.nodeName() == "ReceivePersonImage") {
                    QDomElement ReceivePersonImage = firstChildNode.toElement();

                    QString PersonID = ReceivePersonImage.attribute("PersonID");

                    //标记图片已经发送成功
                    UpdatePicSyncStatus(PersonID);
                }

                firstChildNode = firstChildNode.nextSibling();//下一个节点
            }
        }

        if (RootElement.tagName() == "DoorDevice") {//来自执行器的数据包
            QDomNode firstChildNode = RootElement.firstChild();//第一个子节点

            while (!firstChildNode.isNull()) {
                if (firstChildNode.nodeName() == "PersonImage") {
                    QDomElement PersonImage = firstChildNode.toElement();

                    QString PersonID = PersonImage.attribute("PersonID");

                    QString PersonImageBase64 = PersonImage.text();

                    QImage image =  CommonSetting::Base64_To_QImage(PersonImageBase64.toLatin1());

                    QString url = CommonSetting::GetCurrentPath() + QString("snap/") + QString("PersonImage_") + PersonID + QString(".jpg");

                    image.save(url,"JPG");

                    QMutexLocker lock(&GlobalConfig::GlobalLock);

                    //保存抓拍图片信息到数据库,用于将图片发送给手机
                    QString InsertSql = QString("INSERT INTO pic_info_table VALUES('%1','%2','%3','%4','%5')").arg(PersonID).arg("snap").arg(url).arg("N").arg(CommonSetting::GetCurrentDateTime());

                    QSqlQuery query(QSqlDatabase::database(connectionName));
                    query.exec(InsertSql);

                    if (query.lastError().type() != QSqlError::NoError) {
//                        CommonSetting::print("保存抓拍图片信息到数据库失败 = " + query.lastError().text());
                    } else {
//                        CommonSetting::print("保存抓拍图片信息到数据库成功");
                    }
                }

                firstChildNode = firstChildNode.nextSibling();//下一个节点
            }
        }
    }
}

void TcpSocketApi::slotCheckTcpConnection()
{
    //如果超过1分钟没有收到数据包,主动断开tcp连接
    QDateTime now = QDateTime::currentDateTime();

    if (LastRecvMsgTime.secsTo(now) >= (60)) {
        CommonSetting::print(QString("长时间没有收到[%1]设备的数据包,主动断开tcp连接").arg(this->peerAddress().toString()));
        this->disconnectFromHost();
        this->abort();
    }
}

void TcpSocketApi::slotSendSnapPic()
{
    QMutexLocker lock(&GlobalConfig::GlobalLock);

    SendSnapPicTimer->stop();

    QString SelectSql = QString("SELECT id,type,url FROM pic_info_table WHERE type = 'snap' AND sync_status = 'N' ORDER BY trigger_time ASC LIMIT 1 OFFSET 0");

    QSqlQuery query(QSqlDatabase::database(connectionName));
    query.exec(SelectSql);

    while (query.next()) {
        QString id = query.value(0).toString();
        QString type = query.value(1).toString();
        QString url = query.value(2).toString();

        QImage image(url);
        QString PersonImageBase64 = CommonSetting::QImage_To_Base64(image);

        //xml声明
        QString Msg;
        Msg.append("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
        Msg.append(QString("<DbServer TargetIP=\"%1\" NowTime=\"%2\">").arg(this->peerAddress().toString()).arg(CommonSetting::GetCurrentDateTime()));
        Msg.append(QString("<PersonImage PersonID=\"%1\" PersonType=\"%2\">%3</PersonImage>").arg(id).arg(type).arg(PersonImageBase64));
        Msg.append("</DbServer>");

        int length = Msg.toLocal8Bit().size();
        Msg = QString("ITEST:") + QString("%1").arg(length,14,10,QLatin1Char('0')) + Msg;

        this->write(Msg.toLatin1());

//        CommonSetting::print(QString("发送snap图片"));
    }

    SendSnapPicTimer->start();
}

void TcpSocketApi::slotSendRegisterPic()
{
    QMutexLocker lock(&GlobalConfig::GlobalLock);

    SendRegisterPicTimer->stop();

    QString SelectSql = QString("SELECT id,type,url FROM pic_info_table WHERE type = 'register' AND sync_status = 'N' ORDER BY trigger_time ASC LIMIT 1 OFFSET 0");

    QSqlQuery query(QSqlDatabase::database(connectionName));
    query.exec(SelectSql);

    while (query.next()) {
        QString id = query.value(0).toString();
        QString type = query.value(1).toString();
        QString url = query.value(2).toString();

        QImage image(url);
        QString PersonImageBase64 = CommonSetting::QImage_To_Base64(image);

        //xml声明
        QString Msg;
        Msg.append("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
        Msg.append(QString("<DbServer TargetIP=\"%1\" NowTime=\"%2\">").arg(this->peerAddress().toString()).arg(CommonSetting::GetCurrentDateTime()));
        Msg.append(QString("<PersonImage PersonID=\"%1\" PersonType=\"%2\">%3</PersonImage>").arg(id).arg(type).arg(PersonImageBase64));
        Msg.append("</DbServer>");

        int length = Msg.toLocal8Bit().size();
        Msg = QString("ITEST:") + QString("%1").arg(length,14,10,QLatin1Char('0')) + Msg;

        this->write(Msg.toLatin1());

//        CommonSetting::print(QString("发送register图片"));
    }

    SendRegisterPicTimer->start();
}

void TcpSocketApi::slotUpdateInsignItemParm(const QString &InsignItemIp, const QString &StatusInfo)
{
    //xml声明
    QString Msg;
    Msg.append("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
    Msg.append(QString("<DbServer TargetIP=\"%1\" NowTime=\"%2\" DeviceIP=\"%3\" StatusInfo=\"%4\" />").arg(this->peerAddress().toString()).arg(CommonSetting::GetCurrentDateTime())
               .arg(InsignItemIp).arg(StatusInfo));

    int length = Msg.toLocal8Bit().size();
    Msg = QString("ITEST:") + QString("%1").arg(length,14,10,QLatin1Char('0')) + Msg;

    this->write(Msg.toLatin1());

    CommonSetting::print(Msg);
}

TcpClientApi::TcpClientApi(const QString &InsignItemIp, const QString &WaitSendMsg, QObject *parent) : QTcpSocket(parent)
{
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

    connectToHost(InsignItemIp,GlobalConfig::PhonePort);
}

TcpClientApi::~TcpClientApi()
{

}

void TcpClientApi::slotEstablishConnection()
{
    this->write(this->WaitSendMsg.toLatin1());
}

void TcpClientApi::slotCloseConnection()
{
    this->deleteLater();
}

void TcpClientApi::slotDisplayError(QAbstractSocket::SocketError socketError)
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

    CommonSetting::print(ErrorMsg);

    this->deleteLater();
}

void TcpClientApi::slotRecvMsg()
{
    if (this->bytesAvailable() <= 0) {
        return;
    }

    QByteArray data = this->readAll();

    RecvOriginalMsgBuffer.append(data);
}

void TcpClientApi::slotParseOriginalMsg()
{
    while (this->RecvOriginalMsgBuffer.size() > 0) {
        int size = this->RecvOriginalMsgBuffer.size();

        //寻找帧头的索引
        int FrameHeadIndex = this->RecvOriginalMsgBuffer.indexOf("ITEST");

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

void TcpClientApi::slotParseVaildMsg()
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

        if (RootElement.tagName() == "DoorDevice") {//来自执行器的数据包
            if (RootElement.hasAttribute("StatusInfo")) {
                QString StatusInfo = RootElement.attribute("StatusInfo");

                emit signalUpdateInsignItemParm(InsignItemIp,StatusInfo);

                this->disconnectFromHost();
                this->abort();
            }
        }
    }
}
