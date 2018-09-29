#ifndef COMMONSETTING_H
#define COMMONSETTING_H
#include <QObject>
#include <QSettings>
#include <QDomDocument>
#include <QSqlError>
#include <QStringList>
#include <QList>
#include <QBuffer>
#include <QThread>
#include <QNetworkInterface>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QUdpSocket>
#include <QTextCodec>
#include <QPushButton>
#include <QDateTime>
#include <QProcess>
#include <QUrl>
#include <QDir>
#include <QTimer>
#include <QApplication>
#include <QTextStream>
#include <QDebug>
#include <QWidget>
#include <QImage>
#include <QMutexLocker>
#include <QTcpServer>
#include <QTcpSocket>
#include <QPainter>
#include <QUuid>
#include <QMessageBox>

#if (QT_VERSION >= QT_VERSION_CHECK(5,0,0))
#include <QtConcurrent>
#include <QFuture>
#endif

#define TIMES qPrintable(QTime::currentTime().toString("hh:mm:ss zzz"))

class CommonSetting : public QObject
{
public:
    CommonSetting();
    ~CommonSetting();

    //设置编码为UTF8
    static void SetUTF8Code()
    {
        QTextCodec *codec= QTextCodec::codecForName("UTF-8");
        QTextCodec::setCodecForLocale(codec);

#if (QT_VERSION < QT_VERSION_CHECK(5,0,0))
        QTextCodec::setCodecForCStrings(codec);
        QTextCodec::setCodecForTr(codec);
#endif
    }

    static void OpenDataBase()
    {
        QString dbFile = CommonSetting::GetCurrentPath() + QString("database/FaceDbServer.db");

        if (!CommonSetting::FileExists(dbFile)) {
            CommonSetting::print("数据库不存在");
        } else {
            QSqlDatabase DbConn = QSqlDatabase::addDatabase("QSQLITE");
            DbConn.setDatabaseName(dbFile);

            if (!DbConn.open()) {
                CommonSetting::print("打开数据库失败");
            } else {
#if 0
                //判断表是否存在
                QStringList TableList = DbConn.tables();
                if (!TableList.contains("area_info_table")) {//不存在，则创建
                    QString CreateTable = QString("CREATE TABLE area_info_table ("
                                                  "AreaID    TEXT PRIMARY KEY NOT NULL,"
                                                  "AreaBuild TEXT NOT NULL,"
                                                  "AreaUnit  TEXT NOT NULL,"
                                                  "AreaLevel TEXT NOT NULL,"
                                                  "AreaRoom  TEXT NOT NULL);");

                    CommonSetting::SqlQueryHelper(CreateTable, QString("创建区域信息表"));
                }

                if (!TableList.contains("compare_record_info_table")) {//不存在，则创建
                    QString CreateTable = QString("CREATE TABLE compare_record_info_table ("
                                                  "CompareRecordID        TEXT NOT NULL PRIMARY KEY,"
                                                  "CompareResult          TEXT NOT NULL,"
                                                  "PersonBuild            TEXT,"
                                                  "PersonUnit             TEXT,"
                                                  "PersonLevel            TEXT,"
                                                  "PersonRoom             TEXT,"
                                                  "PersonName             TEXT,"
                                                  "PersonSex              TEXT,"
                                                  "PersonType             TEXT,"
                                                  "IDCardNumber           TEXT,"
                                                  "PhoneNumber            TEXT,"
                                                  "ExpiryTime             TEXT,"
                                                  "Blacklist              TEXT,"
                                                  "isActivate             TEXT,"
                                                  "FaceSimilarity         TEXT,"
                                                  "UseTime                TEXT,"
                                                  "TriggerTime            TEXT,"
                                                  "EnterSnapPicUrl        TEXT,"
                                                  "OriginalSnapPicUrl     TEXT,"
                                                  "isUploadPlatformCenter TEXT NOT NULL);");

                    CommonSetting::SqlQueryHelper(CreateTable, QString("创建比对记录信息表"));
                }

                if (!TableList.contains("device_info_table")) {//不存在，则创建
                    QString CreateTable = QString("CREATE TABLE device_info_table ("
                                                  "DeviceID               TEXT PRIMARY KEY　NOT NULL,"
                                                  "DeviceBuild            TEXT NOT NULL,"
                                                  "DeviceUnit             TEXT NOT NULL,"
                                                  "DeviceIP               TEXT NOT NULL UNIQUE,"
                                                  "MainStreamRtspAddr     TEXT,"
                                                  "SubStreamRtspAddr      TEXT,"
                                                  "isUploadPlatformCenter TEXT NOT NULL);");

                    CommonSetting::SqlQueryHelper(CreateTable, QString("创建设备信息表"));
                }

                if (!TableList.contains("person_info_table")) {//不存在，则创建
                    QString CreateTable = QString("CREATE TABLE person_info_table ("
                                                  "PersonID               TEXT NOT NULL PRIMARY KEY,"
                                                  "PersonBuild            TEXT NOT NULL,"
                                                  "PersonUnit             TEXT NOT NULL,"
                                                  "PersonLevel            TEXT NOT NULL,"
                                                  "PersonRoom             TEXT NOT NULL,"
                                                  "PersonName             TEXT NOT NULL,"
                                                  "PersonSex              TEXT NOT NULL,"
                                                  "PersonType             TEXT NOT NULL,"
                                                  "IDCardNumber           TEXT,"
                                                  "PhoneNumber            TEXT,"
                                                  "RegisterTime           TEXT NOT NULL,"
                                                  "ExpiryTime             TEXT NOT NULL,"
                                                  "FeatureValue           TEXT,"
                                                  "PersonImageUrl         TEXT,"
                                                  "IDCardImageUrl         TEXT,"
                                                  "Blacklist              TEXT NOT NULL,"
                                                  "isActivate             TEXT NOT NULL,"
                                                  "isUploadMainEntrance   TEXT NOT NULL,"
                                                  "isUploadSubEntrance         NOT NULL,"
                                                  "isUploadPlatformCenter      NOT NULL);");

                    CommonSetting::SqlQueryHelper(CreateTable, QString("创建人员信息表"));
                }
#endif
            }
        }
    }

