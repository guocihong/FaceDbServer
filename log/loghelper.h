#ifndef LOGHELPER_H
#define LOGHELPER_H

#include "globalconfig.h"

class LogHelper : public QObject
{
    Q_OBJECT
public:
    explicit LogHelper(QObject *parent = 0);

    static LogHelper *newInstance() {
        static QMutex mutex;

        if (!instance) {
            QMutexLocker locker(&mutex);

            if (!instance) {
                instance = new LogHelper();
            }
        }

        return instance;
    }

    void start();
    void stop();
    void SaveLog(const QString &msg);

signals:
    void signalSendLog(const QString &msg);

public slots:
    void slotProcessNewConnection();
    void slotDisconnect();

    void slotSendLog(const QString &msg);

public:
    static LogHelper *instance;

    QTcpServer *ConnectionListener;
    static QList<QTcpSocket *> ClientBuffer;//保存所有客户端连接对象
};
#endif // LOGHELPER_H
