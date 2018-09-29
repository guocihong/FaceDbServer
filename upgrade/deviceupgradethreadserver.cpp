#include "deviceupgradethreadserver.h"

UpgradeServerApi *UpgradeServerApi::instance = NULL;

UpgradeServerApi::UpgradeServerApi(QObject *parent) : QTcpServer(parent)
{

}

void UpgradeServerApi::Listen(void)
{
#if (QT_VERSION >= QT_VERSION_CHECK(5,0,0))
    this->listen(QHostAddress::AnyIPv4, GlobalConfig::UpgradePort);
#else
    this->listen(QHostAddress::Any, GlobalConfig::UpgradePort);
#endif

    qDebug() << "Listen current thread id = " << QThread::currentThreadId();
}

#if (QT_VERSION >= QT_VERSION_CHECK(5,0,0))
void UpgradeServerApi::incomingConnection(qintptr socketDescriptor)
#else
void UpgradeServerApi::incomingConnection(int socketDescriptor)
#endif
{
    DeviceUpgradeThreadServer *work_thread = new DeviceUpgradeThreadServer(socketDescriptor,this);
    connect(work_thread,SIGNAL(finished()),work_thread,SLOT(deleteLater()));
    work_thread->start();
    handler.add(work_thread);

    qDebug() << "incomingConnection current thread id = " << QThread::currentThreadId();
}

#if (QT_VERSION >= QT_VERSION_CHECK(5,0,0))
DeviceUpgradeThreadServer::DeviceUpgradeThreadServer(qintptr socketDescriptor, QObject *parent) : QThread(parent)
#else
DeviceUpgradeThreadServer::DeviceUpgradeThreadServer(int socketDescriptor, QObject *parent) : QThread(parent)
#endif
{
    this->socketDescriptor = socketDescriptor;

    qDebug() << "DeviceUpgradeThreadServer current thread id = " << QThread::currentThreadId();
}

void DeviceUpgradeThreadServer::run()
{
    qDebug() << "run current thread id = " << QThread::currentThreadId();

    //注意事项,这里不能传入this指针做为parent,因为this是由主线程创建的
    //而client是由子线程创建的,qt里面不支持跨线程设置parent
    //对象由哪个线程创建的,那么这个对象的槽函数就由该线程来执行
    UpgradeClientApi *client = new UpgradeClientApi(socketDescriptor);//正确写法

//    UpgradeClientApi *client = new UpgradeClientApi(socketDescriptor,this);//错误写法

    connect(client,SIGNAL(signalDisconnect()),this,SLOT(quit()));//调用quit,通知线程退出事件循环

    exec();

    qDebug() << "work thread quit";
}

#if (QT_VERSION >= QT_VERSION_CHECK(5,0,0))
UpgradeClientApi::UpgradeClientApi(qintptr socketDescriptor, QObject *parent) : QTcpSocket(parent)
#else
UpgradeClientApi::UpgradeClientApi(int socketDescriptor, QObject *parent) : QTcpSocket(parent)
#endif
{
    this->setSocketDescriptor(socketDescriptor);

    connect(this,SIGNAL(readyRead()),this,SLOT(slotRecvMsg()));
    connect(this,SIGNAL(disconnected()),this,SLOT(slotDisconnect()));

    nextBlockSize = 0;
    isWritable = false;

    ParseMsgTimer = new QTimer(this);
    ParseMsgTimer->setInterval(100);
    connect(ParseMsgTimer,SIGNAL(timeout()),this,SLOT(slotParseMsg()));
    ParseMsgTimer->start();

    qDebug() << "UpgradeClientApi current thread id = " << QThread::currentThreadId();
}

void UpgradeClientApi::ack(QString msg)
{
    QByteArray buffer;
    QDataStream out(&buffer,QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_4_7);

    buffer.clear();
    out.device()->seek(0);
    out << qint64(0) << msg.toUtf8();
    out.device()->seek(0);
    out << qint64(buffer.size() - sizeof(qint64));
    this->write(buffer);

    //采用同步发送
    this->waitForBytesWritten();

    //不管发送成功与否,统一断开tcp连接
    this->disconnectFromHost();
}

void UpgradeClientApi::slotRecvMsg()
{
    qDebug() << "slotRecvMsg current thread id = " << QThread::currentThreadId();

    if (this->bytesAvailable() <= 0) {
        return;
    }

    RecvMsgBuffer.append(this->readAll());
}

void UpgradeClientApi::slotDisconnect()
{
    qDebug() << "slotDisconnect current thread id = " << QThread::currentThreadId();

    emit signalDisconnect();

    this->deleteLater();
}

void UpgradeClientApi::slotParseMsg()
{
    QDataStream in(&RecvMsgBuffer,QIODevice::ReadOnly);
    in.setVersion(QDataStream::Qt_4_7);

    if (nextBlockSize == 0) {
        if (RecvMsgBuffer.size() < sizeof(qint64)) {
            return;
        }

        in >> nextBlockSize;
    }

    if (RecvMsgBuffer.size() < (nextBlockSize + sizeof(qint64))) {
        return;
    }

    qint64 key;
    QByteArray data;
    in >> key >> data;

    RecvMsgBuffer = RecvMsgBuffer.mid(nextBlockSize + sizeof(qint64));

    nextBlockSize = 0;

    switch (key) {
    case 0x01://传输开始标识符
        fileName = QString::fromUtf8(data);
//        file.setFileName(QString("%1/%2").arg(QCoreApplication::applicationDirPath()).arg(fileName));

        file.setFileName(QString("/tmp/%1").arg(fileName));

        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            isWritable = true;
        } else {
            isWritable = false;
        }

        qDebug() << "fileName = " << fileName;

        break;

    case 0x02://传输文件内容标识符
        if (isWritable) {
            file.write(data);
            file.flush();
        }

        break;

    case 0x03://传输结束标识符
        if (isWritable) {
            file.close();

#ifdef Q_OS_LINUX
            qDebug() << "=========================开始解压=======================";

            //同步，本线程会阻塞,但是不会阻塞其他线程
            //system(QString("tar xzvf /UpgradePackage.tar.gz -C / > /dev/null 2>&1").toLatin1().data());

            //异步，不会阻塞，会立即返回
            QProcess process;
//            process.start(QString("tar xzvf %1/%2 -C /").arg(QCoreApplication::applicationDirPath()) .arg(fileName));

            process.start(QString("tar xzvf /tmp/%1 -C /").arg(fileName));

            while (1) {
                if (process.waitForFinished(10)) {//这里的时间最好不要太长，不然会阻塞等待
                    break;
                }

                qDebug() << "=========================正在解压ing=======================";

                qApp->processEvents();//这里只会处理本线程相关的事件,不会处理其他线程的事件
            }

            qDebug() << "=========================解压完成=======================";
#endif

            //升级成功,发送反馈信号
            //必须要等到解压完成以后,才发送反馈信号,不然解压过程中如果客户端断开连接,那么本线程会退出,导致解压失败
            ack("succeed");

            system("reboot");
        } else {
            //升级失败,发送反馈信号
            ack("failed");
        }

        break;
    }
}