    static void SqlQueryHelper(const QString &sql,const QString &msg)
    {
        QSqlQuery query;
        query.exec(sql);

        CommonSetting::print(msg);

        if (query.lastError().type() != QSqlError::NoError) {
            CommonSetting::print(query.lastError().text());
        }
    }

    static QSqlDatabase createConnection(const QString &connectionName)
    {
        // 创建一个新的连接
        QString dbFile = CommonSetting::GetCurrentPath() + QString("database/FaceDbServer.db");

        if (!CommonSetting::FileExists(dbFile)) {
            CommonSetting::print("数据库不存在");
            CommonSetting::print("创建数据库连接失败");

            return QSqlDatabase();
        } else {
            QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connectionName);

            db.setDatabaseName(dbFile);

            if (!db.open()) {
                CommonSetting::print("打开数据库失败");
                CommonSetting::print("创建数据库连接失败");
                return QSqlDatabase();
            }

            CommonSetting::print("创建数据库连接成功");

            return db;
        }
    }

    static void closeConnection(const QString &connectionName)
    {
        QSqlDatabase::removeDatabase(connectionName);

        CommonSetting::print("关闭数据库连接成功");
    }

    //获取当前日期时间星期
    static QString GetCurrentDateTime()
    {
        QDateTime time = QDateTime::currentDateTime();
        return time.toString("yyyy-MM-dd hh:mm:ss");
    }

    //读取文件内容
    static QString ReadFile(QString fileName)
    {
        QFile file(fileName);
        QByteArray fileContent;
        if (file.open(QIODevice::ReadOnly)){
            fileContent = file.readAll();
        }
        file.close();
        return QString(fileContent);
    }

    //写数据到文件
    static void WriteCommonFile(QString fileName,QString data)
    {
        QFile file(fileName);
        if(file.open(QFile::ReadWrite | QFile::Append)){
            file.write(data.toLocal8Bit().data());
            file.close();
        }
    }

    static void WriteCommonFileTruncate(QString fileName,QString data)
    {
        QFile file(fileName);
        if(file.open(QFile::ReadWrite | QFile::Truncate)){
            file.write(data.toLatin1().data());
            file.close();
        }
    }

    static void WriteXmlFile(QString fileName,QString data)
    {
        QFile file(fileName);
        if(file.open(QFile::ReadWrite | QFile::Append)){
            QDomDocument dom;
            QTextStream out(&file);
            out.setCodec("UTF-8");
            dom.setContent(data);
            dom.save(out,4,QDomNode::EncodingFromTextStream);
            file.close();
        }
    }

    //创建文件夹
    static void CreateFolder(QString path,QString strFolder)
    {
        QDir dir(path);
        dir.mkdir(strFolder);
    }

    //删除文件夹
    static bool deleteFolder(const QString &dirName)
    {
        QDir directory(dirName);
        if (!directory.exists()){
            return true;
        }

        QString srcPath =
                QDir::toNativeSeparators(dirName);
        if (!srcPath.endsWith(QDir::separator()))
            srcPath += QDir::separator();

        QStringList fileNames =
                directory.entryList(QDir::AllEntries |
                                    QDir::NoDotAndDotDot | QDir::Hidden);
        bool error = false;
        for (QStringList::size_type i=0; i != fileNames.size(); ++i){
            QString filePath = srcPath +
                    fileNames.at(i);
            QFileInfo fileInfo(filePath);
            if(fileInfo.isFile() ||
                    fileInfo.isSymLink()){
                QFile::setPermissions(filePath, QFile::WriteOwner);
                if (!QFile::remove(filePath)){
                    qDebug() << "remove file" << filePath << " faild!";
                    error = true;
                }
            }else if (fileInfo.isDir()){
                if (!deleteFolder(filePath)){
                    error = true;
                }
            }
        }

        if (!directory.rmdir(
                    QDir::toNativeSeparators(
                        directory.path()))){
            qDebug() << "remove dir" << directory.path() << " faild!";
            error = true;
        }

        return !error;
    }

    //返回指定路径下符合筛选条件的文件，注意只返回文件名，不返回绝对路径
    static QStringList GetFileNames(QString path,QString filter)
    {
        QDir dir;
        dir.setPath(path);
        QStringList fileFormat(filter);
        dir.setNameFilters(fileFormat);
        dir.setFilter(QDir::Files);
        return dir.entryList();
    }

    //返回指定路径下文件夹的集合,注意只返回文件夹名，不返回绝对路径
    static QStringList GetDirNames(QString path)
    {
        QDir dir(path);
        dir.setFilter(QDir::Dirs);
        QStringList dirlist = dir.entryList();
        QStringList dirNames;
        foreach(const QString &dirName,dirlist){
            if(dirName == "." || dirName == "..")
                continue;
            dirNames << dirName;
        }

        if (dirNames.size() > 5) {
            return dirNames.mid(0,5);
        }

        return dirNames;
    }

    //QSetting应用
    static void WriteSettings(const QString &ConfigFile,
                              const QString &key,
                              const QString value)
    {

        QSettings settings(ConfigFile,QSettings::IniFormat);
        settings.setValue(key,value);
    }

    static QString ReadSettings(const QString &ConfigFile,
                                const QString &key)
    {
        QSettings settings(ConfigFile,QSettings::IniFormat);
        settings.setIniCodec("UTF-8");
        return settings.value(key).toString();
    }

    //将指定路径下指定格式的文件返回(只返回文件名，不返回绝对路径)
    static QStringList fileFilter(const QString &path,
                                  const QString &filter)
    {
        QDir dir(path);
        QStringList fileFormat(filter);
        dir.setNameFilters(fileFormat);
        dir.setSorting(QDir::Time);
        dir.setFilter(QDir::Files);
        QStringList fileList = dir.entryList();
        return fileList;
    }

    //获取当前路径
    static QString GetCurrentPath()
    {
        return QString(QApplication::applicationDirPath() + "/");
    }

    //文件是否存在
    static bool FileExists(QString strFile)
    {
        QFile tempFile(strFile);
        if (tempFile.exists()){
            return true;
        }
        return false;
    }

    //文件夹是否存在
    static bool DirExists(QString strDir)
    {
        QDir tempDir(strDir);
        if (tempDir.exists()){
            return true;
        }
        return false;
    }

    static void SettingSystemDateTime(QString SystemDate)
    {
        //设置系统时间
        QString year = SystemDate.mid(0,4);
        QString month = SystemDate.mid(5,2);
        QString day = SystemDate.mid(8,2);
        QString hour = SystemDate.mid(11,2);
        QString minute = SystemDate.mid(14,2);
        QString second = SystemDate.mid(17,2);
        //        QProcess *process = new QProcess;
        //        process->start(tr("date %1%2%3%4%5.%6").arg(month).arg(day)
        //                       .arg(hour).arg(minute)
        //                       .arg(year).arg(second));
        //        process->waitForFinished();
        //        process->start("hwclock -w");
        //        process->waitForFinished();
        //        delete process;

        system(tr("date %1%2%3%4%5.%6").arg(month).arg(day)
               .arg(hour).arg(minute)
               .arg(year).arg(second).toLatin1().data());

        //        system("hwclock -w");
    }

    static void Sleep(quint16 msec)
    {
        QDateTime dieTime =  QDateTime::currentDateTime().addMSecs(msec);

        while (QDateTime::currentDateTime() < dieTime) {
            QApplication::processEvents(QEventLoop::AllEvents, 100);
        }
    }

    static QString GetLocalHostIP()
    {
        QList<QNetworkInterface> list = QNetworkInterface::allInterfaces();
        foreach(const QNetworkInterface &interface,list){
            if(interface.name() == "br0"){
                QList<QNetworkAddressEntry> entrylist = interface.addressEntries();
                foreach (const QNetworkAddressEntry &entry, entrylist) {
                    QHostAddress address = entry.ip();
                    if ((address.protocol() == QAbstractSocket::IPv4Protocol)
                            && (address != QHostAddress::Null)) {
                        return address.toString();
                    }
                }
            }
        }

        return QString("");
    }


    static QString GetMask()
    {
        QList<QNetworkInterface> list = QNetworkInterface::allInterfaces();
        foreach(const QNetworkInterface &interface,list){
            if(interface.name() == "br0"){
                QList<QNetworkAddressEntry> entrylist = interface.addressEntries();
                foreach (const QNetworkAddressEntry &entry, entrylist) {
                    QHostAddress address = entry.netmask();
                    if ((address.protocol() == QAbstractSocket::IPv4Protocol)
                            && (address != QHostAddress::Null)) {
                        return address.toString();
                    }
                }
            }
        }

        return QString("");
    }

    static QString GetGateway()
    {
        system("rm -rf gw.txt");
        system("route -n | grep 'UG' | awk '{print $2}' > gw.txt");

        QString gw;

        QFile file("gw.txt");
        if(file.open(QFile::ReadOnly)){
            gw = QString(file.readAll()).trimmed().simplified();
            file.close();
        }

        return gw;
    }

    static QString GetDNS()
    {
        system("rm -rf dns.txt");
        system("cat /etc/resolv.conf | grep nameserver | awk '{print $2}' > dns.txt");

        QString dns;

        QFile file("dns.txt");
        if(file.open(QFile::ReadOnly)){
            dns = QString(file.readAll()).trimmed().simplified();
            file.close();
        }

        return dns;
    }

    static QString GetMacAddress()
    {
        QList<QNetworkInterface> list = QNetworkInterface::allInterfaces();
        foreach(const QNetworkInterface &interface,list){
            if(interface.name() == "br0"){
                return interface.hardwareAddress();
            }
        }

        return QString("");
    }

    //将QImage转换成base64
    static QByteArray QImage_To_Base64(const QImage &image)
    {
        QByteArray tempData;
        QBuffer tempBuffer(&tempData);
        image.save(&tempBuffer,"JPG");//按照JPG解码保存数据
        QByteArray imgBase64 = tempData.toBase64();

        return imgBase64;
    }

    //将base64转换QImage
    static QImage Base64_To_QImage(const QByteArray &imgBase64)
    {
        QByteArray tempData = QByteArray::fromBase64(imgBase64);

        QImage tempImage;
        tempImage.loadFromData(tempData);

        return tempImage;
    }

    static QImage ImageRotate(QImage image, qreal value)
    {
        QMatrix matrix;
        matrix.rotate(value);
        return image.transformed(matrix,Qt::FastTransformation);
    }

    //将中文转换成base64
    static QString toBase64(const QString &msg)
    {
        return msg.toLocal8Bit().toBase64();
    }

    //将base64转换成中文
    static QString fromBase64(const QString &msg)
    {
        return QString(QByteArray::fromBase64(msg.toLatin1()));
    }

    //开启数据库事物
    static void transaction(const QString &connectionName)
    {
        QSqlDatabase::database(connectionName).transaction();
    }

    //提交
    static void commit(const QString &connectionName)
    {
        if (!QSqlDatabase::database(connectionName).commit()) {
            QSqlDatabase::database(connectionName).rollback();
        }
    }

    static void print(const QString &msg)
    {
        QString str = QString("时间[%1] 设备[%2] >> %3").arg(TIMES).arg(CommonSetting::GetLocalHostIP()).arg(msg);

        qDebug() << str;
    }

    static QString CreateUUID()
    {
        //生成UUID
        QString strId = QUuid::createUuid().toString();
        //"{b5eddbaf-984f-418e-88eb-cf0b8ff3e775}"

        QString UUID = strId.remove("{").remove("}").remove("-");
        // "b5eddbaf984f418e88ebcf0b8ff3e775"

        return UUID;
    }
};

#endif // COMMONSETTING_H
