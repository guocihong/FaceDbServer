#include "tcpserverapi.h"

using namespace Server;

TcpServerApi *TcpServerApi::instance = NULL;

TcpServerApi::TcpServerApi(QObject *parent) : QTcpServer(parent)
{

}

void TcpServerApi::Listen()
{
#if (QT_VERSION >= QT_VERSION_CHECK(5,0,0))
    this->listen(QHostAddress::AnyIPv4, GlobalConfig::RecvPort);
#else
    this->listen(QHostAddress::Any, GlobalConfig::RecvPort);
#endif
}

#if (QT_VERSION >= QT_VERSION_CHECK(5,0,0))
void TcpServerApi::incomingConnection(qintptr socketDescriptor)
#else
void TcpServerApi::incomingConnection(int socketDescriptor)
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

    //    TcpSocketApi *client = new TcpSocketApi(socketDescriptor,this);//错误写法

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

    LastRecvMsgTime = QDateTime::currentDateTime();

    connectionName = QString("connectionName%1").arg(socketDescriptor);

    CommonSetting::createConnection(connectionName);

    CommonSetting::print("数据库连接名 = " + connectionName + ",设备地址 = " + this->peerAddress().toString());
}

TcpSocketApi::~TcpSocketApi()
{
    CommonSetting::closeConnection(connectionName);
}

void TcpSocketApi::SendData(const QString &Body)
{
    QString Msg;
    Msg.append("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
    Msg.append(QString("<DbServer TargetIP=\"%1\" NowTime=\"%2\">").arg(this->peerAddress().toString()).arg(CommonSetting::GetCurrentDateTime()));
    Msg.append(Body);
    Msg.append("</DbServer>");

    int length = Msg.toLocal8Bit().size();
    Msg = QString("IDOOR:") + QString("%1").arg(length,14,10,QLatin1Char('0')) + Msg;
    this->write(Msg.toLatin1());
}

void TcpSocketApi::SendDeviceHeart()
{
    SendData(QString(""));

    CommonSetting::print("返回心跳");
}

void TcpSocketApi::AddPersonInfo(PersonInfo &person)
{
    QMutexLocker lock(&GlobalConfig::GlobalLock);

    //根据楼栋和单元找出本条人员信息需要绑定的执行器
    QStringList ids,syncs;

    QString SelectSql = QString("SELECT DeviceID FROM device_info_table WHERE DeviceBuild = '小区出入口' OR DeviceBuild = '平台' OR DeviceBuild = '%1'").arg(person.PersonBuild);

    QSqlQuery query(QSqlDatabase::database(connectionName));
    query.exec(SelectSql);

    while (query.next()) {
        QString DeviceID = query.value(0).toString();
        ids << DeviceID;
        syncs << "N";
    }

    person.InsignItemIds = ids.join("|");
    person.InsignItemSyncStatus = syncs.join("|");

    //保存人员信息到数据库
    QString InsertSql = QString("INSERT INTO person_info_table VALUES('%1','%2','%3','%4','%5','%6','%7','%8','%9','%10','%11','%12','%13','%14','%15','%16','%17','%18','%19','%20','%21')").arg(person.PersonID).arg(person.PersonBuild).arg(person.PersonUnit).arg(person.PersonFloor).arg(person.PersonRoom).arg(person.PersonName).arg(person.PersonSex).arg(person.PersonType).arg(person.IDCardNumber).arg(person.ICCardNumber).arg(person.PhoneNumber).arg(person.PhoneNumber2).arg(person.RegisterTime).arg(person.ExpiryTime).arg(person.Blacklist).arg(person.isActivate).arg(person.InsignItemIds).arg(person.InsignItemSyncStatus).arg(person.FeatureValue).arg(person.PersonImageUrl).arg(person.IDCardImageUrl);

    query.exec(InsertSql);

    if (query.lastError().type() != QSqlError::NoError) {
        CommonSetting::print("添加人员信息失败 = " + query.lastError().text());
    } else {
        CommonSetting::print("添加人员信息成功");
    }

#ifdef jiance
    //保存注册图片信息到数据库,用于将图片发送给手机
    InsertSql = QString("INSERT INTO pic_info_table VALUES('%1','%2','%3','%4','%5')").arg(person.PersonID).arg("register").arg(person.PersonImageUrl).arg("N").arg(CommonSetting::GetCurrentDateTime());

    query.exec(InsertSql);

    if (query.lastError().type() != QSqlError::NoError) {
        CommonSetting::print("保存注册图片信息到数据库失败 = " + query.lastError().text());
    } else {
        CommonSetting::print("保存注册图片信息到数据库成功");
    }
#endif
}

void TcpSocketApi::AddPersonInfoFromPlatformCenter(PersonInfo &person)
{
    QMutexLocker lock(&GlobalConfig::GlobalLock);

    //根据楼栋和单元找出本条人员信息需要绑定的执行器
    QStringList ids,syncs;

    QString SelectSql = QString("SELECT DeviceID FROM device_info_table WHERE DeviceBuild = '小区出入口' OR DeviceBuild = '%1'").arg(person.PersonBuild);

    QSqlQuery query(QSqlDatabase::database(connectionName));
    query.exec(SelectSql);

    while (query.next()) {
        QString DeviceID = query.value(0).toString();
        ids << DeviceID;
        syncs << "N";
    }

    //不需要将本条人员信息发送到平台
    SelectSql = QString("SELECT DeviceID FROM device_info_table WHERE DeviceBuild = '平台'");

    query.exec(SelectSql);

    while (query.next()) {
        QString DeviceID = query.value(0).toString();
        ids << DeviceID;
        syncs << "Y";
    }

    person.InsignItemIds = ids.join("|");
    person.InsignItemSyncStatus = syncs.join("|");

    //保存人员信息到数据库
    QString InsertSql = QString("INSERT INTO person_info_table VALUES('%1','%2','%3','%4','%5','%6','%7','%8','%9','%10','%11','%12','%13','%14','%15','%16','%17','%18','%19','%20','%21')").arg(person.PersonID).arg(person.PersonBuild).arg(person.PersonUnit).arg(person.PersonFloor).arg(person.PersonRoom).arg(person.PersonName).arg(person.PersonSex).arg(person.PersonType).arg(person.IDCardNumber).arg(person.ICCardNumber).arg(person.PhoneNumber).arg(person.PhoneNumber2).arg(person.RegisterTime).arg(person.ExpiryTime).arg(person.Blacklist).arg(person.isActivate).arg(person.InsignItemIds).arg(person.InsignItemSyncStatus).arg(person.FeatureValue).arg(person.PersonImageUrl).arg(person.IDCardImageUrl);

    query.exec(InsertSql);

    if (query.lastError().type() != QSqlError::NoError) {
        CommonSetting::print("添加人员信息失败 = " + query.lastError().text());
    } else {
        CommonSetting::print("添加人员信息成功");
    }
}

void TcpSocketApi::SelectPersonInfo(const PersonInfo &person,PersonPageInfo &person_page)
{
    QMutexLocker lock(&GlobalConfig::GlobalLock);

    QString SelectSqlCount = QString("SELECT COUNT(*) FROM person_info_table WHERE 1=1");
    QString SelectSql = QString("SELECT * FROM person_info_table WHERE 1=1");

    if (!person.PersonID.isEmpty()) {
        SelectSql += QString(" AND PersonID = '%1'").arg(person.PersonID);
    }

    if (!person.PersonBuild.isEmpty()) {
        SelectSqlCount += QString(" AND PersonBuild = '%1'").arg(person.PersonBuild);
        SelectSql += QString(" AND PersonBuild = '%1'").arg(person.PersonBuild);
    }

    if (!person.PersonUnit.isEmpty()) {
        SelectSqlCount += QString(" AND PersonUnit = '%1'").arg(person.PersonUnit);
        SelectSql += QString(" AND PersonUnit = '%1'").arg(person.PersonUnit);
    }

    if (!person.PersonFloor.isEmpty()) {
        SelectSqlCount += QString(" AND PersonFloor = '%1'").arg(person.PersonFloor);
        SelectSql += QString(" AND PersonFloor = '%1'").arg(person.PersonFloor);
    }

    if (!person.PersonRoom.isEmpty()) {
        SelectSqlCount += QString(" AND PersonRoom = '%1'").arg(person.PersonRoom);
        SelectSql += QString(" AND PersonRoom = '%1'").arg(person.PersonRoom);
    }

    if (!person.PersonName.isEmpty()) {
        SelectSqlCount += QString(" AND PersonName = '%1'").arg(person.PersonName);
        SelectSql += QString(" AND PersonName = '%1'").arg(person.PersonName);
    }

    if (!person.PersonType.isEmpty()) {
        SelectSqlCount += QString(" AND PersonType = '%1'").arg(person.PersonType);
        SelectSql += QString(" AND PersonType = '%1'").arg(person.PersonType);
    }

    if (person.PersonID.isEmpty()) {//需要获取总记录条数
        QSqlQuery query(QSqlDatabase::database(connectionName));
        query.exec(SelectSqlCount);

        while (query.next()) {
            person_page.ResultCount = query.value(0).toULongLong();//总记录数
        }

        //总页数
        person_page.PageCount = person_page.ResultCount / person_page.ResultCurrent;

        if ((person_page.ResultCount % person_page.ResultCurrent) != 0) {
            person_page.PageCount++;

        }
    }

    //偏移位置
    quint64 StartIndex = (person_page.PageCurrent - 1) * person_page.ResultCurrent;

    SelectSql += QString(" ORDER BY RegisterTime DESC LIMIT %1,%2").arg(StartIndex).arg(person_page.ResultCurrent);

    CommonSetting::print(SelectSql);

    QSqlQuery query(QSqlDatabase::database(connectionName));
    query.exec(SelectSql);

    if (query.lastError().type() != QSqlError::NoError) {
        CommonSetting::print("查询人员信息失败 = " + query.lastError().text());
    } else {
        CommonSetting::print("查询人员信息成功");
    }

    QString Body;

    if (person.PersonID.isEmpty()) {//获取人员的文字信息
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

            QStringList InsignItemIdList = query.value(16).toString().split("|");
            QStringList InsignItemSyncStatusList = query.value(17).toString().split("|");

            QStringList SyncStatusList;
            QString InsignItemSyncStatus;

            int size = InsignItemIdList.size();

            for (int i = 0; i < size; i++) {
                QString id = InsignItemIdList.at(i);
                QString status = (InsignItemSyncStatusList.at(i) == "Y") ? "同步成功" : "等待终端同步";

                QString DeviceName;//执行器的名字

                QString SelectSql = QString("SELECT DeviceBuild,DeviceUnit FROM device_info_table WHERE DeviceID = '%1'").arg(id);

                QSqlQuery query(QSqlDatabase::database(connectionName));
                query.exec(SelectSql);

                while(query.next()) {
                    DeviceName = query.value(0).toString() + QString("|") + query.value(1).toString() + QString("|人脸识别设备");
                    SyncStatusList << QString("%1|%2").arg(DeviceName).arg(status);
                }
            }

            InsignItemSyncStatus = SyncStatusList.join(";");

            InsignItemSyncStatus = CommonSetting::toBase64(InsignItemSyncStatus);

            Body.append(QString("<PersonInfo PersonID=\"%1\" PersonBuild=\"%2\" PersonUnit=\"%3\" PersonFloor=\"%4\" PersonRoom=\"%5\" PersonName=\"%6\" PersonSex=\"%7\" PersonType=\"%8\" IDCardNumber=\"%9\" ICCardNumber = \"%10\" PhoneNumber=\"%11\" PhoneNumber2=\"%12\"  RegisterTime=\"%13\" ExpiryTime=\"%14\" Blacklist=\"%15\" isActivate=\"%16\" InsignItemSyncStatus=\"%17\"/>").arg(PersonID).arg(PersonBuild).arg(PersonUnit).arg(PersonFloor).arg(PersonRoom).arg(PersonName).arg(PersonSex).arg(PersonType).arg(IDCardNumber).arg(ICCardNumber).arg(PhoneNumber).arg(PhoneNumber2).arg(RegisterTime).arg(ExpiryTime).arg(Blacklist).arg(isActivate).arg(InsignItemSyncStatus));
        }

        Body.append(QString("<PersonPageInfo PageCurrent=\"%1\" ResultCurrent=\"%2\" PageCount=\"%3\" ResultCount=\"%4\" />").arg(person_page.PageCurrent).arg(person_page.ResultCurrent).arg(person_page.PageCount).arg(person_page.ResultCount));
    } else {//获取人员的特征值和图片信息
        while (query.next()) {
            QString PersonID = query.value(0).toString();
            QString FeatureValue = query.value(18).toString();
            QString PersonImageUrl = query.value(19).toString();
            QString IDCardImageUrl = query.value(20).toString();

            QString PersonImageBase64;
            if (!PersonImageUrl.isEmpty()) {
                QImage PersonImage(PersonImageUrl);
                PersonImageBase64 = CommonSetting::QImage_To_Base64(PersonImage);
            }

            QString IDCardImageBase64;
            if (!IDCardImageUrl.isEmpty()) {
                QImage IDCardImage(IDCardImageUrl);
                IDCardImageBase64 = CommonSetting::QImage_To_Base64(IDCardImage);
            }

            Body.append(QString("<PersonImage PersonID=\"%1\">").arg(PersonID));
            Body.append(QString("<FeatureValue>%1</FeatureValue>").arg(FeatureValue));
            Body.append(QString("<PersonImage>%1</PersonImage>").arg(PersonImageBase64));
            Body.append(QString("<IDCardImage>%1</IDCardImage>").arg(IDCardImageBase64));
            Body.append(QString("</PersonImage>"));
        }
    }

    SendData(Body);

    if (person.PersonID.isEmpty()) {
        CommonSetting::print("返回人员的文字信息");
    } else {
        CommonSetting::print("返回人员的特征值和图片信息");
    }
}

