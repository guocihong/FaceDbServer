#include "loghelper.h"

LogHelper *LogHelper::instance = NULL;
QList<QTcpSocket *> LogHelper::ClientBuffer = QList<QTcpSocket *>();

#if (QT_VERSION >= QT_VERSION_CHECK(5,0,0))
void LogCatcher(QtMsgType type, const QMessageLogContext &context, const QString &msg)
#else
void LogCatcher(QtMsgType type, const char *msg)
#endif
{
    //加锁,防止多线程中qdebug太频繁导致崩溃
    static QMutex mutex;
    QMutexLocker locker(&mutex);

    LogHelper::newInstance()->SaveLog(msg);
}

LogHelper::LogHelper(QObject *parent) :
    QObject(parent)
{
    ConnectionListener = new QTcpServer(this);
    connect(ConnectionListener,SIGNAL(newConnection()),this,SLOT(slotProcessNewConnection()));

#if (QT_VERSION >= QT_VERSION_CHECK(5,0,0))
    ConnectionListener->listen(QHostAddress::AnyIPv4,GlobalConfig::LogPort);
#else
    ConnectionListener->listen(QHostAddress::Any,GlobalConfig::LogPort);
#endif

    //必须用信号槽形式,不然提示 QSocketNotifier: Socket notifiers cannot be enabled or disabled from another thread
    //估计日志钩子可能单独开了线程
    connect(this,SIGNAL(signalSendLog(QString)),this,SLOT(slotSendLog(QString)));
}

//安装日志钩子,通过网络输出调试信息,便于调试
void LogHelper::start()
{
#if (QT_VERSION >= QT_VERSION_CHECK(5,0,0))
    qInstallMessageHandler(LogCatcher);
#else
    qInstallMsgHandler(LogCatcher);
#endif
}

//卸载日志钩子
void LogHelper::stop()
{
#if (QT_VERSION >= QT_VERSION_CHECK(5,0,0))
    qInstallMessageHandler(0);
#else
    qInstallMsgHandler(0);
#endif
}

void LogHelper::SaveLog(const QString &msg)
{
    emit signalSendLog(msg);
}

void LogHelper::slotProcessNewConnection()
{
    QTcpSocket *client = ConnectionListener->nextPendingConnection();
    connect(client,SIGNAL(disconnected()),this,SLOT(slotDisconnect()));
    LogHelper::ClientBuffer.append(client);
}

void LogHelper::slotDisconnect()
{
    QTcpSocket *client = (QTcpSocket *)sender();

    int size = LogHelper::ClientBuffer.size();
    for (int i = 0; i < size; i++) {
        if (LogHelper::ClientBuffer.at(i) == client) {
            client->deleteLater();
            LogHelper::ClientBuffer.removeAt(i);
            break;//不加break，会越界，导致段错误
        }
    }
}

void LogHelper::slotSendLog(const QString &msg)
{
    foreach (QTcpSocket *client, LogHelper::ClientBuffer) {
       client->write(msg.toUtf8());//需要转换成utf8,不然接收时会乱码
    }
}
