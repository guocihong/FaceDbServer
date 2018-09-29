#include "deviceupgradethreadclient.h"

#define PER_PACKET_SIZE  (qint64)65536  //64KB

DeviceUpgradeThreadClient::DeviceUpgradeThreadClient(QString FilePath, QString ServerIP, quint16 ServerPort, QProgressBar *ProgressBar, QObject *parent) : QThread(parent)
{
    this->FilePath = FilePath;
    this->ServerIP = ServerIP;
    this->ServerPort = ServerPort;
    this->ProgressBar = ProgressBar;

    qDebug() << "DeviceUpgradeThreadClient current thread id = " << QThread::currentThreadId();
}

void DeviceUpgradeThreadClient::run()
{
    qDebug() << "run current thread id = " << QThread::currentThreadId();

    DeviceUpgradeSocketClient *client = new DeviceUpgradeSocketClient(FilePath,ServerIP,ServerPort);
    connect(client,SIGNAL(error()),this,SLOT(slotSendError()));
    connect(client,SIGNAL(finish()),this,SLOT(quit()));
    connect(client,SIGNAL(setProgressBarMaximum(qint64)),this,SLOT(slotSetProgressBarMaximum(qint64)));
    connect(client,SIGNAL(updateProgressBar(qint64)),this,SLOT(slotUpdateProgressBar(qint64)));

    exec();

    qDebug() << "thread quit";
}

void DeviceUpgradeThreadClient::slotSendError()
{
    qDebug() << "slotSendError current thread id = " << QThread::currentThreadId();

    //设置进度条为红色，醒目显示
    QString style = QString("QProgressBar{border:1px solid red;background:red;color:white;}"
                          "QProgressBar::chunk{background:red;}");
    ProgressBar->setStyleSheet(style);

    //通知子线程退出事件循环
    this->quit();
}

void DeviceUpgradeThreadClient::slotSetProgressBarMaximum(qint64 TotalBytesToWritten)
{
    qDebug() << "slotSetProgressBarMaximum current thread id = " << QThread::currentThreadId();

    QString style = QString("QProgressBar{border:1px solid green;color:white;}"
                          "QProgressBar::chunk{background:green;}");

    ProgressBar->setStyleSheet(style);

    ProgressBar->setMaximum(TotalBytesToWritten + 1);//这里之所以加1,是为了等待前端设备解压完成后才显示100%的进度
}

void DeviceUpgradeThreadClient::slotUpdateProgressBar(qint64 BytesWritten)
{
    qDebug() << "slotUpdateProgressBar current thread id = " << QThread::currentThreadId();

    ProgressBar->setValue(BytesWritten);
}

DeviceUpgradeSocketClient::DeviceUpgradeSocketClient(QString FilePath, QString ServerIP, quint16 ServerPort, QObject *parent) :
    QTcpSocket(parent)
{
    qDebug() << "DeviceUpgradeSocketClient current thread id = " << QThread::currentThreadId();

    this->FilePath = FilePath;
    this->ServerIP = ServerIP;
    this->ServerPort = ServerPort;

    this->TotalBytesToWritten = 0;
    this->BytesWritten = 0;
    this->BytesToWrite = 0;

    this->flag = DeviceUpgradeSocketClient::SendFailed;

    connect(this,SIGNAL(connected()),this,SLOT(slotStartTransfers()));
    connect(this,SIGNAL(bytesWritten(qint64)),this,SLOT(slotContinueTransfers(qint64)));
    connect(this,SIGNAL(readyRead()),this,SLOT(slotRecvMsg()));
    connect(this,SIGNAL(disconnected()),this,SLOT(slotDisconnect()));
    connect(this,SIGNAL(error(QAbstractSocket::SocketError)),this,SLOT(slotDisplayError(QAbstractSocket::SocketError)));

    this->connectToHost(ServerIP,ServerPort);
}

void DeviceUpgradeSocketClient::slotStartTransfers()
{
    qDebug() << "slotStartTransfers current thread id = " << QThread::currentThreadId();

    file = new QFile(FilePath);
    if (!file->open(QFile::ReadOnly)) {
        qDebug() << "升级文件打开失败";
        emit error();
        return;
    }

    QString fileName = QFileInfo(FilePath).fileName();
    qint64 fileSize = file->size();
    this->TotalBytesToWritten = fileSize;
    this->BytesToWrite = fileSize;
    emit setProgressBarMaximum(fileSize);

    QByteArray buffer;
    QDataStream out(&buffer, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_4_7);//这里设置最低版本，这样就可以兼容之前所有程序

    //发送开始标识符及文件名称
    buffer.clear();
    out.device()->seek(0);
    out << qint64(0) << qint64(DeviceUpgradeSocketClient::SendStartIdentifier) << fileName.toUtf8();
    out.device()->seek(0);
    out << qint64(buffer.size() - sizeof(qint64));
    this->write(buffer);

    qDebug() << "发送开始标识符及文件名称";
}