void TcpSocketApi::DeletePersonInfo(const PersonInfo &person)
{
    QMutexLocker lock(&GlobalConfig::GlobalLock);

    if (!person.PersonID.isEmpty()) {//删除指定人员信息
        //发送命令通知本条人员信息所绑定的执行器删除该条人员信息
        QStringList InsignItemIdList;
        QStringList InsignItemIPList;

        QString SelectSql = QString("SELECT InsignItemIds FROM person_info_table WHERE PersonID = '%1'").arg(person.PersonID);

        QSqlQuery query(QSqlDatabase::database(connectionName));
        query.exec(SelectSql);

        while (query.next()) {
            InsignItemIdList = query.value(0).toString().split("|");
        }

        foreach (QString InsignItemId, InsignItemIdList) {
            QString SelectSql = QString("SELECT DeviceIP FROM device_info_table WHERE DeviceID = '%1'　AND DeviceBuild != '平台'").arg(InsignItemId);

            QSqlQuery query(QSqlDatabase::database(connectionName));
            query.exec(SelectSql);

            while (query.next()) {
                InsignItemIPList << query.value(0).toString();
            }
        }

        //准备发送的数据包
        QString Msg;
        if (!InsignItemIPList.isEmpty()) {
            Msg.append("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
            Msg.append(QString("<DbServer NowTime=\"%1\">").arg(CommonSetting::GetCurrentDateTime()));
            Msg.append(QString("<DeletePersonInfo PersonID=\"%1\" />").arg(person.PersonID));
            Msg.append("</DbServer>");

            int length = Msg.toLocal8Bit().size();
            Msg = QString("IDOOR:") + QString("%1").arg(length,14,10,QLatin1Char('0')) + Msg;
        }

        //发送数据包
        foreach (QString InsignItemIP, InsignItemIPList) {
            TcpClientApi *client = new TcpClientApi(InsignItemIP,Msg);

            Q_UNUSED(client);
        }

        //删除数据库中指定人员信息
        QString DeleteSql = QString("DELETE FROM person_info_table WHERE PersonID = '%1'").arg(person.PersonID);

        query.exec(DeleteSql);

        if (query.lastError().type() != QSqlError::NoError) {
            CommonSetting::print("删除数据库中指定人员信息失败 = " + query.lastError().text());
        } else {
            CommonSetting::print("删除数据库中指定人员信息成功");
        }

        //删除指定人员图片
        QString PersonImageUrl = CommonSetting::GetCurrentPath() + QString("images/") + QString("PersonImage_") + person.PersonID + QString(".jpg");
        int result = system(QString("rm -rf %1").arg(PersonImageUrl).toLatin1().data());
        Q_UNUSED(result);

        QString IDCardImageUrl = CommonSetting::GetCurrentPath() + QString("images/") + QString("IDCardImage_") + person.PersonID + QString(".jpg");
        result = system(QString("rm -rf %1").arg(IDCardImageUrl).toLatin1().data());
        Q_UNUSED(result);

        CommonSetting::print("删除指定人员图片成功");
    } else {//批量删除符合条件的人员信息
        QStringList PersonIDList,InsignItemIdsList,PersonImageUrlList,IDCardImageUrlList;

        QString SelectSql = QString("SELECT PersonID,InsignItemIds,PersonImageUrl,IDCardImageUrl FROM person_info_table WHERE 1=1");
        QString DeleteSql = QString("DELETE FROM person_info_table WHERE 1=1");


        if (!person.PersonBuild.isEmpty()) {
            SelectSql += QString(" AND PersonBuild = '%1'").arg(person.PersonBuild);
            DeleteSql += QString(" AND PersonBuild = '%1'").arg(person.PersonBuild);
        }

        if (!person.PersonUnit.isEmpty()) {
            SelectSql += QString(" AND PersonUnit = '%1'").arg(person.PersonUnit);
            DeleteSql += QString(" AND PersonUnit = '%1'").arg(person.PersonUnit);
        }

        if (!person.PersonFloor.isEmpty()) {
            SelectSql += QString(" AND PersonFloor = '%1'").arg(person.PersonFloor);
            DeleteSql += QString(" AND PersonFloor = '%1'").arg(person.PersonFloor);
        }

        if (!person.PersonRoom.isEmpty()) {
            SelectSql += QString(" AND PersonRoom = '%1'").arg(person.PersonRoom);
            DeleteSql += QString(" AND PersonRoom = '%1'").arg(person.PersonRoom);
        }

        if (!person.PersonName.isEmpty()) {
            SelectSql += QString(" AND PersonName = '%1'").arg(person.PersonName);
            DeleteSql += QString(" AND PersonName = '%1'").arg(person.PersonName);
        }

        if (!person.PersonType.isEmpty()) {
            SelectSql += QString(" AND PersonType = '%1'").arg(person.PersonType);
            DeleteSql += QString(" AND PersonType = '%1'").arg(person.PersonType);
        }

        QSqlQuery SelectQuery(QSqlDatabase::database(connectionName));
        SelectQuery.exec(SelectSql);

        while (SelectQuery.next()) {
            PersonIDList << SelectQuery.value(0).toString();
            InsignItemIdsList << SelectQuery.value(1).toString();
            PersonImageUrlList << SelectQuery.value(2).toString();
            IDCardImageUrlList << SelectQuery.value(3).toString();
        }

        //发送命令通知本条人员信息所绑定的执行器删除该条人员信息
        int size = PersonIDList.size();

        for (int i = 0; i < size; i++) {
            QString tempInsignItemId = InsignItemIdsList.at(i);
            QStringList tempInsignItemIdList = tempInsignItemId.split("|");
            QStringList InsignItemIPList;

            foreach (QString InsignItemId, tempInsignItemIdList) {
                QString SelectSql = QString("SELECT DeviceIP FROM device_info_table WHERE DeviceID = '%1'　AND DeviceBuild != '平台'").arg(InsignItemId);

                QSqlQuery query(QSqlDatabase::database(connectionName));
                query.exec(SelectSql);

                while (query.next()) {
                    InsignItemIPList << query.value(0).toString();
                }
            }

            //准备发送的数据包
            QString Msg;
            if (!InsignItemIPList.isEmpty()) {
                Msg.append("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
                Msg.append(QString("<DbServer NowTime=\"%1\">").arg(CommonSetting::GetCurrentDateTime()));
                Msg.append(QString("<DeletePersonInfo PersonID=\"%1\" />").arg(PersonIDList.at(i)));
                Msg.append("</DbServer>");

                int length = Msg.toLocal8Bit().size();
                Msg = QString("IDOOR:") + QString("%1").arg(length,14,10,QLatin1Char('0')) + Msg;
            }

            //发送数据包
            foreach (QString InsignItemIP, InsignItemIPList) {
                TcpClientApi *client = new TcpClientApi(InsignItemIP,Msg);

                Q_UNUSED(client);
            }
        }

        //批量删除数据库中人员信息
        QSqlQuery DeleteQuery(QSqlDatabase::database(connectionName));
        DeleteQuery.exec(DeleteSql);

        if (DeleteQuery.lastError().type() != QSqlError::NoError) {
            CommonSetting::print("批量删除数据库中人员信息失败 = " + DeleteQuery.lastError().text());
        } else {
            CommonSetting::print("批量删除数据库中人员信息成功");
        }

        //删除本地人员图片
        for (int i = 0; i < size; i++) {
            int result = system(QString("rm -rf %1").arg(PersonImageUrlList.at(i)).toLatin1().data());

            result = system(QString("rm -rf %1").arg(IDCardImageUrlList.at(i)).toLatin1().data());

            CommonSetting::print("删除人员图片成功");

            Q_UNUSED(result);
        }
    }
}

//本函数还有缺陷，如果楼栋和单元有变更，需要将之前设备上面的人员信息删除
void TcpSocketApi::UpdatePersonInfo(PersonInfo &person)
{
    QMutexLocker lock(&GlobalConfig::GlobalLock);

    if (!person.PersonID.isEmpty()) {
        //根据楼栋和单元找出本条人员信息需要绑定的执行器
        QStringList ids,syncs;

        QString SelectSql = QString("SELECT DeviceID FROM device_info_table WHERE DeviceBuild = '小区出入口' OR DeviceBuild = '%1' AND DeviceUnit = '%2'").arg(person.PersonBuild).arg(person.PersonUnit);

        QSqlQuery query(QSqlDatabase::database(connectionName));
        query.exec(SelectSql);

        while (query.next()) {
            QString DeviceID = query.value(0).toString();
            ids << DeviceID;
            syncs << "N";
        }

        person.InsignItemIds = ids.join("|");
        person.InsignItemSyncStatus = syncs.join("|");

        QString UpdateSql;

        if (person.FeatureValue.isEmpty()) {
            UpdateSql = QString("UPDATE person_info_table SET PersonBuild = '%1',PersonUnit = '%2',PersonFloor = '%3',PersonRoom = '%4',PersonName = '%5',PersonSex = '%6',PersonType = '%7',IDCardNumber = '%8',ICCardNumber = '%9',PhoneNumber = '%10',PhoneNumber2 = '%11',RegisterTime = '%12',ExpiryTime = '%13',InsignItemIds = '%14',InsignItemSyncStatus = '%15' WHERE PersonID = '%16'").arg(person.PersonBuild).arg(person.PersonUnit).arg(person.PersonFloor).arg(person.PersonRoom).arg(person.PersonName).arg(person.PersonSex).arg(person.PersonType).arg(person.IDCardNumber).arg(person.ICCardNumber).arg(person.PhoneNumber).arg(person.PhoneNumber2).arg(person.RegisterTime).arg(person.ExpiryTime).arg(person.InsignItemIds).arg(person.InsignItemSyncStatus).arg(person.PersonID);
        } else {
            UpdateSql = QString("UPDATE person_info_table SET PersonBuild = '%1',PersonUnit = '%2',PersonFloor = '%3',PersonRoom = '%4',PersonName = '%5',PersonSex = '%6',PersonType = '%7',IDCardNumber = '%8',ICCardNumber = '%9',PhoneNumber = '%10',PhoneNumber2 = '%11',RegisterTime = '%12',ExpiryTime = '%13',FeatureValue = '%14',InsignItemIds = '%15',InsignItemSyncStatus = '%16' WHERE PersonID = '%17'").arg(person.PersonBuild).arg(person.PersonUnit).arg(person.PersonFloor).arg(person.PersonRoom).arg(person.PersonName).arg(person.PersonSex).arg(person.PersonType).arg(person.IDCardNumber).arg(person.ICCardNumber).arg(person.PhoneNumber).arg(person.PhoneNumber2).arg(person.RegisterTime).arg(person.ExpiryTime).arg(person.InsignItemIds).arg(person.InsignItemSyncStatus).arg(person.PersonID);
        }

        query.exec(UpdateSql);

        if (query.lastError().type() != QSqlError::NoError) {
            CommonSetting::print("更新人员信息失败 = " + query.lastError().text());
        } else {
            CommonSetting::print("更新人员信息成功");
        }
    }
}

void TcpSocketApi::ClearPersonInfo()
{
    QMutexLocker lock(&GlobalConfig::GlobalLock);

    //发送命令通知所有执行器删除所有人员信息
    QString Msg;
    Msg.append("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
    Msg.append(QString("<DbServer NowTime=\"%1\">").arg(CommonSetting::GetCurrentDateTime()));
    Msg.append("<ClearPersonInfo />");
    Msg.append("</DbServer>");

    int length = Msg.toLocal8Bit().size();
    Msg = QString("IDOOR:") + QString("%1").arg(length,14,10,QLatin1Char('0')) + Msg;


    QStringList InsignItemIPList;

    QString SelectSql = QString("SELECT DeviceIP FROM device_info_table WHERE DeviceBuild != '平台'");

    QSqlQuery query(QSqlDatabase::database(connectionName));
    query.exec(SelectSql);

    while (query.next()) {
        InsignItemIPList << query.value(0).toString();
    }

    //发送数据包
    foreach (QString InsignItemIP, InsignItemIPList) {
        TcpClientApi *client = new TcpClientApi(InsignItemIP,Msg);

        Q_UNUSED(client);
    }

    //清空数据库中所有的人员信息
    QString DeleteSql = QString("DELETE FROM person_info_table");

    query.exec(DeleteSql);

    if (query.lastError().type() != QSqlError::NoError) {
        CommonSetting::print("清空数据库中所有的人员信息失败 = " + query.lastError().text());
    } else {
        CommonSetting::print("清空数据库中所有的人员信息成功");
    }

    QFuture<void> future = QtConcurrent::run([=]() {
        //删除本地所有人员图片
        QString path = CommonSetting::GetCurrentPath() + "images";

        //int result = system(QString("rm -rf %1").arg(path).toLatin1().data());//图片太多的情况下会报错argument too long
        int result = system(QString("ls %1 | xargs rm -rf").arg(path).toLatin1().data());

        Q_UNUSED(result);

        CommonSetting::print("删除本地所有人员图片成功");
    });
}

void TcpSocketApi::AddDeviceInfo(const DeviceInfo &device)
{
    QString InsertSql = QString("INSERT INTO device_info_table VALUES('%1','%2','%3','%4','%5','%6','%7','%8','%9','%10')").arg(device.DeviceID).arg(device.DeviceBuild).arg(device.DeviceUnit).arg(device.DeviceIP).arg(device.Longitude).arg(device.Latitude).arg(device.Altitude).arg(device.MainStreamRtspAddr).arg(device.SubStreamRtspAddr).arg(device.isUploadPlatformCenter);

    QSqlQuery query(QSqlDatabase::database(connectionName));
    query.exec(InsertSql);

    if (query.lastError().type() != QSqlError::NoError) {
        CommonSetting::print("添加执行器信息失败 = " + query.lastError().text());
    } else {
        CommonSetting::print("添加执行器信息成功");

        QFuture<void> future = QtConcurrent::run([=]() {
            // Code in this block will run in another thread
            QMutexLocker lock(&GlobalConfig::GlobalLock);

            DeviceInfo di = device;

            QString connectionName = QString("connectionName%1").arg(di.DeviceID);

            CommonSetting::createConnection(connectionName);

            CommonSetting::print("数据库连接名 = " + connectionName);
            {
                QStringList PersonIDList,InsignItemIdsList,InsignItemSyncStatusList;

                QTime time;

                time.start();

                //同步更新人员信息的执行器id列表和执行器的同步状态
                QString SelectSql = QString("SELECT PersonID,PersonBuild,InsignItemIds,InsignItemSyncStatus FROM person_info_table");//这里如果不排序的情况下,同步更新人员信息的执行器id列表和执行器的同步状态会有问题，并且会导致内存溢出，一直增加

                QSqlQuery query(QSqlDatabase::database(connectionName));
                query.exec(SelectSql);

                while (query.next()) {
                    QString PersonID = query.value(0).toString();
                    QString PersonBuild = query.value(1).toString();
                    QString id = query.value(2).toString();
                    QString status = query.value(3).toString();

                    QStringList tempInsignItemIdsList;
                    QStringList tempInsignItemSyncStatusList;

                    if (!id.isEmpty()) {
                        tempInsignItemIdsList = id.split("|");
                    }

                    if (!status.isEmpty()) {
                        tempInsignItemSyncStatusList = status.split("|");
                    }

                    if ((di.DeviceBuild == QString("小区出入口")) ||
                            (di.DeviceBuild == QString("平台")) ||
                            (di.DeviceBuild == PersonBuild)) {
                        //小区出入口设备、平台设备、单元楼设备或者地下室设备
                        tempInsignItemIdsList << di.DeviceID;
                        tempInsignItemSyncStatusList << "N";

                        QString InsignItemIds =
                                tempInsignItemIdsList.join("|").simplified().trimmed();

                        QString InsignItemSyncStatus =
                                tempInsignItemSyncStatusList.join("|").simplified().trimmed();

                        PersonIDList << PersonID;
                        InsignItemIdsList << InsignItemIds;
                        InsignItemSyncStatusList << InsignItemSyncStatus;
                    }
                }

                int UseTime = time.elapsed();

                CommonSetting::print(QString("查询所有人员信号耗时 = %1").arg(UseTime));

                time.start();

                CommonSetting::transaction(connectionName);

                QSqlQuery UpdateQuery(QSqlDatabase::database(connectionName));

                int size = PersonIDList.size();

                for (int i = 0; i < size; i++) {
                    QString UpdateSql = QString("UPDATE person_info_table SET InsignItemIds = '%1',InsignItemSyncStatus = '%2' WHERE PersonID = '%3'").arg(InsignItemIdsList.at(i)).arg(InsignItemSyncStatusList.at(i))
                            .arg(PersonIDList.at(i));

                    UpdateQuery.exec(UpdateSql);

                    if (UpdateQuery.lastError().type() != QSqlError::NoError) {
                        CommonSetting::print("同步更新人员信息的执行器id列表和执行器的同步状态失败 = " + UpdateQuery.lastError().text());
                    } else {
                        //CommonSetting::print("同步更新人员信息的执行器id列表和执行器的同步状态成功");
                    }

                    UpdateQuery.clear();
                }

                CommonSetting::commit(connectionName);

                UseTime = time.elapsed();

                CommonSetting::print(QString("更新所有人员信息耗时 = %1").arg(UseTime));
            }

            CommonSetting::closeConnection(connectionName);

            CommonSetting::print("同步更新人员信息的执行器id列表和执行器的同步状态成功");
        });
    }
}

void TcpSocketApi::SelectDeviceInfo(const DeviceInfo &device)
{
    QMutexLocker lock(&GlobalConfig::GlobalLock);

    QList<DeviceInfo> device_list;
    QString SelectSql;

    if (device.DeviceIP.isEmpty()) {
        SelectSql = QString("SELECT * FROM device_info_table");
    } else {
        SelectSql = QString("SELECT * FROM device_info_table WHERE DeviceIP = '%1'").arg(device.DeviceIP);
    }

    QSqlQuery query(QSqlDatabase::database(connectionName));
    query.exec(SelectSql);

    while(query.next()) {
        DeviceInfo di;

        di.DeviceID = query.value(0).toString();
        di.DeviceBuild = CommonSetting::toBase64(query.value(1).toString());
        di.DeviceUnit = CommonSetting::toBase64(query.value(2).toString());
        di.DeviceIP = query.value(3).toString();
        di.Longitude = query.value(4).toString();
        di.Latitude = query.value(5).toString();
        di.Altitude = query.value(6).toString();
        di.MainStreamRtspAddr = query.value(7).toString();
        di.SubStreamRtspAddr = query.value(8).toString();
        di.isUploadPlatformCenter = query.value(9).toString();

        device_list.append(di);
    }

    QString Msg;

    foreach (DeviceInfo di, device_list) {
        Msg.append(QString("<DeviceInfo DeviceID=\"%1\" DeviceBuild=\"%2\" DeviceUnit=\"%3\" DeviceIP=\"%4\" Longitude=\"%5\" Latitude=\"%6\" Altitude=\"%7\" MainStreamRtspAddr=\"%8\" SubStreamRtspAddr=\"%9\" isUploadPlatformCenter=\"%10\" />").arg(di.DeviceID).arg(di.DeviceBuild).arg(di.DeviceUnit).arg(di.DeviceIP).arg(di.Longitude).arg(di.Latitude).arg(di.Altitude).arg(di.MainStreamRtspAddr).arg(di.SubStreamRtspAddr).arg(di.isUploadPlatformCenter));
    }

    SendData(Msg);

    CommonSetting::print("返回执行器信息");
}

void TcpSocketApi::DelectDeviceInfo(DeviceInfo &device)
{
    if (!device.DeviceID.isEmpty()) {
        //根据执行器id找到执行器的楼栋
        QString SelectSql = QString("SELECT DeviceBuild FROM device_info_table WHERE DeviceID = '%1'").arg(device.DeviceID);

        QSqlQuery query(QSqlDatabase::database(connectionName));
        query.exec(SelectSql);

        while (query.next()) {
            device.DeviceBuild = query.value(0).toString();
        }

        //删除执行器
        QString DeleteSql = QString("DELETE FROM device_info_table WHERE DeviceID = '%1'").arg(device.DeviceID);

        query.exec(DeleteSql);

        if (query.lastError().type() != QSqlError::NoError) {
            CommonSetting::print("删除执行器失败 = " + query.lastError().text());
        } else {
            CommonSetting::print("删除执行器成功");

            QFuture<void> future = QtConcurrent::run([=]() {
                // Code in this block will run in another thread
                QMutexLocker lock(&GlobalConfig::GlobalLock);

                DeviceInfo di = device;

                QString connectionName = QString("connectionName%1").arg(di.DeviceID);

                CommonSetting::createConnection(connectionName);

                CommonSetting::print("数据库连接名 = " + connectionName);

                {
                    QStringList PersonIDList,InsignItemIdsList,InsignItemSyncStatusList;

                    QTime time;

                    time.start();

                    //同步更新人员信息的执行器id列表和执行器的同步状态
                    QString SelectSql = QString("SELECT PersonID,PersonBuild,InsignItemIds,InsignItemSyncStatus FROM person_info_table");

                    QSqlQuery query(QSqlDatabase::database(connectionName));
                    query.exec(SelectSql);

                    while (query.next()) {
                        QString PersonID = query.value(0).toString();

                        QString PersonBuild = query.value(1).toString();

                        QStringList tempInsignItemIdsList = query.value(2).toString().split("|");

                        QStringList tempInsignItemSyncStatusList =
                                query.value(3).toString().split("|");

                        if ((device.DeviceBuild == QString("小区出入口")) ||
                                (device.DeviceBuild == QString("平台")) ||
                                (device.DeviceBuild == PersonBuild)) {
                            //小区出入口设备、平台设备、单元楼设备或者地下室设备
                            int size = tempInsignItemIdsList.size();

                            for (int i = 0; i < size; i++) {
                                if (device.DeviceID == tempInsignItemIdsList.at(i)) {
                                    tempInsignItemIdsList.removeAt(i);
                                    tempInsignItemSyncStatusList.removeAt(i);
                                    break;
                                }
                            }

                            QString InsignItemIds = tempInsignItemIdsList.join("|");
                            QString InsignItemSyncStatus = tempInsignItemSyncStatusList.join("|");

                            PersonIDList << PersonID;
                            InsignItemIdsList << InsignItemIds;
                            InsignItemSyncStatusList << InsignItemSyncStatus;
                        }
                    }

                    int UseTime = time.elapsed();

                    CommonSetting::print(QString("查询所有人员信号耗时 = %1").arg(UseTime));

                    time.start();

                    CommonSetting::transaction(connectionName);

                    QSqlQuery UpdateQuery(QSqlDatabase::database(connectionName));

                    int size = PersonIDList.size();

                    for (int i = 0; i < size; i++) {
                        QString UpdateSql = QString("UPDATE person_info_table SET InsignItemIds = '%1',InsignItemSyncStatus = '%2' WHERE PersonID = '%3'").arg(InsignItemIdsList.at(i)).arg(InsignItemSyncStatusList.at(i)).arg(PersonIDList.at(i));

                        UpdateQuery.exec(UpdateSql);

                        if (UpdateQuery.lastError().type() != QSqlError::NoError) {
                            CommonSetting::print("同步更新人员信息的执行器id列表和执行器的同步状态失败 = " + UpdateQuery.lastError().text());
                        } else {
                            //CommonSetting::print("同步更新人员信息的执行器id列表和执行器的同步状态成功");
                        }

                        UpdateQuery.clear();
                    }

                    CommonSetting::commit(connectionName);

                    UseTime = time.elapsed();

                    CommonSetting::print(QString("更新所有人员信息耗时 = %1").arg(UseTime));
                }

                CommonSetting::closeConnection(connectionName);

                CommonSetting::print("同步更新人员信息的执行器id列表和执行器的同步状态成功");
            });
        }
    }
}

void TcpSocketApi::UpdateDeviceInfo(const DeviceInfo &device)
{
    if (!device.DeviceID.isEmpty()) {
        //根据执行器id找出执行器的DeviceBuild
        QString DeviceBuild;

        QString SelectSql = QString("SELECT DeviceBuild FROM device_info_table WHERE DeviceID = '%1'").arg(device.DeviceID);

        QSqlQuery query(QSqlDatabase::database(connectionName));
        query.exec(SelectSql);

        while (query.next()) {
            DeviceBuild = query.value(0).toString();
        }

        //更新执行器信息
        QString UpdateSql = QString("UPDATE device_info_table SET DeviceBuild = '%1',DeviceUnit = '%2',DeviceIP = '%3',Longitude = '%4',Latitude = '%5',Altitude = '%6',MainStreamRtspAddr = '%7',SubStreamRtspAddr = '%8',isUploadPlatformCenter = '%9' WHERE DeviceID = '%10'").arg(device.DeviceBuild).arg(device.DeviceUnit).arg(device.DeviceIP).arg(device.Longitude).arg(device.Latitude).arg(device.Altitude).arg(device.MainStreamRtspAddr).arg(device.SubStreamRtspAddr).arg(device.isUploadPlatformCenter).arg(device.DeviceID);

        query.exec(UpdateSql);

        if (query.lastError().type() != QSqlError::NoError) {
            CommonSetting::print("更新执行器信息失败 = " + query.lastError().text());
        } else {
            CommonSetting::print("更新执行器信息成功");

            QFuture<void> future = QtConcurrent::run([=]() {
                // Code in this block will run in another thread
                QMutexLocker lock(&GlobalConfig::GlobalLock);

                DeviceInfo di = device;

                QString connectionName = QString("connectionName%1").arg(di.DeviceID);

                CommonSetting::createConnection(connectionName);

                CommonSetting::print("数据库连接名 = " + connectionName);

                {
                    //同步更新人员信息的执行器id列表和执行器的同步状态
                    if ((DeviceBuild == "小区出入口") && (device.DeviceBuild == "平台")) {
                        //do nothing
                    } else if ((DeviceBuild == "平台") && (device.DeviceBuild == "小区出入口")) {
                        //do nothing
                    } else if ((DeviceBuild.contains("号楼")) && ((device.DeviceBuild == "小区出入口") || (device.DeviceBuild == "平台"))) {
                        QStringList PersonIDList,InsignItemIdsList,InsignItemSyncStatusList;

                        QString SelectSql = QString("SELECT PersonID,PersonBuild,InsignItemIds,InsignItemSyncStatus FROM person_info_table");

                        QSqlQuery query(QSqlDatabase::database(connectionName));
                        query.exec(SelectSql);

                        while(query.next()) {
                            QString PersonID = query.value(0).toString();

                            QString PersonBuild = query.value(1).toString();

                            QStringList tempInsignItemIdsList =
                                    query.value(2).toString().split("|");

                            QStringList tempInsignItemSyncStatusList =
                                    query.value(3).toString().split("|");

                            if (PersonBuild != DeviceBuild) {
                                tempInsignItemIdsList << device.DeviceID;
                                tempInsignItemSyncStatusList << "N";

                                QString InsignItemIds = tempInsignItemIdsList.join("|");
                                QString InsignItemSyncStatus =
                                        tempInsignItemSyncStatusList.join("|");

                                PersonIDList << PersonID;
                                InsignItemIdsList << InsignItemIds;
                                InsignItemSyncStatusList << InsignItemSyncStatus;
                            }
                        }

                        CommonSetting::transaction(connectionName);

                        QSqlQuery UpdateQuery(QSqlDatabase::database(connectionName));

                        int size = PersonIDList.size();

                        for (int i = 0; i < size; i++) {
                            QString UpdateSql = QString("UPDATE person_info_table SET InsignItemIds = '%1',InsignItemSyncStatus = '%2' WHERE PersonID = '%3'").arg(InsignItemIdsList.at(i)).arg(InsignItemSyncStatusList.at(i)).arg(PersonIDList.at(i));

                            UpdateQuery.exec(UpdateSql);

                            if (UpdateQuery.lastError().type() != QSqlError::NoError) {
                                CommonSetting::print("同步更新人员信息的执行器id列表和执行器的同步状态失败 = " + UpdateQuery.lastError().text());
                            } else {
                                //CommonSetting::print("同步更新人员信息的执行器id列表和执行器的同步状态成功");
                            }

                            UpdateQuery.clear();
                        }

                        CommonSetting::commit(connectionName);
                    } else if (((DeviceBuild == "小区出入口") || (DeviceBuild == "平台")) && (device.DeviceBuild.contains("号楼"))) {
                        QStringList PersonIDList,InsignItemIdsList,InsignItemSyncStatusList;

                        QString SelectSql = QString("SELECT PersonID,PersonBuild,InsignItemIds,InsignItemSyncStatus FROM person_info_table");

                        QSqlQuery query(QSqlDatabase::database(connectionName));
                        query.exec(SelectSql);

                        while(query.next()) {
                            QString PersonID = query.value(0).toString();

                            QString PersonBuild = query.value(1).toString();

                            QStringList tempInsignItemIdsList =
                                    query.value(2).toString().split("|");

                            QStringList tempInsignItemSyncStatusList =
                                    query.value(3).toString().split("|");

                            QString SyncStatus;

                            int size = tempInsignItemIdsList.size();

                            for (int i = 0; i < size; i++) {
                                if (device.DeviceID == tempInsignItemIdsList.at(i)) {
                                    SyncStatus = tempInsignItemSyncStatusList.at(i);
                                    tempInsignItemIdsList.removeAt(i);
                                    tempInsignItemSyncStatusList.removeAt(i);
                                    break;
                                }
                            }

                            if (PersonBuild == device.DeviceBuild) {
                                tempInsignItemIdsList << device.DeviceID;
                                tempInsignItemSyncStatusList << SyncStatus;
                            }

                            QString InsignItemIds = tempInsignItemIdsList.join("|");
                            QString InsignItemSyncStatus = tempInsignItemSyncStatusList.join("|");


                            PersonIDList << PersonID;
                            InsignItemIdsList << InsignItemIds;
                            InsignItemSyncStatusList << InsignItemSyncStatus;
                        }

                        CommonSetting::transaction(connectionName);

                        QSqlQuery UpdateQuery(QSqlDatabase::database(connectionName));

                        int size = PersonIDList.size();

                        for (int i = 0; i < size; i++) {
                            QString UpdateSql = QString("UPDATE person_info_table SET InsignItemIds = '%1',InsignItemSyncStatus = '%2' WHERE PersonID = '%3'").arg(InsignItemIdsList.at(i)).arg(InsignItemSyncStatusList.at(i)).arg(PersonIDList.at(i));

                            UpdateQuery.exec(UpdateSql);

                            if (UpdateQuery.lastError().type() != QSqlError::NoError) {
                                CommonSetting::print("同步更新人员信息的执行器id列表和执行器的同步状态失败 = " + UpdateQuery.lastError().text());
                            } else {
                                //CommonSetting::print("同步更新人员信息的执行器id列表和执行器的同步状态成功");
                            }

                            UpdateQuery.clear();
                        }

                        CommonSetting::commit(connectionName);
                    }

                }

                CommonSetting::closeConnection(connectionName);

                CommonSetting::print("同步更新人员信息的执行器id列表和执行器的同步状态成功");
            });
        }
    }
}

void TcpSocketApi::ClearDeviceInfo()
{
    QString DeleteSql = QString("DELETE FROM device_info_table");

    QSqlQuery query(QSqlDatabase::database(connectionName));
    query.exec(DeleteSql);

    if (query.lastError().type() != QSqlError::NoError) {
        CommonSetting::print("清空执行器信息失败 = " + query.lastError().text());
    } else {
        CommonSetting::print("清空执行器信息成功");

        QFuture<void> future = QtConcurrent::run([=]() {
            // Code in this block will run in another thread
            QMutexLocker lock(&GlobalConfig::GlobalLock);

            QString connectionName = QString("connectionName");

            CommonSetting::createConnection(connectionName);

            CommonSetting::print("数据库连接名 = " + connectionName);
            {
                CommonSetting::transaction(connectionName);

                //同步更新人员信息的执行器id列表和执行器的同步状态
                QString UpdateSql = QString("UPDATE person_info_table SET InsignItemIds = '%1',InsignItemSyncStatus = '%2'").arg("").arg("");

                QSqlQuery UpdateQuery(QSqlDatabase::database(connectionName));
                UpdateQuery.exec(UpdateSql);

                if (UpdateQuery.lastError().type() != QSqlError::NoError) {
                    CommonSetting::print("同步更新人员信息的执行器id列表和执行器的同步状态失败 = " + UpdateQuery.lastError().text());
                } else {
                    //CommonSetting::print("同步更新人员信息的执行器id列表和执行器的同步状态成功");
                }

                CommonSetting::commit(connectionName);
            }

            CommonSetting::closeConnection(connectionName);

            CommonSetting::print("同步更新人员信息的执行器id列表和执行器的同步状态成功");
        });
    }
}

void TcpSocketApi::ResetDeviceInfo(const DeviceInfo &device)
{
    QFuture<void> future = QtConcurrent::run([=]() {
        // Code in this block will run in another thread
        QMutexLocker lock(&GlobalConfig::GlobalLock);

        DeviceInfo di = device;

        QString connectionName = QString("connectionName%1").arg(di.DeviceID);

        CommonSetting::createConnection(connectionName);

        CommonSetting::print("数据库连接名 = " + connectionName);
        {
            QStringList PersonIDList,InsignItemIdsList,InsignItemSyncStatusList;

            QString SelectSql = QString("SELECT PersonID,InsignItemIds,InsignItemSyncStatus FROM person_info_table WHERE instr(InsignItemIds, '%1') > 0").arg(di.DeviceID);

            QSqlQuery SelectQuery(QSqlDatabase::database(connectionName));
            SelectQuery.exec(SelectSql);

            while (SelectQuery.next()) {
                QString PersonID = SelectQuery.value(0).toString();
                QStringList tempInsignItemIdsList = SelectQuery.value(1).toString().split("|");
                QStringList tempInsignItemSyncStatusList = SelectQuery.value(2).toString().split("|");

                int size = tempInsignItemIdsList.size();

                for (int i = 0; i < size; i++) {
                    if (tempInsignItemIdsList.at(i) == di.DeviceID) {
                        tempInsignItemSyncStatusList.replace(i,"N");
                        break;
                    }
                }


                QString InsignItemIds = tempInsignItemIdsList.join("|");
                QString InsignItemSyncStatus = tempInsignItemSyncStatusList.join("|");

                PersonIDList << PersonID;
                InsignItemIdsList << InsignItemIds;
                InsignItemSyncStatusList << InsignItemSyncStatus;
            }


            //同步更新人员信息的执行器id列表和执行器的同步状态
            CommonSetting::transaction(connectionName);

            int size = PersonIDList.size();

            for (int i = 0; i < size; i++) {
                QString UpdateSql = QString("UPDATE person_info_table SET InsignItemIds = '%1',InsignItemSyncStatus = '%2' WHERE PersonID = '%3'").arg(InsignItemIdsList.at(i))
                        .arg(InsignItemSyncStatusList.at(i)).arg(PersonIDList.at(i));

                QSqlQuery UpdateQuery(QSqlDatabase::database(connectionName));
                UpdateQuery.exec(UpdateSql);

                if (UpdateQuery.lastError().type() != QSqlError::NoError) {
                    CommonSetting::print("同步更新人员信息的执行器id列表和执行器的同步状态失败 = " + UpdateQuery.lastError().text());
                } else {
                    //CommonSetting::print("同步更新人员信息的执行器id列表和执行器的同步状态成功");
                }
            }

            CommonSetting::commit(connectionName);
        }

        CommonSetting::closeConnection(connectionName);

        CommonSetting::print("同步更新人员信息的执行器id列表和执行器的同步状态成功");
    });
}

void TcpSocketApi::SelectCompareRecordInfo(const CompareRecordInfo &record, CompareRecordPageInfo &record_page)
{
    QMutexLocker lock(&GlobalConfig::GlobalLock);

    QString SelectSqlCount = QString("SELECT COUNT(*) FROM compare_record_info_table WHERE 1=1");
    QString SelectSql = QString("SELECT * FROM compare_record_info_table WHERE 1=1");

    if (!record.CompareRecordID.isEmpty()) {
        SelectSql += QString(" AND CompareRecordID = '%1'").arg(record.CompareRecordID);
    }

    if (!record.CompareResult.isEmpty()) {
        SelectSqlCount += QString(" AND CompareResult = '%1'").arg(record.CompareResult);
        SelectSql += QString(" AND CompareResult = '%1'").arg(record.CompareResult);
    }

    if (!record.PersonBuild.isEmpty()) {
        SelectSqlCount += QString(" AND PersonBuild = '%1'").arg(record.PersonBuild);
        SelectSql += QString(" AND PersonBuild = '%1'").arg(record.PersonBuild);
    }

    if (!record.PersonUnit.isEmpty()) {
        SelectSqlCount += QString(" AND PersonUnit = '%1'").arg(record.PersonUnit);
        SelectSql += QString(" AND PersonUnit = '%1'").arg(record.PersonUnit);
    }

    if (!record.PersonFloor.isEmpty()) {
        SelectSqlCount += QString(" AND PersonFloor = '%1'").arg(record.PersonFloor);
        SelectSql += QString(" AND PersonFloor = '%1'").arg(record.PersonFloor);
    }

    if (!record.PersonRoom.isEmpty()) {
        SelectSqlCount += QString(" AND PersonRoom = '%1'").arg(record.PersonRoom);
        SelectSql += QString(" AND PersonRoom = '%1'").arg(record.PersonRoom);
    }

    if (!record.PersonName.isEmpty()) {
        SelectSqlCount += QString(" AND PersonName = '%1'").arg(record.PersonName);
        SelectSql += QString(" AND PersonName = '%1'").arg(record.PersonName);
    }

    if (!record.PersonType.isEmpty()) {
        SelectSqlCount += QString(" AND PersonType = '%1'").arg(record.PersonType);
        SelectSql += QString(" AND PersonType = '%1'").arg(record.PersonType);
    }

    if (!record.TriggerTime.isEmpty()) {
        QString StartTime = record.TriggerTime.split("|").at(0);
        QString EndTime = record.TriggerTime.split("|").at(1);

        SelectSqlCount += QString(" AND TriggerTime >= '%1' AND TriggerTime <= '%2'").arg(StartTime).arg(EndTime);

        SelectSql += QString(" AND TriggerTime >= '%1' AND TriggerTime <= '%2'").arg(StartTime).arg(EndTime);
    }

    if (record.CompareRecordID.isEmpty()) {//需要获取总记录条数
        QSqlQuery query(QSqlDatabase::database(connectionName));
        query.exec(SelectSqlCount);

        while (query.next()) {
            record_page.ResultCount = query.value(0).toULongLong();//总记录数
        }

        //总页数
        record_page.PageCount = record_page.ResultCount / record_page.ResultCurrent;

        if ((record_page.ResultCount % record_page.ResultCurrent) != 0) {
            record_page.PageCount++;
        }
    }

    //偏移位置
    quint64 StartIndex = (record_page.PageCurrent - 1) * record_page.ResultCurrent;

    SelectSql += QString(" ORDER BY TriggerTime DESC LIMIT %1 OFFSET %2").arg(record_page.ResultCurrent).arg(StartIndex);

    QSqlQuery query(QSqlDatabase::database(connectionName));
    query.exec(SelectSql);

    if (query.lastError().type() != QSqlError::NoError) {
        CommonSetting::print("查询比对记录信息失败 = " + query.lastError().text());
    }

    QString Body;

    if (record.CompareRecordID.isEmpty()) {//获取比对记录文字信息
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
            QString isUploadPlatformCenter = query.value(20).toString();

            Body.append(QString("<CompareRecordInfo CompareRecordID=\"%1\" CompareResult=\"%2\" PersonBuild=\"%3\" PersonUnit=\"%4\" PersonFloor=\"%5\" PersonRoom=\"%6\" PersonName=\"%7\" PersonSex=\"%8\" PersonType=\"%9\" IDCardNumber=\"%10\" ICCardNumber=\"%11\" PhoneNumber=\"%12\"  ExpiryTime=\"%13\" Blacklist=\"%14\" isActivate=\"%15\" FaceSimilarity=\"%16\" UseTime=\"%17\" TriggerTime=\"%18\" isUploadPlatformCenter=\"%19\"/>").arg(CompareRecordID).arg(CompareResult).arg(PersonBuild).arg(PersonUnit).arg(PersonFloor).arg(PersonRoom).arg(PersonName).arg(PersonSex).arg(PersonType).arg(IDCardNumber).arg(ICCardNumber).arg(PhoneNumber).arg(ExpiryTime).arg(Blacklist).arg(isActivate).arg(FaceSimilarity).arg(UseTime).arg(TriggerTime).arg(isUploadPlatformCenter));
        }

        Body.append(QString("<CompareRecordPageInfo PageCurrent=\"%1\" ResultCurrent=\"%2\" PageCount=\"%3\" ResultCount=\"%4\" />").arg(record_page.PageCurrent).arg(record_page.ResultCurrent).arg(record_page.PageCount).arg(record_page.ResultCount));
    } else {//获取比对记录图片信息
        while (query.next()) {
            QString CompareRecordID = query.value(0).toString();
            QString EnterSnapPicUrl = query.value(18).toString();
            QString OriginalSnapPicUrl = query.value(19).toString();

            QString EnterSnapImageBase64;
            if (!EnterSnapPicUrl.isEmpty()) {
                QImage EnterSnapImage(EnterSnapPicUrl);
                EnterSnapImageBase64 = CommonSetting::QImage_To_Base64(EnterSnapImage);
            }

            QString OriginalSnapImageBase64;
            if (!OriginalSnapPicUrl.isEmpty()) {
                QImage OriginalSnapImage(OriginalSnapPicUrl);
                OriginalSnapImageBase64 = CommonSetting::QImage_To_Base64(OriginalSnapImage);
            }

            Body.append(QString("<CompareRecordImage CompareRecordID=\"%1\">").arg(CompareRecordID));
            Body.append(QString("<EnterSnapPic>%1</EnterSnapPic>").arg(EnterSnapImageBase64));
            Body.append(QString("<OriginalSnapPic>%1</OriginalSnapPic>").arg(OriginalSnapImageBase64));
            Body.append(QString("</CompareRecordImage>"));
        }
    }

    SendData(Body);

    if (record.CompareRecordID.isEmpty()) {
        CommonSetting::print("返回比对记录文字信息");
    } else {
        CommonSetting::print("返回比对记录图片信息");
    }
}

void TcpSocketApi::DeleteCompareRecordInfo(const CompareRecordInfo &record)
{
    QMutexLocker lock(&GlobalConfig::GlobalLock);

    QStringList EnterSnapPicUrlList,OriginalSnapPicUrlList;

    QString DeleteSql = QString("DELETE FROM compare_record_info_table WHERE 1=1");
    QString SelectSql = QString("SELECT EnterSnapPicUrl,OriginalSnapPicUrl FROM compare_record_info_table WHERE 1=1");

    if (!record.CompareRecordID.isEmpty()) {
        DeleteSql += QString(" AND CompareRecordID = '%1'").arg(record.CompareRecordID);
        SelectSql += QString(" AND CompareRecordID = '%1'").arg(record.CompareRecordID);
    }

    if (!record.CompareResult.isEmpty()) {
        DeleteSql += QString(" AND CompareResult = '%1'").arg(record.CompareResult);
        SelectSql += QString(" AND CompareResult = '%1'").arg(record.CompareResult);
    }

    if (!record.PersonBuild.isEmpty()) {
        DeleteSql += QString(" AND PersonBuild = '%1'").arg(record.PersonBuild);
        SelectSql += QString(" AND PersonBuild = '%1'").arg(record.PersonBuild);
    }

    if (!record.PersonUnit.isEmpty()) {
        DeleteSql += QString(" AND PersonUnit = '%1'").arg(record.PersonUnit);
        SelectSql += QString(" AND PersonUnit = '%1'").arg(record.PersonUnit);
    }

    if (!record.PersonFloor.isEmpty()) {
        DeleteSql += QString(" AND PersonFloor = '%1'").arg(record.PersonFloor);
        SelectSql += QString(" AND PersonFloor = '%1'").arg(record.PersonFloor);
    }

    if (!record.PersonRoom.isEmpty()) {
        DeleteSql += QString(" AND PersonRoom = '%1'").arg(record.PersonRoom);
        SelectSql += QString(" AND PersonRoom = '%1'").arg(record.PersonRoom);
    }

    if (!record.PersonName.isEmpty()) {
        DeleteSql += QString(" AND PersonName = '%1'").arg(record.PersonName);
        SelectSql += QString(" AND PersonName = '%1'").arg(record.PersonName);
    }

    if (!record.PersonType.isEmpty()) {
        DeleteSql += QString(" AND PersonType = '%1'").arg(record.PersonType);
        SelectSql += QString(" AND PersonType = '%1'").arg(record.PersonType);
    }

    if (!record.TriggerTime.isEmpty()) {
        QString StartTime = record.TriggerTime.split("|").at(0);
        QString EndTime = record.TriggerTime.split("|").at(1);

        DeleteSql += QString(" AND TriggerTime >= '%1' AND TriggerTime <= '%2'").arg(StartTime).arg(EndTime);

        SelectSql += QString(" AND TriggerTime >= '%1' AND TriggerTime <= '%2'").arg(StartTime).arg(EndTime);
    }

    //删除比对记录文字信息
    QSqlQuery DeleteQuery(QSqlDatabase::database(connectionName));
    DeleteQuery.exec(DeleteSql);

    if (DeleteQuery.lastError().type() != QSqlError::NoError) {
        CommonSetting::print("删除比对记录信息失败 = " + DeleteQuery.lastError().text());
    } else {
        CommonSetting::print("删除比对记录信息成功");
    }

    //删除比对记录图片
    QSqlQuery SelectQuery(QSqlDatabase::database(connectionName));
    SelectQuery.exec(SelectSql);

    while (SelectQuery.next()) {
        EnterSnapPicUrlList << SelectQuery.value(0).toString();
        OriginalSnapPicUrlList << SelectQuery.value(1).toString();
    }

    //这里耗时比较长，放入线程单独处理
    QFuture<void> future = QtConcurrent::run([=]() {
        int size = EnterSnapPicUrlList.size();

        for (int i = 0; i < size; i++) {
            int result = system(QString("rm -rf %1").arg(EnterSnapPicUrlList.at(i)).toLatin1().data());
            result = system(QString("rm -rf %1").arg(OriginalSnapPicUrlList.at(i)).toLatin1().data());

            Q_UNUSED(result);
        }

        CommonSetting::print("删除比对记录图片成功");
    });
}

void TcpSocketApi::ClearCompareRecordInfo()
{    
    QFuture<void> future = QtConcurrent::run([=]() {
        // Code in this block will run in another thread
        QMutexLocker lock(&GlobalConfig::GlobalLock);

        QString connectionName = QString("ClearCompareRecordInfo");

        CommonSetting::createConnection(connectionName);

        CommonSetting::print("数据库连接名 = " + connectionName);
        {
            QString DeleteSql = QString("DELETE FROM compare_record_info_table");

            QSqlQuery query(QSqlDatabase::database(connectionName));
            query.exec(DeleteSql);

            if (query.lastError().type() != QSqlError::NoError) {
                CommonSetting::print("清空比对记录信息失败 = " + query.lastError().text());
            } else {
                CommonSetting::print("清空比对记录信息成功");
            }

            //这里需要解锁，因为删除所有比对记录图片耗时比较长
            lock.unlock();

            //删除比对记录所有图片
            QString path = CommonSetting::GetCurrentPath() + QString("log/");

            int result = system(QString("ls %1 | xargs rm -rf").arg(path).toLatin1().data());

            Q_UNUSED(result);
        }

        CommonSetting::closeConnection(connectionName);
    });
}

void TcpSocketApi::AddAreaInfo(const AreaInfo &area)
{
    QMutexLocker lock(&GlobalConfig::GlobalLock);

    QString InsertSql = QString("INSERT INTO area_info_table VALUES('%1','%2','%3','%4','%5','%6')").arg(area.AreaID).arg(area.AreaBuild).arg(area.AreaUnit).arg(area.AreaFloor).arg(area.AreaRoom).arg(area.AreaTel);

    QSqlQuery query(QSqlDatabase::database(connectionName));
    query.exec(InsertSql);

    if (query.lastError().type() != QSqlError::NoError) {
        CommonSetting::print("添加区域信息失败 = " + query.lastError().text());
    } else {
        CommonSetting::print("添加区域信息成功");
    }
}

void TcpSocketApi::SelectAreaInfo()
{
    QMutexLocker lock(&GlobalConfig::GlobalLock);

    QList<AreaInfo> area_list;

    QString SelectSql = QString("SELECT * FROM area_info_table");

    QSqlQuery query(QSqlDatabase::database(connectionName));
    query.exec(SelectSql);

    if (query.lastError().type() != QSqlError::NoError) {
        CommonSetting::print("查询区域信息失败 = " + query.lastError().text());
    }

    while(query.next()) {
        AreaInfo ai;

        ai.AreaID = query.value(0).toString();
        ai.AreaBuild = CommonSetting::toBase64(query.value(1).toString());
        ai.AreaUnit = CommonSetting::toBase64(query.value(2).toString());
        ai.AreaFloor = CommonSetting::toBase64(query.value(3).toString());
        ai.AreaRoom = CommonSetting::toBase64(query.value(4).toString());
        ai.AreaTel = query.value(5).toString();

        area_list.append(ai);
    }

    QString Msg;

    foreach (AreaInfo ai, area_list) {
        Msg.append(QString("<AreaInfo AreaID=\"%1\" AreaBuild=\"%2\" AreaUnit=\"%3\" AreaFloor=\"%4\" AreaRoom=\"%5\" AreaTel=\"%6\" />").arg(ai.AreaID).arg(ai.AreaBuild).arg(ai.AreaUnit).arg(ai.AreaFloor).arg(ai.AreaRoom).arg(ai.AreaTel));
    }

    SendData(Msg);

    CommonSetting::print("返回区域信息");
}

void TcpSocketApi::DeleteAreaInfo(const AreaInfo &area)
{
    QMutexLocker lock(&GlobalConfig::GlobalLock);

    if (!area.AreaID.isEmpty()) {
        QString DeleteSql = QString("DELETE FROM area_info_table WHERE AreaID = '%1'").arg(area.AreaID);

        QSqlQuery query(QSqlDatabase::database(connectionName));
        query.exec(DeleteSql);

        if (query.lastError().type() != QSqlError::NoError) {
            CommonSetting::print("删除区域信息失败 = " + query.lastError().text());
        } else {
            CommonSetting::print("删除区域信息成功");
        }
    }
}

void TcpSocketApi::UpdateAreaInfo(const AreaInfo &area)
{
    QMutexLocker lock(&GlobalConfig::GlobalLock);

    if (!area.AreaID.isEmpty()) {
        QString UpdateSql = QString("UPDATE area_info_table SET AreaBuild = '%1',AreaUnit = '%2',AreaFloor = '%3',AreaRoom = '%4',AreaTel = '%5' WHERE AreaID = '%6'")
                .arg(area.AreaBuild).arg(area.AreaUnit).arg(area.AreaFloor).arg(area.AreaRoom).arg(area.AreaTel).arg(area.AreaID);

        QSqlQuery query(QSqlDatabase::database(connectionName));
        query.exec(UpdateSql);

        if (query.lastError().type() != QSqlError::NoError) {
            CommonSetting::print("更新区域信息失败 = " + query.lastError().text());
        } else {
            CommonSetting::print("更新区域信息成功");
        }
    }
}

void TcpSocketApi::ClearAreaInfo()
{
    QMutexLocker lock(&GlobalConfig::GlobalLock);

    QString DeleteSql = QString("DELETE FROM area_info_table");

    QSqlQuery query(QSqlDatabase::database(connectionName));
    query.exec(DeleteSql);

    if (query.lastError().type() != QSqlError::NoError) {
        CommonSetting::print("清空区域信息失败 = " + query.lastError().text());
    } else {
        CommonSetting::print("清空区域信息成功");
    }
}

void TcpSocketApi::GetFaceServerIP()
{
    QString Msg;
    Msg.append("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
    Msg.append(QString("<DbServer AgentID=\"%1\">").arg(GlobalConfig::AgentID));
    Msg.append(QString("<FaceServerIP>%1</FaceServerIP>").arg(GlobalConfig::FaceServerIP));
    Msg.append("</DbServer>");

    int length = Msg.toLocal8Bit().size();
    Msg = QString("IDOOR:") + QString("%1").arg(length,14,10,QLatin1Char('0')) + Msg;
    this->write(Msg.toLatin1());
}

void TcpSocketApi::SendAckToPlatformCenter(const QString &PersonID, const QString &Status, const QString &Msg)
{
    QString AckMsg;
    AckMsg.append("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
    AckMsg.append(QString("<DbServer AgentID=\"%1\" PersonID=\"%2\" Status=\"%3\" Msg=\"%4\" />")
                  .arg(GlobalConfig::AgentID).arg(PersonID).arg(Status).arg(CommonSetting::toBase64(Msg)));
    int length = AckMsg.toLocal8Bit().size();
    AckMsg = QString("IDOOR:") + QString("%1").arg(length,14,10,QLatin1Char('0')) + AckMsg;
    this->write(AckMsg.toLatin1());
}

void TcpSocketApi::SendAckToDoorDevice(const QString &CompareRecordID)
{
    //xml声明
    QString Msg;
    Msg.append("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
    Msg.append(QString("<DbServer NowTime=\"%1\" CompareRecordID=\"%2\" />").arg(CommonSetting::GetCurrentDateTime()).arg(CompareRecordID));

    int length = Msg.toLocal8Bit().size();
    Msg = QString("IDOOR:") + QString("%1").arg(length,14,10,QLatin1Char('0')) + Msg;

    this->write(Msg.toLatin1());
}

void TcpSocketApi::slotRecvMsg()
{
    if (this->bytesAvailable() <= 0) {
        return;
    }

    QByteArray data = this->readAll();

    LastRecvMsgTime = QDateTime::currentDateTime();

    QMutexLocker lock(&mutex);

    RecvOriginalMsgBuffer.append(data);
}

void TcpSocketApi::slotDisconnect()
{
    emit signalDisconnect();

    this->deleteLater();
}

void TcpSocketApi::slotParseOriginalMsg()
{
    QMutexLocker lock(&mutex);

    ParseOriginalMsgTimer->stop();

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

    ParseOriginalMsgTimer->start();
}

void TcpSocketApi::slotParseVaildMsg()
{
    QMutexLocker lock(&mutex);

    ParseVaildMsgTimer->stop();

    while (this->RecvVaildMsgBuffer.size() > 0) {
        QByteArray data = this->RecvVaildMsgBuffer.takeFirst();

        QDomDocument dom;
        QString errorMsg;
        int errorLine, errorColumn;

        if (!dom.setContent(data, &errorMsg, &errorLine, &errorColumn)) {
            qDebug() << "Parse error: " +  errorMsg << data;
            continue;
        }

        if (data.contains("SelectPersonInfo")) {
            CommonSetting::print(data);
        }

        //心跳
        bool isDeviceHeart = false;

        //人员信息相关变量
        bool isAddPersonInfo = false;
        bool isSelectPersonInfo = false;
        bool isDeletePersonInfo = false;
        bool isUpdatePersonInfo = false;
        bool isClearPersonInfo = false;

        //设备信息相关变量
        bool isAddDeviceInfo = false;
        bool isSelectDeviceInfo = false;
        bool isDelectDeviceInfo = false;
        bool isUpdateDeviceInfo = false;
        bool isClearDeviceInfo = false;

        //查询比对记录相关变量
        bool isSelectCompareRecordInfo = false;
        bool isDeleteCompareRecordInfo = false;
        bool isClearCompareRecordInfo = false;

        //区域信息相关变量
        bool isAddAreaInfo = false;
        bool isSelectAreaInfo = false;
        bool isDeleteAreaInfo = false;
        bool isUpdateAreaInfo = false;
        bool isClearAreaInfo = false;
        bool isResetDeviceInfo = false;

        PersonInfo person;
        PersonPageInfo person_page;
        DeviceInfo device;
        CompareRecordInfo record;
        CompareRecordPageInfo record_page;
        AreaInfo area;

        QDomElement RootElement = dom.documentElement();//获取根元素

        if (RootElement.tagName() == "VisitorServer") {//来自人工/自助访客机的数据包
            QDomNode firstChildNode = RootElement.firstChild();//第一个子节点

            while (!firstChildNode.isNull()) {
                //心跳
                if (firstChildNode.nodeName() == "DeviceHeart") {
                    isDeviceHeart = true;
                }

                //人员信息相关
                if ((firstChildNode.nodeName() == "AddPersonInfo") ||
                        (firstChildNode.nodeName() == "PersonInfo")) {//人工访客机、自助访客机
                    isAddPersonInfo = true;

                    QDomElement AddPersonInfo = firstChildNode.toElement();

                    person.PersonID = CommonSetting::CreateUUID();

                    person.PersonBuild  =
                            CommonSetting::fromBase64(AddPersonInfo.attribute("PersonBuild"));

                    person.PersonUnit   =
                            CommonSetting::fromBase64(AddPersonInfo.attribute("PersonUnit"));

                    person.PersonFloor  =
                            CommonSetting::fromBase64(AddPersonInfo.attribute("PersonFloor"));

                    person.PersonRoom   =
                            CommonSetting::fromBase64(AddPersonInfo.attribute("PersonRoom"));

                    person.PersonName   =
                            CommonSetting::fromBase64(AddPersonInfo.attribute("PersonName"));

                    person.PersonSex    =
                            CommonSetting::fromBase64(AddPersonInfo.attribute("PersonSex"));

                    person.PersonType   =
                            CommonSetting::fromBase64(AddPersonInfo.attribute("PersonType"));

                    person.IDCardNumber =
                            CommonSetting::fromBase64(AddPersonInfo.attribute("IDCardNumber"));

                    person.ICCardNumber  = AddPersonInfo.attribute("ICCardNumber");
                    person.PhoneNumber  = AddPersonInfo.attribute("PhoneNumber");
                    person.PhoneNumber2  = AddPersonInfo.attribute("PhoneNumber2");
                    person.RegisterTime = AddPersonInfo.attribute("RegisterTime");
                    person.ExpiryTime   = AddPersonInfo.attribute("ExpiryTime");
                    person.Blacklist = "N";
                    person.isActivate = "Y";

                    QDomNode firstChildNode = AddPersonInfo.firstChild();//第一个子节点

                    while(!firstChildNode.isNull()){
                        if(firstChildNode.nodeName() == "FeatureValue"){
                            QDomElement firstChildElement = firstChildNode.toElement();
                            person.FeatureValue = firstChildElement.text();
                        }

                        if(firstChildNode.nodeName() == "PersonImage"){
                            QDomElement PersonImage = firstChildNode.toElement();
                            QString PersonImageBase64 = PersonImage.text();

                            if (!PersonImageBase64.isEmpty()) {
                                QImage PersonImage =
                                        CommonSetting::Base64_To_QImage(PersonImageBase64.toLatin1());
                                person.PersonImageUrl = CommonSetting::GetCurrentPath() + QString("images/") + QString("PersonImage_") + person.PersonID + QString(".jpg");
                                bool result = PersonImage.save(person.PersonImageUrl,"JPG");

                                if (!result) {
                                    CommonSetting::WriteCommonFile("save.txt",QString("保存人员生活照图片失败 = %1").arg(person.PersonImageUrl));
                                }
                            }
                        }

                        if(firstChildNode.nodeName() == "IDCardImage"){
                            QDomElement IDCardImage = firstChildNode.toElement();
                            QString IDCardImageBase64 = IDCardImage.text();

                            if (!IDCardImageBase64.isEmpty()) {
                                QImage IDCardImage =
                                        CommonSetting::Base64_To_QImage(IDCardImageBase64.toLatin1());
                                person.IDCardImageUrl = CommonSetting::GetCurrentPath() + QString("images/") + QString("IDCardImage_") + person.PersonID + QString(".jpg");
                                bool result = IDCardImage.save(person.IDCardImageUrl,"JPG");

                                if (!result) {
                                    CommonSetting::WriteCommonFile("save.txt",QString("保存人员证件图片失败 = %1").arg(person.IDCardImageUrl));
                                }
                            }
                        }

                        firstChildNode = firstChildNode.nextSibling();//下一个节点
                    }
                }

                if (firstChildNode.nodeName() == "SelectPersonInfo") {
                    isSelectPersonInfo = true;

                    QDomElement SelectPersonInfo = firstChildNode.toElement();

                    person.PersonID = SelectPersonInfo.attribute("PersonID");

                    person.PersonBuild =
                            CommonSetting::fromBase64(SelectPersonInfo.attribute("PersonBuild"));

                    person.PersonUnit =
                            CommonSetting::fromBase64(SelectPersonInfo.attribute("PersonUnit"));

                    person.PersonFloor =
                            CommonSetting::fromBase64(SelectPersonInfo.attribute("PersonFloor"));

                    person.PersonRoom =
                            CommonSetting::fromBase64(SelectPersonInfo.attribute("PersonRoom"));

                    person.PersonName =
                            CommonSetting::fromBase64(SelectPersonInfo.attribute("PersonName"));

                    person.PersonType =
                            CommonSetting::fromBase64(SelectPersonInfo.attribute("PersonType"));

                    //查询哪一页
                    person_page.PageCurrent =
                            SelectPersonInfo.attribute("PageCurrent").toULongLong();

                    if (person_page.PageCurrent == 0) {
                        person_page.PageCurrent = 1;
                    }

                    //每页多少条记录
                    person_page.ResultCurrent =
                            SelectPersonInfo.attribute("ResultCurrent").toULongLong();

                    if (person_page.ResultCurrent == 0) {
                        person_page.ResultCurrent = 100;
                    }
                }

                if (firstChildNode.nodeName() == "DeletePersonInfo") {
                    isDeletePersonInfo = true;

                    QDomElement DeletePersonInfo = firstChildNode.toElement();

                    person.PersonID = DeletePersonInfo.attribute("PersonID");

                    person.PersonBuild =
                            CommonSetting::fromBase64(DeletePersonInfo.attribute("PersonBuild"));

                    person.PersonUnit =
                            CommonSetting::fromBase64(DeletePersonInfo.attribute("PersonUnit"));

                    person.PersonFloor =
                            CommonSetting::fromBase64(DeletePersonInfo.attribute("PersonFloor"));

                    person.PersonRoom =
                            CommonSetting::fromBase64(DeletePersonInfo.attribute("PersonRoom"));

                    person.PersonName =
                            CommonSetting::fromBase64(DeletePersonInfo.attribute("PersonName"));

                    person.PersonType =
                            CommonSetting::fromBase64(DeletePersonInfo.attribute("PersonType"));
                }

                if (firstChildNode.nodeName() == "UpdatePersonInfo") {
                    isUpdatePersonInfo = true;

                    QDomElement UpdatePersonInfo = firstChildNode.toElement();

                    person.PersonID = UpdatePersonInfo.attribute("PersonID");

                    person.PersonBuild =
                            CommonSetting::fromBase64(UpdatePersonInfo.attribute("PersonBuild"));

                    person.PersonUnit =
                            CommonSetting::fromBase64(UpdatePersonInfo.attribute("PersonUnit"));

                    person.PersonFloor =
                            CommonSetting::fromBase64(UpdatePersonInfo.attribute("PersonFloor"));

                    person.PersonRoom =
                            CommonSetting::fromBase64(UpdatePersonInfo.attribute("PersonRoom"));

                    person.PersonName =
                            CommonSetting::fromBase64(UpdatePersonInfo.attribute("PersonName"));

                    person.PersonSex =
                            CommonSetting::fromBase64(UpdatePersonInfo.attribute("PersonSex"));

                    person.PersonType =
                            CommonSetting::fromBase64(UpdatePersonInfo.attribute("PersonType"));

                    person.IDCardNumber =
                            CommonSetting::fromBase64(UpdatePersonInfo.attribute("IDCardNumber"));

                    person.ICCardNumber = UpdatePersonInfo.attribute("ICCardNumber");

                    person.PhoneNumber = UpdatePersonInfo.attribute("PhoneNumber");
                    person.PhoneNumber2 = UpdatePersonInfo.attribute("PhoneNumber2");

                    person.RegisterTime = UpdatePersonInfo.attribute("RegisterTime");
                    person.ExpiryTime = UpdatePersonInfo.attribute("ExpiryTime");

                    QDomNode firstChildNode = UpdatePersonInfo.firstChild();//第一个子节点

                    while (!firstChildNode.isNull()) {
                        if (firstChildNode.nodeName() == "FeatureValue") {
                            QDomElement firstChildElement = firstChildNode.toElement();
                            person.FeatureValue = firstChildElement.text();
                        }

                        if (firstChildNode.nodeName() == "PersonImage") {
                            QDomElement PersonImage = firstChildNode.toElement();
                            QString PersonImageBase64 = PersonImage.text();

                            if (!PersonImageBase64.isEmpty()) {
                                QImage PersonImage =
                                        CommonSetting::Base64_To_QImage(PersonImageBase64.toLatin1());
                                person.PersonImageUrl = CommonSetting::GetCurrentPath() + QString("images/") + QString("PersonImage_") + person.PersonID + QString(".jpg");
                                bool result = PersonImage.save(person.PersonImageUrl,"JPG");

                                if (!result) {
                                    CommonSetting::WriteCommonFile("save.txt",QString("保存人员生活照图片失败 = %1").arg(person.PersonImageUrl));
                                }
                            }
                        }

                        if (firstChildNode.nodeName() == "IDCardImage") {
                            QDomElement IDCardImage = firstChildNode.toElement();
                            QString IDCardImageBase64 = IDCardImage.text();

                            if (!IDCardImageBase64.isEmpty()) {
                                QImage IDCardImage =
                                        CommonSetting::Base64_To_QImage(IDCardImageBase64.toLatin1());
                                person.IDCardImageUrl = CommonSetting::GetCurrentPath() + QString("images/") + QString("IDCardImage_") + person.PersonID + QString(".jpg");
                                bool result = IDCardImage.save(person.IDCardImageUrl,"JPG");

                                if (!result) {
                                    CommonSetting::WriteCommonFile("save.txt",QString("保存人员证件图片失败 = %1").arg(person.IDCardImageUrl));
                                }
                            }
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

                    device.DeviceID = CommonSetting::CreateUUID();

                    device.DeviceBuild  =
                            CommonSetting::fromBase64(AddDeviceInfo.attribute("DeviceBuild"));

                    device.DeviceUnit  =
                            CommonSetting::fromBase64(AddDeviceInfo.attribute("DeviceUnit"));

                    device.DeviceIP = AddDeviceInfo.attribute("DeviceIP");

                    device.Longitude = AddDeviceInfo.attribute("Longitude");

                    device.Latitude = AddDeviceInfo.attribute("Latitude");

                    device.Altitude = AddDeviceInfo.attribute("Altitude");

                    device.MainStreamRtspAddr = AddDeviceInfo.attribute("MainStreamRtspAddr");

                    device.SubStreamRtspAddr = AddDeviceInfo.attribute("SubStreamRtspAddr");

                    device.isUploadPlatformCenter = QString("N");
                }

                if(firstChildNode.nodeName() == "SelectDeviceInfo"){
                    isSelectDeviceInfo = true;

                    QDomElement SelectDeviceInfo = firstChildNode.toElement();

                    device.DeviceIP = SelectDeviceInfo.attribute("DeviceIP");
                }

                if(firstChildNode.nodeName() == "DeleteDeviceInfo"){
                    isDelectDeviceInfo = true;

                    QDomElement DelectDeviceInfo = firstChildNode.toElement();
                    device.DeviceID = DelectDeviceInfo.attribute("DeviceID");
                }

                if(firstChildNode.nodeName() == "UpdateDeviceInfo"){
                    isUpdateDeviceInfo = true;

                    QDomElement UpdateDeviceInfo = firstChildNode.toElement();

                    device.DeviceID = UpdateDeviceInfo.attribute("DeviceID");

                    device.DeviceBuild  =
                            CommonSetting::fromBase64(UpdateDeviceInfo.attribute("DeviceBuild"));

                    device.DeviceUnit  =
                            CommonSetting::fromBase64(UpdateDeviceInfo.attribute("DeviceUnit"));

                    device.DeviceIP = UpdateDeviceInfo.attribute("DeviceIP");

                    device.Longitude = UpdateDeviceInfo.attribute("Longitude");

                    device.Latitude = UpdateDeviceInfo.attribute("Latitude");

                    device.Altitude = UpdateDeviceInfo.attribute("Altitude");

                    device.MainStreamRtspAddr = UpdateDeviceInfo.attribute("MainStreamRtspAddr");

                    device.SubStreamRtspAddr = UpdateDeviceInfo.attribute("SubStreamRtspAddr");

                    device.isUploadPlatformCenter = QString("N");
                }

                if(firstChildNode.nodeName() == "ClearDeviceInfo"){
                    isClearDeviceInfo = true;
                }

                if(firstChildNode.nodeName() == "ResetDeviceInfo"){
                    isResetDeviceInfo = true;

                    QDomElement ResetDeviceInfo = firstChildNode.toElement();

                    device.DeviceID = ResetDeviceInfo.attribute("DeviceID");
                }

                //查询比对记录相关
                if(firstChildNode.nodeName() == "SelectCompareRecordInfo"){
                    isSelectCompareRecordInfo = true;

                    QDomElement SelectCompareRecordInfo = firstChildNode.toElement();

                    record.CompareRecordID = SelectCompareRecordInfo.attribute("CompareRecordID");

                    record.CompareResult = CommonSetting::fromBase64(SelectCompareRecordInfo.attribute("CompareResult"));

                    record.PersonBuild  = CommonSetting::fromBase64(SelectCompareRecordInfo.attribute("PersonBuild"));

                    record.PersonUnit   = CommonSetting::fromBase64(SelectCompareRecordInfo.attribute("PersonUnit"));

                    record.PersonFloor  = CommonSetting::fromBase64(SelectCompareRecordInfo.attribute("PersonFloor"));

                    record.PersonRoom   = CommonSetting::fromBase64(SelectCompareRecordInfo.attribute("PersonRoom"));

                    record.PersonName   = CommonSetting::fromBase64(SelectCompareRecordInfo.attribute("PersonName"));

                    record.PersonType   = CommonSetting::fromBase64(SelectCompareRecordInfo.attribute("PersonType"));

                    record.TriggerTime = SelectCompareRecordInfo.attribute("TriggerTime");

                    //查询哪一页
                    record_page.PageCurrent =
                            SelectCompareRecordInfo.attribute("PageCurrent").toULongLong();

                    if (record_page.PageCurrent == 0) {
                        record_page.PageCurrent = 1;
                    }

                    //每页多少条记录
                    record_page.ResultCurrent =
                            SelectCompareRecordInfo.attribute("ResultCurrent").toULongLong();

                    if (record_page.ResultCurrent == 0) {
                        record_page.ResultCurrent = 100;
                    }
                }

                if(firstChildNode.nodeName() == "DeleteCompareRecordInfo"){
                    isDeleteCompareRecordInfo = true;

                    QDomElement DeleteCompareRecordInfo = firstChildNode.toElement();

                    record.CompareRecordID = DeleteCompareRecordInfo.attribute("CompareRecordID");

                    record.CompareResult = CommonSetting::fromBase64(DeleteCompareRecordInfo.attribute("CompareResult"));

                    record.PersonBuild  = CommonSetting::fromBase64(DeleteCompareRecordInfo.attribute("PersonBuild"));

                    record.PersonUnit   = CommonSetting::fromBase64(DeleteCompareRecordInfo.attribute("PersonUnit"));

                    record.PersonFloor  = CommonSetting::fromBase64(DeleteCompareRecordInfo.attribute("PersonFloor"));

                    record.PersonRoom   = CommonSetting::fromBase64(DeleteCompareRecordInfo.attribute("PersonRoom"));

                    record.PersonName   = CommonSetting::fromBase64(DeleteCompareRecordInfo.attribute("PersonName"));

                    record.PersonType   = CommonSetting::fromBase64(DeleteCompareRecordInfo.attribute("PersonType"));

                    record.TriggerTime = DeleteCompareRecordInfo.attribute("TriggerTime");
                }

                if(firstChildNode.nodeName() == "ClearCompareRecordInfo"){
                    isClearCompareRecordInfo = true;
                }

                //区域信息相关
                if(firstChildNode.nodeName() == "AddAreaInfo"){
                    isAddAreaInfo = true;

                    QDomElement AddAreaInfo = firstChildNode.toElement();

                    area.AreaID = CommonSetting::CreateUUID();

                    area.AreaBuild =
                            CommonSetting::fromBase64(AddAreaInfo.attribute("AreaBuild"));

                    area.AreaUnit =
                            CommonSetting::fromBase64(AddAreaInfo.attribute("AreaUnit"));

                    area.AreaFloor =
                            CommonSetting::fromBase64(AddAreaInfo.attribute("AreaFloor"));

                    area.AreaRoom =
                            CommonSetting::fromBase64(AddAreaInfo.attribute("AreaRoom"));

                    area.AreaTel = AddAreaInfo.attribute("AreaTel");
                }

                if(firstChildNode.nodeName() == "SelectAreaInfo"){
                    isSelectAreaInfo = true;
                }

                if(firstChildNode.nodeName() == "DeleteAreaInfo"){
                    isDeleteAreaInfo = true;

                    QDomElement DeleteAreaInfo = firstChildNode.toElement();

                    area.AreaID = DeleteAreaInfo.attribute("AreaID");
                }

                if(firstChildNode.nodeName() == "UpdateAreaInfo"){
                    isUpdateAreaInfo = true;

                    QDomElement UpdateAreaInfo = firstChildNode.toElement();

                    area.AreaID = UpdateAreaInfo.attribute("AreaID");

                    area.AreaBuild =
                            CommonSetting::fromBase64(UpdateAreaInfo.attribute("AreaBuild"));

                    area.AreaUnit =
                            CommonSetting::fromBase64(UpdateAreaInfo.attribute("AreaUnit"));

                    area.AreaFloor =
                            CommonSetting::fromBase64(UpdateAreaInfo.attribute("AreaFloor"));

                    area.AreaRoom =
                            CommonSetting::fromBase64(UpdateAreaInfo.attribute("AreaRoom"));

                    area.AreaTel = UpdateAreaInfo.attribute("AreaTel");
                }

                if(firstChildNode.nodeName() == "ClearAreaInfo"){
                    isClearAreaInfo = true;
                }

                firstChildNode = firstChildNode.nextSibling();//下一个节点
            }

            if (isDeviceHeart) {
                CommonSetting::print("接收心跳");

                SendDeviceHeart();
            }

            if (isAddPersonInfo) {
                CommonSetting::print("添加人员信息");

                AddPersonInfo(person);
            }

            if (isSelectPersonInfo) {
                CommonSetting::print("获取人员信息");

                SelectPersonInfo(person,person_page);
            }

            if (isDeletePersonInfo) {
                CommonSetting::print("删除人员信息");

                DeletePersonInfo(person);
            }

            if (isUpdatePersonInfo) {
                CommonSetting::print("更新人员信息");

                UpdatePersonInfo(person);
            }

            if (isClearPersonInfo) {
                CommonSetting::print("清空人员信息");

                ClearPersonInfo();
            }

            if (isAddDeviceInfo) {
                CommonSetting::print("添加执行器信息");

                AddDeviceInfo(device);
            }

            if (isSelectDeviceInfo) {
                CommonSetting::print("查询执行器信息");

                SelectDeviceInfo(device);
            }

            if (isDelectDeviceInfo) {
                CommonSetting::print("删除执行器信息");

                DelectDeviceInfo(device);
            }

            if (isUpdateDeviceInfo) {
                CommonSetting::print("更新执行器信息");

                UpdateDeviceInfo(device);
            }

            if (isClearDeviceInfo) {
                CommonSetting::print("清空执行器信息");

                ClearDeviceInfo();
            }

            if (isResetDeviceInfo) {
                CommonSetting::print("同步设备");

                ResetDeviceInfo(device);
            }

            if (isSelectCompareRecordInfo) {
                CommonSetting::print("查询比对记录信息");

                SelectCompareRecordInfo(record,record_page);
            }

            if (isDeleteCompareRecordInfo) {
                CommonSetting::print("删除比对记录信息");

                DeleteCompareRecordInfo(record);
            }

            if (isClearCompareRecordInfo) {
                CommonSetting::print("清空比对记录信息");

                ClearCompareRecordInfo();
            }

            if (isAddAreaInfo) {
                CommonSetting::print("添加区域信息");

                AddAreaInfo(area);
            }

            if (isSelectAreaInfo) {
                CommonSetting::print("查询区域信息");

                SelectAreaInfo();
            }

            if (isDeleteAreaInfo) {
                CommonSetting::print("删除区域信息");

                DeleteAreaInfo(area);
            }

            if (isUpdateAreaInfo) {
                CommonSetting::print("更新区域信息");

                UpdateAreaInfo(area);
            }

            if (isClearAreaInfo) {
                CommonSetting::print("清空区域信息");

                ClearAreaInfo();
            }
        }

        if (RootElement.tagName() == "PlatformCenter") {//来自敏达平台的数据包
            if (RootElement.hasAttribute("NowTime")) {
                QString NowTime = RootElement.attribute("NowTime");
                CommonSetting::SettingSystemDateTime(NowTime);
            }

            if (RootElement.hasAttribute("AgentID")) {
                QString AgentID = RootElement.attribute("AgentID");
                if (AgentID == GlobalConfig::AgentID) {
                    QDomNode firstChildNode = RootElement.firstChild();//第一个子节点

                    while (!firstChildNode.isNull()) {
                        if (firstChildNode.nodeName() == "GetFaceServerIP ") {
                            GetFaceServerIP();
                        }

                        if (firstChildNode.nodeName() == "AddPersonInfo") {
                            QDomElement AddPersonInfo = firstChildNode.toElement();

                            person.PersonID = AddPersonInfo.attribute("PersonID");

                            person.PersonBuild  =
                                    CommonSetting::fromBase64(AddPersonInfo.attribute("PersonBuild"));

                            person.PersonUnit   =
                                    CommonSetting::fromBase64(AddPersonInfo.attribute("PersonUnit"));

                            person.PersonFloor  =
                                    CommonSetting::fromBase64(AddPersonInfo.attribute("PersonFloor"));

                            person.PersonRoom   =
                                    CommonSetting::fromBase64(AddPersonInfo.attribute("PersonRoom"));

                            person.PersonName   =
                                    CommonSetting::fromBase64(AddPersonInfo.attribute("PersonName"));

                            person.PersonSex    =
                                    CommonSetting::fromBase64(AddPersonInfo.attribute("PersonSex"));

                            person.PersonType   =
                                    CommonSetting::fromBase64(AddPersonInfo.attribute("PersonType"));

                            person.IDCardNumber =
                                    CommonSetting::fromBase64(AddPersonInfo.attribute("IDCardNumber"));

                            person.ICCardNumber  = AddPersonInfo.attribute("ICCardNumber");
                            person.PhoneNumber  = AddPersonInfo.attribute("PhoneNumber");
                            person.PhoneNumber2  = AddPersonInfo.attribute("PhoneNumber2");
                            person.RegisterTime = AddPersonInfo.attribute("RegisterTime");
                            person.ExpiryTime   = AddPersonInfo.attribute("ExpiryTime");
                            person.Blacklist = AddPersonInfo.attribute("Blacklist");
                            person.isActivate = AddPersonInfo.attribute("isActivate");

                            QDomNode firstChildNode = AddPersonInfo.firstChild();//第一个子节点

                            while(!firstChildNode.isNull()){
                                if(firstChildNode.nodeName() == "FeatureValue"){
                                    QDomElement firstChildElement = firstChildNode.toElement();
                                    person.FeatureValue = firstChildElement.text();
                                }

                                if(firstChildNode.nodeName() == "PersonImage"){
                                    QDomElement PersonImage = firstChildNode.toElement();
                                    QString PersonImageBase64 = PersonImage.text();

                                    if (!PersonImageBase64.isEmpty()) {
                                        QImage PersonImage =
                                                CommonSetting::Base64_To_QImage(PersonImageBase64.toLatin1());
                                        person.PersonImageUrl = CommonSetting::GetCurrentPath() + QString("images/") + QString("PersonImage_") + person.PersonID + QString(".jpg");
                                        bool result = PersonImage.save(person.PersonImageUrl,"JPG");

                                        if (!result) {
                                            CommonSetting::WriteCommonFile("save.txt",QString("保存人员生活照图片失败 = %1").arg(person.PersonImageUrl));
                                        }
                                    }
                                }

                                if(firstChildNode.nodeName() == "IDCardImage"){
                                    QDomElement IDCardImage = firstChildNode.toElement();
                                    QString IDCardImageBase64 = IDCardImage.text();

                                    if (!IDCardImageBase64.isEmpty()) {
                                        QImage IDCardImage =
                                                CommonSetting::Base64_To_QImage(IDCardImageBase64.toLatin1());
                                        person.IDCardImageUrl = CommonSetting::GetCurrentPath() + QString("images/") + QString("IDCardImage_") + person.PersonID + QString(".jpg");
                                        bool result = IDCardImage.save(person.IDCardImageUrl,"JPG");

                                        if (!result) {
                                            CommonSetting::WriteCommonFile("save.txt",QString("保存人员证件图片失败 = %1").arg(person.IDCardImageUrl));
                                        }
                                    }
                                }

                                firstChildNode = firstChildNode.nextSibling();//下一个节点
                            }

                            QString Status,Msg;

                            if (person.FeatureValue.isEmpty()) {//人脸特征值为空
                                Status = QString("FAIL");
                                Msg = QString("人脸特征值为空");
                            } else if (person.FeatureValue.split("|").size() != 256) {//人脸特征值损坏
                                Status = QString("FAIL");
                                Msg = QString("人脸特征值损坏");
                            } else {//人脸特征值正常
                                Status = QString("SUCC");
                                Msg = QString("人脸特征值正常");

                                AddPersonInfoFromPlatformCenter(person);
                            }

                            SendAckToPlatformCenter(person.PersonID,Status,Msg);
                        }

                        firstChildNode = firstChildNode.nextSibling();//下一个节点
                    }
                }
            }
        }

        if (RootElement.tagName() == "DoorDevice") {//来自门禁执行器的数据包
            QDomNode firstChildNode = RootElement.firstChild();//第一个子节点

            while (!firstChildNode.isNull()) {
                CompareRecordInfo record;

                if (firstChildNode.nodeName() == "CompareRecordInfo") {
                    QDomElement CompareRecordInfo = firstChildNode.toElement();

                    record.CompareRecordID = CompareRecordInfo.attribute("CompareRecordID");

                    record.CompareResult =
                            CommonSetting::fromBase64(CompareRecordInfo.attribute("CompareResult"));

                    record.PersonBuild =
                            CommonSetting::fromBase64(CompareRecordInfo.attribute("PersonBuild"));

                    record.PersonUnit =
                            CommonSetting::fromBase64(CompareRecordInfo.attribute("PersonUnit"));

                    record.PersonFloor =
                            CommonSetting::fromBase64(CompareRecordInfo.attribute("PersonFloor"));
                    record.PersonRoom =

                            CommonSetting::fromBase64(CompareRecordInfo.attribute("PersonRoom"));

                    record.PersonName =
                            CommonSetting::fromBase64(CompareRecordInfo.attribute("PersonName"));

                    record.PersonSex =
                            CommonSetting::fromBase64(CompareRecordInfo.attribute("PersonSex"));
                    record.PersonType =

                            CommonSetting::fromBase64(CompareRecordInfo.attribute("PersonType"));

                    record.IDCardNumber =
                            CommonSetting::fromBase64(CompareRecordInfo.attribute("IDCardNumber"));

                    record.ICCardNumber = CompareRecordInfo.attribute("ICCardNumber");

                    record.PhoneNumber = CompareRecordInfo.attribute("PhoneNumber");

                    record.ExpiryTime = CompareRecordInfo.attribute("ExpiryTime");

                    record.Blacklist = CompareRecordInfo.attribute("Blacklist");

                    record.isActivate = CompareRecordInfo.attribute("isActivate");

                    record.FaceSimilarity = CompareRecordInfo.attribute("FaceSimilarity");

                    record.UseTime = CompareRecordInfo.attribute("UseTime");

                    record.TriggerTime = CompareRecordInfo.attribute("TriggerTime");

                    QDomNode firstChildNode = CompareRecordInfo.firstChild();//第一个子节点

                    while (!firstChildNode.isNull()) {
                        if (firstChildNode.nodeName() == "EnterSnapPic") {
                            QDomElement EnterSnapPic = firstChildNode.toElement();

                            QString EnterSnapPicBase64 = EnterSnapPic.text();

                            if (!EnterSnapPicBase64.isEmpty()) {
                                QImage EnterSnapPicImage =
                                        CommonSetting::Base64_To_QImage(EnterSnapPicBase64.toLatin1());

                                record.EnterSnapPicUrl = CommonSetting::GetCurrentPath() + QString("log/") + QString("EnterSnapPic_") + record.CompareRecordID + QString(".jpg");

                                bool result = EnterSnapPicImage.save(record.EnterSnapPicUrl,"JPG");

                                if (!result) {
                                    CommonSetting::WriteCommonFile("save.txt",QString("保存比对记录现场抓拍图片失败 = %1").arg(record.EnterSnapPicUrl));
                                }
                            }
                        }

                        if(firstChildNode.nodeName() == "OriginalSnapPic"){
                            QDomElement OriginalSnapPic = firstChildNode.toElement();

                            QString OriginalSnapPicBase64 = OriginalSnapPic.text();

                            if (!OriginalSnapPicBase64.isEmpty()) {
                                QImage OriginalSnapPicImage =
                                        CommonSetting::Base64_To_QImage(OriginalSnapPicBase64.toLatin1());
                                record.OriginalSnapPicUrl = CommonSetting::GetCurrentPath() + QString("log/") + QString("OriginalSnapPic_") + record.CompareRecordID + QString(".jpg");
                                bool result =
                                        OriginalSnapPicImage.save(record.OriginalSnapPicUrl,"JPG");

                                if (!result) {
                                    CommonSetting::WriteCommonFile("save.txt",QString("保存比对记录注册图片失败 = %1").arg(record.OriginalSnapPicUrl));
                                }
                            }
                        }

                        firstChildNode = firstChildNode.nextSibling();//下一个节点
                    }

                    QMutexLocker lock(&GlobalConfig::GlobalLock);

                    //将人脸比对记录信息保存到数据库中
                    QString InsertSql = QString("INSERT INTO compare_record_info_table VALUES('%1','%2','%3','%4','%5','%6','%7','%8','%9','%10','%11','%12','%13','%14','%15','%16','%17','%18','%19','%20','%21')").arg(record.CompareRecordID).arg(record.CompareResult).arg(record.PersonBuild).arg(record.PersonUnit).arg(record.PersonFloor).arg(record.PersonRoom).arg(record.PersonName).arg(record.PersonSex).arg(record.PersonType).arg(record.IDCardNumber).arg(record.ICCardNumber).arg(record.PhoneNumber).arg(record.ExpiryTime).arg(record.Blacklist).arg(record.isActivate).arg(record.FaceSimilarity).arg(record.UseTime).arg(record.TriggerTime).arg(record.EnterSnapPicUrl).arg(record.OriginalSnapPicUrl).arg("N");

                    QSqlQuery query(QSqlDatabase::database(connectionName));
                    query.exec(InsertSql);

                    if (query.lastError().type() != QSqlError::NoError) {
                        CommonSetting::print(QString("保存比对记录到数据库失败 = %1,来自执行器[%2]").arg(query.lastError().text()).arg(this->peerAddress().toString()));
                    } else {
                        CommonSetting::print(QString("保存比对记录到数据库成功,来自执行器[%1]").arg(this->peerAddress().toString()));
                    }

                    //发反馈给前端门禁执行器设备
                    SendAckToDoorDevice(record.CompareRecordID);
                }

                firstChildNode = firstChildNode.nextSibling();//下一个节点
            }
        }
    }

    ParseVaildMsgTimer->start();
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

    connectToHost(InsignItemIp,GlobalConfig::SendPort);
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

        }
    }
}

