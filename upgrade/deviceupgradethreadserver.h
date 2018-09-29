#ifndef DEVICEUPGRADETHREADSERVER_H
#define DEVICEUPGRADETHREADSERVER_H

#include "globalconfig.h"

class UpgradeServerApi : public QTcpServer
{
    Q_OBJECT
public:
    explicit UpgradeServerApi(QObject *parent = 0);

    static UpgradeServerApi *newInstance() {
        static QMutex mutex;

        if (!instance) {
            QMutexLocker locker(&mutex);

            if (!instance) {
                instance = new UpgradeServerApi();
            }
        }

        return instance;
    }

    void Listen();

protected:
#if (QT_VERSION >= QT_VERSION_CHECK(5,0,0))
    void incomingConnection(qintptr socketDescriptor);
#else
    void incomingConnection(int socketDescriptor);
#endif

private:
    static UpgradeServerApi *instance;
    QObjectCleanupHandler handler;
};

class DeviceUpgradeThreadServer : public QThread
{
    Q_OBJECT
public:
#if (QT_VERSION >= QT_VERSION_CHECK(5,0,0))
    explicit DeviceUpgradeThreadServer(qintptr socketDescriptor, QObject *parent = 0);
#else
    explicit DeviceUpgradeThreadServer(int socketDescriptor, QObject *parent = 0);
#endif

protected:
    void run();

private:
#if (QT_VERSION >= QT_VERSION_CHECK(5,0,0))
    qintptr socketDescriptor;
#else
    int socketDescriptor;
#endif
};

class UpgradeClientApi : public QTcpSocket
{
    Q_OBJECT
public:
#if (QT_VERSION >= QT_VERSION_CHECK(5,0,0))
    explicit UpgradeClientApi(qintptr socketDescriptor, QObject *parent = 0);
#else
    explicit UpgradeClientApi(int socketDescriptor, QObject *parent = 0);
#endif

    void ack(QString msg);

private slots:
    void slotRecvMsg();
    void slotDisconnect();
    void slotParseMsg();

signals:
    void signalDisconnect();

private:
    qint64 nextBlockSize;
    QString fileName;
    QFile file;
    bool isWritable;

    QTimer *ParseMsgTimer;
    QByteArray RecvMsgBuffer;
};

#endif // DEVICEUPGRADETHREADSERVER_H