void DeviceUpgradeSocketClient::slotContinueTransfers(qint64 bytes)
{
    qDebug() << "slotContinueTransfers current thread id = " << QThread::currentThreadId();

    QByteArray buffer;
    QDataStream out(&buffer, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_4_7);//这里设置最低版本，这样就可以兼容之前所有程序

    if (BytesToWrite > 0) {//发送文件内容
        buffer.clear();
        out.device()->seek(0);

        qint64 size = qMin(PER_PACKET_SIZE,BytesToWrite);
        out << qint64(0) << qint64(DeviceUpgradeSocketClient::SendFileContentIdentifier) << file->read(size);
        out.device()->seek(0);
        out << qint64(buffer.size() - sizeof(qint64));
        this->write(buffer);
        BytesWritten += size;
        BytesToWrite = TotalBytesToWritten - BytesWritten;

        //更新进度条
        emit updateProgressBar(BytesWritten);

        qDebug() << "发送文件内容";
    } else if (BytesToWrite == 0) {//发送结束标识符
        file->close();

        //发送结束标识符
        buffer.clear();
        out.device()->seek(0);
        out << qint64(0) << qint64(DeviceUpgradeSocketClient::SendEndIdentifier) << QFileInfo(FilePath).fileName().toUtf8();
        out.device()->seek(0);
        out << qint64(buffer.size() - sizeof(qint64));
        this->write(buffer);

        qDebug() << "发送结束标识符";

        //这里需要置为-1,不然会一直发送结束标识符
        BytesToWrite = -1;
    }
}

void DeviceUpgradeSocketClient::slotRecvMsg()
{
    qDebug() << "slotRecvMsg current thread id = " << QThread::currentThreadId();

    QDataStream in(this);
    in.setVersion(QDataStream::Qt_4_7);

    static qint64 nextBlockSize = 0;

    if (nextBlockSize == 0) {
        if (this->bytesAvailable() < sizeof(qint64)) {
            return;
        }

        in >> nextBlockSize;
    }

    if (this->bytesAvailable() < nextBlockSize) {
        return;
    }

    nextBlockSize = 0;//这里必须重新置为0,不然下一次升级会收不到数据

    QByteArray data;
    in >> data;

    if (data == "succeed") {
        flag = DeviceUpgradeSocketClient::SendSucceed;

        //更新为100%的进度
        emit updateProgressBar(TotalBytesToWritten + 1);

        emit finish();

        qDebug() << "升级成功";
    } else if (data == "failed") {
        flag = DeviceUpgradeSocketClient::SendFailed;
        emit error();

        qDebug() << "升级失败";
    }

    this->deleteLater();
}

void DeviceUpgradeSocketClient::slotDisconnect()
{
    qDebug() << "slotDisconnect current thread id = " << QThread::currentThreadId();

    qDebug() << "DeviceUpgradeSocketClient::slotDisconnect";

    if (flag == DeviceUpgradeSocketClient::SendFailed) {//发送过程中出错
        emit error();

        qDebug() << "升级失败";
    } else if (flag == DeviceUpgradeSocketClient::SendSucceed) {//发送成功,服务器主动断开连接,属于正常情况
        //do nothing
    }

    this->deleteLater();
}

void DeviceUpgradeSocketClient::slotDisplayError(QAbstractSocket::SocketError socketError)
{
    qDebug() << "slotDisplayError current thread id = " << QThread::currentThreadId();

    switch(socketError){
    case QAbstractSocket::ConnectionRefusedError:
        qDebug() << "QAbstractSocket::ConnectionRefusedError";
        break;
    case QAbstractSocket::RemoteHostClosedError:
        qDebug() << "QAbstractSocket::RemoteHostClosedError";
        break;
    case QAbstractSocket::HostNotFoundError:
        qDebug() << "QAbstractSocket::HostNotFoundError";
        break;
    }

    if (flag == DeviceUpgradeSocketClient::SendFailed) {//没有连接成功或者发送过程中出错
        emit error();
    } else if (flag == DeviceUpgradeSocketClient::SendSucceed) {//发送成功,服务器主动断开连接,属于正常情况
        //do nothing
    }

    this->deleteLater();
}
